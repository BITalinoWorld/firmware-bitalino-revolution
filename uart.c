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


static const byte PROGMEM crcTable[16] = {0, 3, 6, 5, 12, 15, 10, 9, 11, 8, 13, 14, 7, 4, 1, 2};


static byte txbuff[TXBUFSIZ];

static byte TCCR1B_new = B(WGM12) | B(CS11);   // default value for 1000 Hz @ 12 MHz

static bool pendingPWMval = false;

void sendStatus(void);  // cannot be static because it needs a stack frame (called from a naked ISR)


ISR(USART_RX_vect, ISR_NAKED)  // Receive UART Data
{
   byte cmd = UDR0;
   
   if (pendingPWMval)
   {
      pendingPWMval = false;
      OCR0B = cmd;   // set PWM output duty cycle
      
      reti();
   }
   
   // all-mode commands (BITalino 2 only)
   if ((cmd & 0xA3) == 0xA3 && cmd != 0xFF) // 1 X 1 X X X 1 1  (0xFF is handled later)
   {
      if (cmd == 0xA3)  // Set analog output
         pendingPWMval = true;
      else if ((cmd & 0xF3) == 0xB3)   // Set digital outputs
         PORTD = (PORTD & ~(BIT_O1 | BIT_O2)) | ((cmd << 1) & (BIT_O1 | BIT_O2));   // update O1 and O2
      
      reti();
   }
   
   if (TCCR1B == 0)  // idle mode
      switch (cmd & 3)
      {
         case 0:  // Battery threshold definition
            batThres = BATTHRES_BASE + (cmd >> 2);  // cmd arg = 0 -> 527 = 3.4 V ; cmd arg = 63 -> 527+63 = 590 = 3.8 V
            CLR_LED_BAT;   // battery LED may turn off with new threshold value
            break;
            
         case 1: // Start live mode
         case 2: // Start simulated mode
            if ((cmd & 3) == 2) // simulated mode
            {
               simulated = true;
               simulSeq = 0;
            }
            else
               simulated = false;
            
            seq = 0;
            nChannels = 0;
            for(byte i = 0; i < 6; i++)
            {
               if (cmd & 0x04)
                  chTable[nChannels++] = simulated ? i : (i + BITPOS_A1);
               cmd >>= 1;
            }
            TCNT1 = 0;           // reset timer 1 counter
            TCCR1B = TCCR1B_new; // start timer 1
            ledIncr = LEDINCR_FAST;
            OCR0A = 100;
            break;
            
         case 3:
            switch (cmd)
            {
               case 0x07: // Send version string
               case 0x0F:
                  sendProgmemStr(versionStr);
                  break;
               case 0x0B: // Send device status
                  sendStatus();
                  break;
               case 0x03:  // Set sampling rate at 1 Hz
                  OCR1A = 46875-1;  // f = 12e6 / 256 / 46875 = 1 Hz
                  TCCR1B_new = B(WGM12) | B(CS12);  // timer 1 CTC mode with prescaling factor of 256
                  break;
               case 0x43:  // Set sampling rate at 10 Hz
                  OCR1A = 18750-1;  // f = 12e6 / 64 / 18750 = 10 Hz
                  TCCR1B_new = B(WGM12) | B(CS11) | B(CS10);  // timer 1 CTC mode with prescaling factor of 64
                  break;
               case 0x83:  // Set sampling rate at 100 Hz
                  OCR1A = 1875-1;  // f = 12e6 / 64 / 1875 = 100 Hz
                  TCCR1B_new = B(WGM12) | B(CS11) | B(CS10);  // timer 1 CTC mode with prescaling factor of 64
                  break;
               case 0xC3:  // Set sampling rate at 1000 Hz
                  OCR1A = 1500-1;  // f = 12e6 / 8 / 1500 = 1000 Hz
                  TCCR1B_new = B(WGM12) | B(CS11);  // timer 1 CTC mode with prescaling factor of 8
                  break;
               // command 0xFF (stop) is ignored in idle mode
            }
            break;
      }
   else  // live mode
      if (cmd == 0 || cmd == 0xFF)  // Stop live / simulated mode
      {
         TCCR1B = 0;  // stop Timer 1
         ledIncr = LEDINCR_SLOW;
         OCR0A = 100;
      }
      else if ((cmd & 3) == 3)   // Set digital outputs
         PORTD = (PORTD & ~(BIT_O1 | BIT_O2)) | ((cmd << 1) & (BIT_O1 | BIT_O2));   // update O1 and O2
   
   reti();
}


ISR(USART_UDRE_vect)   // Transmit UART Data (non-naked ISR since it can be called while running timer1TickLive() or configureAndTest() )
{
	UDR0 = txbuff[tail++];
	if (tail == TXBUFSIZ)
		tail = 0;
	
	if (head == tail)
   {
		UCSR0B &= ~B(UDRIE0); // disable this interrupt until new data to send
      head = 0;   // reset buffer pointers just to optimize add_tx_data() (call memcpy() only once)
      tail = 0;

      PCICR = 0; // disable /CTS pin change interrupt (if not btStatMode)
   }
}

ISR(PCINT1_vect)   // /CTS pin change (non-naked ISR since it can be called while running timer1TickLive() )
{
   if (head == tail)  return;  // TX buffer is empty, nothing to do
   
   if (ISSET_CTS)
      UCSR0B &= ~B(UDRIE0); // /CTS is inactive, disable UDRE interrupt (if not already disabled)
   else
      UCSR0B |= B(UDRIE0); // /CTS is active, enable UDRE interrupt (if not already enabled)
}

