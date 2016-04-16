/* Name:    VCO100
   Purpose: Object for Voltcraft CO-100 data and methods.
   Author:  Martijn van den Burg, martijn@[remove-me-first]palebluedot . nl
   
   Copyright (C) 2016 Martijn van den Burg. All right reserved.

   This program is free software: you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation, either version 3 of the License, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/ 


#include "Arduino.h"
#include "VCO100.h"

#define FRAME_SIZE 40    // number of bits in one data frame


/* 
 * Constructor.
 */
VCO100::VCO100() {
  data_ok = 0;
  reset();
} 


/*
 * Destructor
 */
VCO100::~VCO100() { /* nothing to do */ }


/* Stores the measured value in an array of bytes. */
void VCO100::store(byte val) {
  byte idx = bitstored / 8;      // calculate the byte in which to store this bit
  byte posn = bitstored % 8;     // calculate bit position within byte
  bufferIn[idx] = bufferIn[idx] | (val ? 1 : 0) << (7 - posn);  // store 1/0 value of measured bit in byte buffer
  bitstored++;			// keep track of how many we stored

  // Update the private data when we're done reading values.
  if (! readmore()) {
    update();
  }
} // store


/* 
 * Accessor to update object attributes with measured values.
 */
void VCO100::update() {
  // Fill INT with the MSB and LSB of the measured data
  int value = 0 | bufferIn[1] << 8;	// MSB
      value = value | bufferIn[2];	// LSB

  byte measured_checksum = bufferIn[3];
  byte calculated_checksum = (bufferIn[0] + bufferIn[1] + bufferIn[2]) % 256; // take low byte
  
  /* Do not store value when checksum NOK */
  if (measured_checksum != calculated_checksum) {
    data_ok = 0;
  }
  else {
    data_ok = 1;

    switch (bufferIn[0]) {
      case 0x41:	// relative humidity
	this->relHum.value = value / 100;
	this->relHum.valid = true;
	break;
      case 0x42:	// Temperature, Kelvin
	this->tempKelvin.value = (float)value / 16;
	this->tempKelvin.valid = true;
	break;
      case 0x50:	// CO2 perccentage
	this->coTwo.value = value;
	this->coTwo.valid = true;
	break;
      default:
	// nothing
	break;
    } // switch

  }
  
} // update


/* Returns 1 if we haven't read all FRAME_SIZE values, as counted by bitstored.
 * Returns 0 otherwise.
 */
int VCO100::readmore() {
  return (bitstored == FRAME_SIZE ? 0 : 1);
} // readmore


/*
 * Public accessor for measured and derived values.
 * Returns the measured value if it is valid, and zero
 * otherwise.
 * When the room temperature or humidity drops to zero
 * we have bigger issues then Voltcraft measurements.
 */
float VCO100::getValue(char item) {
  if (item == 'A') { return (this->relHum.valid ? this->relHum.value : 0); }
  if (item == 'B') { return (this->tempKelvin.valid ? this->tempKelvin.value : 0); }
  // This is the temp. in deg Celsius, derived from Kelvin.
  if (item == '1') { return (this->tempKelvin.valid ? this->tempKelvin.value -273.15 : 0); }
  else if (item == 'P') { return (this->coTwo.valid ? this->coTwo.value : 0); }
  else { return 0; }
}


/* Accessor */
int VCO100::dataOK() {
  return data_ok;
}

/* 
 * Reset data structures for next round of signal measuring.
 */
void VCO100::reset() {
  bitstored = 0;
  memset(bufferIn, 0, sizeof(bufferIn)); // initialize (clear) the array we use for storing values
} // reset


/* End */