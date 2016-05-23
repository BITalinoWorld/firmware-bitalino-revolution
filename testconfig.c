/**
 * \file
 * \copyright  Copyright 2014-2016 PLUX - Wireless Biosignals, S.A.
 * \author     Filipe Silva
 * \version    1.0
 * \date       April 2016
 *
 * \section LICENSE
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 
 */


#include "main.h"

#include <util/delay.h>


static const char PROGMEM cmdName[] = "AT+NAMEBITalino";
static const char PROGMEM cmdBaud[] = "AT+BAUD8";


#pragma pack(1)

struct TestData
{
   byte init,
        // Rest values
        vBat, dv1_1, av1_1, vRef, emgRef_2, ecgRef_2, eegRef_2, // voltage levels
        ldrSta00, ldrBat00, ldrLedOff, pwmOff,                  // outputs
        accXoff, accYoff, accZoff, luxOff,                      // inputs
   
        // Stimuli on outputs
        ldrSta10, ldrBat10,      // status LED on
        ldrSta01, ldrBat01,      // battery LED on
        ldrLedOn,                // O1 LED on
        pwmOn,                   // PWM on
   
        // Stimuli on inputs
        inputs0, inputs1,        // digital inputs
        edaShort,                // EDA at short circuit (100 Ohm)
        //edaOn,                   // EDA on
        accXon, accYon, accZon;  // ACC ST on
        //luxOn,                   // LUX on
        //emgMean, emgAmpl,        // EMG on
        //ecgMean, ecgAmpl,        // ECG on
        //eegMean, eegAmpl;        // EEG on
};

#pragma pack()



static uint8_t sampleADC(uint8_t ch)
{
   ADMUX = B(REFS0) | B(ADLAR) | ch;         // ADC Reference is AVcc; left adjusted result; select analog input
   ADCSRA |= B(ADSC);
   loop_until_bit_is_clear(ADCSRA, ADSC);
   return ADCH;
}

static byte recvSPI(void)
{
   _delay_us(100);   // give time to ADC conversion on test system (13 cycles / 187.5 kHz = 69 us)
   
   SPDR = 0;
   loop_until_bit_is_set(SPSR, SPIF);
   return SPDR;
}