static void add_tx_data(const void *data, byte siz)
{
   // UDRE or global interrupts must be disabled here
   
   word space = TXBUFSIZ - head; //(head+siz) - (txbuff+bufsiz);
   bool tail_ahead = (tail > head);
   if (space >= siz)
   {
      memcpy(txbuff+head, data, siz);
      head += siz;
      if (head == TXBUFSIZ)
      {
         head = 0;
         if (tail == 0)    tail = 1; // lose oldest byte in buffer
      }
      else if (tail_ahead && tail <= head)  // if buffer overflow
      {
         tail = head+1; // lose oldest bytes in buffer
         if (tail == TXBUFSIZ)    tail = 0;
      }
   }
   else
   {
      memcpy(txbuff+head, data, space);
      siz -= space;
      memcpy(txbuff, data+space, siz);
      head = siz;
      if (tail_ahead || tail <= head)  // if buffer overflow
      {
         tail = head+1; // lose oldest bytes in buffer
         if (tail == TXBUFSIZ)    tail = 0;
      }
   }
   
   if (!btStatMode)
   {
      PCIFR = B(PCIF1); // clear interrupt flag
      PCICR = B(PCIE1); // enable /CTS pin change interrupt (if not already enabled)
   }
   
   // if in CTS mode, start sending data only if CTS is active
   // if aquiring at 1000 Hz, send every 2 frames to overcome BT121 module problem (problems with inter-frame period of 1 ms)
   if (btStatMode || (!ISSET_CTS && !(TCCR1B == (B(WGM12) | B(CS11)) && (seq & 1))))
      UCSR0B |= B(UDRIE0); // enable UDRE interrupt (if not already enabled)
}

static byte byteCRC(byte crc, byte b)
{
   crc = pgm_read_byte(crcTable + crc) ^ (b >> 4);
   crc = pgm_read_byte(crcTable + crc) ^ (b & 0x0F);
   
   return crc;
}

static word wordCRC(byte crc, word w)
{
   crc = byteCRC(crc, w);
   crc = byteCRC(crc, w >> 8);
   
   return crc;
}

void sendFrameCRC(byte *frame)
{
   byte startPos = 6 - nChannels;
   if (nChannels >= 3 && nChannels <= 5)  startPos--;
   
   // send frame and calculate CRC (except last byte (seq+CRC) )
   byte crc = 0;
   for(byte i = startPos; i < 7; i++)
   {
      const byte b = frame[i];

      /*
      // send byte
      loop_until_bit_is_set(UCSR0A, UDRE0);
      UDR0 = b;
      */
      // calculate CRC nibble by nibble
      //crc = pgm_read_byte(crcTable + crc) ^ (b >> 4);
      //crc = pgm_read_byte(crcTable + crc) ^ (b & 0x0F);
      crc = byteCRC(crc, b);
   }
   
   // calculate CRC for last byte (seq+CRC)
   crc = pgm_read_byte(crcTable + crc) ^ (seq & 0x0F);
   frame[7] = (seq << 4) | pgm_read_byte(crcTable + crc);
   
   /*
   // send last byte
   loop_until_bit_is_set(UCSR0A, UDRE0);
   UDR0 = crc;
    */
   
   add_tx_data(frame+startPos, 8-startPos);
}


void sendProgmemStr(const char *str)
{
   while(1)
   {
      const char b = pgm_read_byte(str);
      if (b == 0)    return;
      
      add_tx_data(&b, sizeof b);
      /*
       loop_until_bit_is_set(UCSR0A, UDRE0);
       UDR0 = b;
       */
      str++;
   }
}

void sendStatus(void)
{
   // send all analog inputs and battery and calculate CRC
   byte crc = 0;
   ADCSRA = B(ADEN) | B(ADPS2) | B(ADPS0); // Enable ADC with 1/32 clock prescaler - fAD = 375 kHz @ 12 MHz
   
   for(byte ch = 1; ch <= 7; ch++)
   {
      ADMUX = B(REFS0) | ch;         // ADC Reference is AVcc; right adjusted result; select ch analog input
      ADCSRA |= B(ADSC);
      loop_until_bit_is_clear(ADCSRA, ADSC);
      const word val = ADC & 0x3FF;
      add_tx_data(&val, sizeof val);
      crc = wordCRC(crc, val);
   }
   
   ADCSRA = B(ADIF);    // Disable ADC and clear pending interrupt flag
   
   // send battery threshold and calculate CRC
   byte b = batThres - BATTHRES_BASE;
   add_tx_data(&b, sizeof b);
   crc = byteCRC(crc, b);
   
   // send analog output value and calculate CRC
   b = OCR0B;
   add_tx_data(&b, sizeof b);
   crc = byteCRC(crc, b);
   
   // send digital ports and calculate and send CRC
   b = 0;
   if (ISSET_I1)  b |= 0x08;
   if (ISSET_I2)  b |= 0x04;
   if (ISSET_O1)  b |= 0x02;
   if (ISSET_O2)  b |= 0x01;
   crc = pgm_read_byte(crcTable + crc) ^ b;
   b = (b << 4) | pgm_read_byte(crcTable + crc);
   add_tx_data(&b, sizeof b);
}
