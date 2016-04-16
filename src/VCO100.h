/*
 VCO100.h 
 
 Copyright 2016 Martijn van den Burg, martijn@[remove-me-first]palebluedot . nl
 
 This file is part of the VCO100 library for reading Voltcraft CO-100
 data with Arduino.
 
 VCO100 is free software: you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free
 Software Foundation, either version 3 of the License, or (at your option)
 any later version.
 
 This software is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.
   
   You should have received a copy of the GNU General Public License
   along with VoltcraftCO100.  If not, see <http://www.gnu.org/licenses/>.
   
   */


/* This library is written for the Arduino Duemilanova and Arduino software version 1.6.8.
 * 
 * This library may work with other hardware and/or software. YMMV.
 */

#include "Arduino.h"

#ifndef VCO100_h
#define VCO100_h

#define VERSION 1.0


class VCO100 {
  public:
    VCO100();
    ~VCO100();

    // public functions, accessors
    void reset();
    void store(byte);
    int readmore();
    int dataOK();
    float getValue(char);
  
  private:
    // Structs for measured value and validity flag
    struct {
      float value;
      bool valid = false;
    } coTwo;
    struct {
      float value;
      bool valid = false;
    } relHum;
    struct {
      float value;
      bool valid = false;
    } tempKelvin;

    int data_ok;
    byte bufferIn[5];
    int bitstored;

    void update();
    void setValue(int, float);
};


#endif