void configureAndTest(void)
{
   struct TestData testData;
   char            btName[20];
   
   
// Configure Bluetooth module
   
   SET_LED_STAT;
   
   // USART setup at 9600 baud (default in Bluetooth module)
   UBRR0 = 77;        // = [F_CPU / (16*baud)] - 1  (9600 bps @ 12MHz)
   UCSR0B = B(TXEN0); // Enable TX and keep default 8n1 serial data format
   
   sei();   // To enable UDRE interrupt to send UART data
   btStatMode = true;
   
   _delay_ms(1000); // Bluetooth module startup delay
   sendProgmemStr(cmdName);
   
   _delay_ms(1000); // Bluetooth module delay between commands
   sendProgmemStr(cmdBaud);
   
   CLR_LED_STAT;  // give time for the LDRs on test system to stabilize
   
   _delay_ms(1000); // Bluetooth module delay between commands
   
   cli();
   
   
// System test (with test system)
   
   // ADC setup
   ADCSRA = B(ADEN) | B(ADPS2) | B(ADPS1);   // Enable ADC with 1/64 prescaler - fAD = 12e6/64 = 187.5 kHz @ 12MHz (must be < 200 kHz)
   
   // SPI module setup to communicate with test system
   SPCR = B(SPE) | B(MSTR) | B(SPR0); // Enable SPI master mode, SCK freq = fosc/8 = 1.5 MHz @ 12 MHz (must be < 12 MHz / 4)
   SPSR = B(SPI2X);
   
   testData.inputs0 = ISSET_I2 | ISSET_INT;  // should be low
   testData.inputs1 = ISSET_I1;  // should be high (button not pressed)
   
   CLR_SS;   // set /SS output low
   
   // initiate communication with test system
   SPDR = 0x34;
   loop_until_bit_is_set(SPSR, SPIF);
   testData.init = SPDR;
   
   // Rest values
   
   //  voltage levels
   //_delay_us(10);  // give time to test system set I2
   testData.vBat = sampleADC(BITPOS_ABAT);   // this will also give time to test system set I2
   sampleADC(0x0E); // pre-select bandgap voltage reference to allow it to stabilize
   testData.inputs1 |= ISSET_I2; // should be high
   
   testData.dv1_1 = recvSPI();          // 1.1 V internal reference on test system (powered by DVcc)
   testData.av1_1 = sampleADC(0x0E);    // 1.1 V internal reference; also give time to test system set /INT

   //_delay_us(10);  // give time to test system set /INT
   testData.inputs1 |= ISSET_INT; // should be high
   
   testData.vRef     = recvSPI();
   testData.emgRef_2 = recvSPI();
   testData.ecgRef_2 = recvSPI();
   testData.eegRef_2 = recvSPI();
   
   //  outputs
   testData.ldrSta00  = recvSPI();
   testData.ldrBat00  = recvSPI();
   testData.ldrLedOff = recvSPI();
   testData.pwmOff    = recvSPI();
   
   //  inputs
   testData.edaShort = sampleADC(BITPOS_A3);   // EDA
   testData.accXoff  = recvSPI();
   testData.accYoff  = recvSPI();
   testData.accZoff  = sampleADC(BITPOS_A5);   // ACC
   testData.luxOff   = sampleADC(BITPOS_A6);   // LUX
   
   // Start PWM stimulus: Timer 0 setup for PWM output
   OCR0B = 0xC0;                             // set PWM duty cycle as 3/4
   TCCR0A = B(COM0B1) | B(WGM01) | B(WGM00); // Non-inverting Fast PWM mode on OC0B
   TCCR0B = B(CS00);                         // Start timer with no prescaling
   
   // Status LED on
   SET_LED_STAT;
   _delay_ms(110); // give time to test system reply
   testData.ldrSta10 = recvSPI();
   testData.ldrBat10 = recvSPI();
   CLR_LED_STAT;
   
   // Battery LED on
   SET_LED_BAT;
   _delay_ms(110); // give time to test system reply
   testData.ldrSta01 = recvSPI();
   testData.ldrBat01 = recvSPI();
   CLR_LED_BAT;
   
   // O1 LED on
   SET_O1;
   _delay_ms(110); // give time to test system reply
   testData.ldrLedOn = recvSPI();
   CLR_O1;
   
   // Button pressed
   _delay_ms(110); // give time to test system press button
   testData.inputs0 |= ISSET_I1;  // should be low (button pressed)
   
   // ACC ST on
   testData.accXon = recvSPI();
   testData.accYon = recvSPI();
   testData.accZon = sampleADC(BITPOS_A5);   // ACC

   // LUX on
   //_delay_ms(100); // wait 100 ms for the LDRs to stabilize
   //testData.luxOn = sampleADC(BITPOS_A6);   // LUX
   
   // PWM on (~400 ms have elapsed since start of PWM stimulus)
   //_delay_ms(10); // give time to test system reply
   testData.pwmOn = recvSPI();
   OCR0B = 0;
   
   // EDA on
   //testData.edaOn = sampleADC(BITPOS_A3);   // EDA
   
   ADCSRA = B(ADIF);    // Disable ADC and clear pending interrupt flag
   SPCR = 0;   // Disable SPI
   
   SET_SS;   // set /SS output high
   
   
// Send test data
   
   UBRR0 = 12;                                // = [F_CPU / (8*baud)] - 1  with U2X0 = 1  (115.2kbps @ 12MHz)
   UCSR0A = B(U2X0);
   UCSR0B = B(RXEN0) | B(TXEN0); // Enable RX and TX and keep default 8n1 serial data format
   
   loop_until_bit_is_set(UCSR0A, RXC0);   // wait for incoming byte (and ignore it)
   
   for(byte i = 0; i < sizeof testData; i++)
   {
      loop_until_bit_is_set(UCSR0A, UDRE0);
      UDR0 = ((byte *)&testData)[i];
   }
   
// Receive Set Bluetooth name command
   
   do
   {
      loop_until_bit_is_set(UCSR0A, RXC0);   // wait for incoming byte
   } while (UDR0 != 0x63); // loop until Set Bluetooth name command is received
   
   for(byte i = 0; i < sizeof btName; i++)
   {
      loop_until_bit_is_set(UCSR0A, RXC0);   // wait for incoming byte
      btName[i] = UDR0;
      if (btName[i] == 0)  break;
   }
   
   // send command reply
   loop_until_bit_is_set(UCSR0A, UDRE0);
   UDR0 = 0x36;
   
// Change Bluetooth name
   
   // wait for Bluetooth connection drop
   while (ISSET_CTS) ;
   
   _delay_ms(100); // Add a delay between connection drop and command
   
   // send "AT+NAME"
   for(byte i = 0; i < 7; i++)
   {
      loop_until_bit_is_set(UCSR0A, UDRE0);
      UDR0 = pgm_read_byte(cmdName+i);
   }
   
   for(byte i = 0; i < sizeof btName; i++)
   {
      if (btName[i] == 0)  break;
      
      loop_until_bit_is_set(UCSR0A, UDRE0);
      UDR0 = btName[i];
   }
   
   _delay_ms(1000); // Bluetooth module delay between commands
   
   //UCSR0A = B(TXC0) | B(U2X0);   // clear TXC0
   //loop_until_bit_is_set(UCSR0A, TXC0);   // wait for end of transmission
   
   UCSR0B = 0; // flush RX buffer
}

void assignBtStatMode(void)
{
   // Detect Bluetooth module presence to assign btStatMode
   
   OCR0A = 100;   // turn status led on during this procedure
   
   // Send AT command
   _delay_ms(750); // Bluetooth module startup delay
   
   UDR0 = 'A';
   loop_until_bit_is_set(UCSR0A, UDRE0);
   UDR0 = 'T';

   // Check for OK answer
   _delay_ms(750); // Bluetooth module command delay
   
   if (UCSR0A & B(RXC0))
      if (UDR0 == 'O')
         if (UCSR0A & B(RXC0))
            if (UDR0 == 'K')
               btStatMode = true;
}
