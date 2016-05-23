/**
 * \file
 * \copyright  Copyright 2014-2016 PLUX - Wireless Biosignals, S.A.
 * \author     Filipe Silva
 * \version    1.0
 * \date       September 2015
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


// Decomment the line below to make the following changes on each frame:
//  first sample : buffer head / 2
//  second sample : buffer tail / 2
//  third digital value (IO2) : UDRIE0 bit value
//#define TEST_BUFFER


EMPTY_INTERRUPT(ADC_vect);             // Used only to wake up after ADC conversion


static word convADC10(byte channel)  // 10-bit ADC conversion
{
   ADMUX = B(REFS0) | channel;         // ADC Reference is AVcc; right adjusted result; select analog input
   
   __asm volatile("sleep");               // autostart a new ADC conversion
   
   loop_until_bit_is_clear(ADCSRA, ADSC); // MCU can wake up by UDRE or PCINT1 interrupts, so ADC may still be converting.
                                          // Busy-wait for ADC completion (not safe to sleep again since a new ADC conversion may start)
   
   return ADC & 0x3FF;    // ADC value read
}

void timer1TickLive(void)
{
   // Acquire and send a new data frame
   
	byte frame[8];
   
   /*
   static byte t1 = 0;
   byte t0 = TCNT1;
   */
   
#ifdef TEST_BUFFER
   word xhead = head >> 1;
   word xtail = tail >> 1;
   byte xudrie = UCSR0B & B(UDRIE0);
#endif
   
   // Disable all interrupts except ADC, UDRE and PCINT1
   UCSR0B &= ~B(RXCIE0);   // Disable UART RX interrupt (but keep UDRE interrupt unchanged)
   TIMSK1 = 0;             // Disable TIMER 1 compare match interrupt
   TIMSK2 = 0;             // Disable TIMER 2 compare match interrupt
   
   sei();   // To enable ADC, UDRE and PCINT1 interrupts
   
   memset(frame, 0, 7);
   
   if (ISSET_I1)  frame[6] |= 0x80;
   if (ISSET_I2)  frame[6] |= 0x40;
#ifdef TEST_BUFFER
   if (xudrie)    frame[6] |= 0x20;
#else
   if (ISSET_O1)  frame[6] |= 0x20;
#endif
   if (ISSET_O2)  frame[6] |= 0x10;
   
   ADCSRA = B(ADEN) | B(ADIE) | B(ADPS2) | B(ADPS0); // Enable ADC with interrupts and 1/32 clock prescaler - fAD = 375 kHz @ 12 MHz
   
#ifdef TEST_BUFFER
   *(word*)(frame+5) |= xhead << 2; convADC10(chTable[0]);
#else
   *(word*)(frame+5) |= convADC10(chTable[0]) << 2;
   //*(word*)(frame+5) |= (word)seq << 2; convADC10(chTable[0]);  // decomment to send sequence number on first channel
#endif
   
   if (nChannels > 1)
#ifdef TEST_BUFFER
      *(word*)(frame+4) |= xtail; convADC10(chTable[1]);
#else
      *(word*)(frame+4) |= convADC10(chTable[1]);
#endif
   
   if (nChannels > 2)
      *(word*)(frame+2) |= convADC10(chTable[2]) << 6;
   if (nChannels > 3)
      *(word*)(frame+1) |= convADC10(chTable[3]) << 4;
   if (nChannels > 4)
      *(word*)frame |= (convADC10(chTable[4]) & 0x3F0) << 2;   // only the 6 upper bits of the 10-bit value are used
   if (nChannels > 5)
      frame[0] |= convADC10(chTable[5]) >> 4;                  // only the 6 upper bits of the 10-bit value are used
   
   if (!ISSET_LED_BAT)
      if (convADC10(BITPOS_ABAT) < batThres)     SET_LED_BAT;   // check battery voltage if battery LED not already on
   
   ADCSRA = 0;    // Disable ADC
   
   /*
   *(word*)(frame+5) = t0 << 2;
   frame[4] = TCNT1;
   *(word*)(frame+2) = ((word)t1) << 6;
   */
   
   // Re-enable all other interrupts (when this ISR context ends)
   cli();               // Disable Global Interrupt enable bit
   UCSR0B |= B(RXCIE0); // Reenable UART RX interrupt
   TIMSK1 = B(OCIE1A);  // Reenable TIMER 1 compare match interrupt
   TIMSK2 = B(OCIE2A);  // Reenable TIMER 2 compare match interrupt
   
   sendFrameCRC(frame);
   
   //t1 = TCNT1;
}
