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


#define def #define

#define NL /*
*/

#define ASSIGN_PIN(name, reg, bit) \
def SET_##name (PORT##reg |= B(bit)) NL \
def CLR_##name (PORT##reg &= ~B(bit)) NL \
def ISSET_##name (PIN##reg & B(bit)) NL \
def BIT_##name B(bit) NL \
def BITPOS_##name bit

// Pin assignments

ASSIGN_PIN(I1  , B, 0)     // I (with pull-up, BTN)
ASSIGN_PIN(I2  , B, 1)     // I (with pull-up)

ASSIGN_PIN(SS  , B, 2)     // O
ASSIGN_PIN(MOSI, B, 3)     // O
ASSIGN_PIN(MISO, B, 4)     // I (with pull-up)
ASSIGN_PIN(SCK , B, 5)     // O

ASSIGN_PIN(CTS , C, 0)     // I (with pull-up)
ASSIGN_PIN(A1  , C, 1)     // Analog I (EMG)
ASSIGN_PIN(A2  , C, 2)     // Analog I (ECG)
ASSIGN_PIN(A3  , C, 3)     // Analog I (EDA)
ASSIGN_PIN(A4  , C, 4)     // Analog I (EEG)
ASSIGN_PIN(A5  , C, 5)     // Analog I (ACC)
ASSIGN_PIN(A6  , C, 6)     // Analog I (LUX)
ASSIGN_PIN(ABAT, C, 7)     // Analog I (ABAT)

// PD0: RXD
// PD1: TXD
ASSIGN_PIN(INT     , D, 2) // I (with pull-up)
ASSIGN_PIN(O1      , D, 3) // O
ASSIGN_PIN(O2      , D, 4) // O

ASSIGN_PIN(PWM     , D, 5) // O
ASSIGN_PIN(LED_STAT, D, 6) // O
ASSIGN_PIN(LED_BAT , D, 7) // O
