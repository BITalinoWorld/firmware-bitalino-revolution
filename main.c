/**
 * \file
 * \copyright  Copyright 2014-2016 PLUX - Wireless Biosignals, S.A.
 * \author     Filipe Silva
 * \version    1.1
 * \date       July 2016
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


// Global program memory variables
const char PROGMEM versionStr[] = "BITalino_v5.2\n";

// Global variables
word batThres = BATTHRES_BASE; // default battery threshold of 3.4 V
word head = 0, tail = 0;
byte ledIncr = LEDINCR_SLOW;
byte seq, simulSeq, nChannels, chTable[6];
bool btStatMode = false, simulated;


// Local EEPROM variables
static byte EEMEM ee_tested;


int main(void)
{    
   // Assign GPIO pull-ups to digital inputs and high level to output /SS
   PORTB = BIT_I1 | BIT_I2 | BIT_SS | BIT_MISO;
   PORTC = BIT_CTS;
   PORTD = BIT_INT;
   
   // Assign GPIO outputs
   DDRB = BIT_SS | BIT_MOSI | BIT_SCK;
   DDRD = BIT_O1 | BIT_O2 | BIT_PWM | BIT_LED_STAT | BIT_LED_BAT;
   
   // ADC setup
   DIDR0 = BIT_A1 | BIT_A2 | BIT_A3 | BIT_A4 | BIT_A5;   // Digital Input Disable in analog inputs A1-A5 (A6 and ABAT don't need this)
   
   ACSR = B(ACD);    // Analog Comparator Disable
   
   // run test and configure procedure if not done so since last memory programming
   eeprom_busy_wait();
   if (eeprom_read_byte(&ee_tested) != EE_BT_PROG_VAL)
   {
      eeprom_busy_wait();
      eeprom_write_byte(&ee_tested, EE_BT_PROG_VAL);
      
      configureAndTest();
   }
   
   // USART setup
   UBRR0 = 12;                                // = [F_CPU / (8*baud)] - 1  with U2X0 = 1  (115.2kbps @ 12MHz)
   UCSR0A = B(U2X0);
   UCSR0B = B(RXCIE0) | B(RXEN0) | B(TXEN0); // Enable RX, RX interrupt and TX and keep default 8n1 serial data format

   // Timer 0 setup for status LED PWM and output PWM
   TCCR0A = B(COM0A1) | B(COM0B1) | B(WGM01) | B(WGM00); // Non-inverting Fast PWM mode on OC0A and OC0B
   TCCR0B = B(CS00);                                     // Start timer with no prescaling
   
   // Timer 1 setup for sampling timing
   OCR1A = 1500-1;               // default value for 1000 Hz @ 12 MHz
   TIMSK1 = B(OCIE1A);           // Enable compare match interrupt
   
   // Timer 2 setup for status LED update
   OCR2A = 255;                           // Interrupt frequency = F_CPU / 1024 / 256 = 45.8 Hz @ 12 MHz
   TCCR2A = B(WGM21);                     // CTC mode
   TCCR2B = B(CS22) | B(CS21) | B(CS20);  // Start timer with 1/1024 prescaling
   TIMSK2 = B(OCIE2A);                    // Enable compare match interrupt
   
   if (!btStatMode)  // btStatMode is true if configureAndTest() was called
      assignBtStatMode();
   
   // Pin change interrupt setup
   if (!btStatMode)     PCMSK1 = B(PCINT8);  // pin change interrupt at PC0 (/CTS), still disabled (PCICR==0) since TX fifo is empty
   
   // Sleep Mode setup
   SMCR = B(SE);        // Idle Mode and Sleep Enabled
   
   // Stop unused modules
   PRR = B(PRTWI) | B(PRSPI);   // keep timers, USART and ADC running
   
   sei();   // Enable interrupts
   
	while(1)
      __asm volatile("sleep");

	return 0;
}
