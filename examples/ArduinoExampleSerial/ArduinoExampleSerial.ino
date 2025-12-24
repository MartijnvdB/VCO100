
/*
   Name:    ArduinoExamplesSerial
   Date:    April 2016
   Author:  MBUR

   Exampleh sketch for use with the Voltcraft CO-100 class, VCO100, in the form of
   a Finite State Machine (FSM).


   instanciate the object,
   wait for the moment that a new data frame has arrived,
   sample the data frame,
   output the data as text to the serial console.

*/



#include <VCO100.h>

#define CLOCK_PIN 2     // clock IN from CO-100
#define DATA_PIN 3      // data IN from CO-100
#define ledPin 13       // debug LED
#define MINWAIT 20000   // uSec
#define PUBLISH_DELAY_MILLIS 3000  // time between consecutive MQTT publications


/* Global variables */

byte prefdirty;
volatile byte val;              // pin value read in the ISR
volatile int dirty;             // dirty flag for data processing
int nextStatus = 0;             // status variable for FSM
unsigned long beginTime;
unsigned long publishTime = 0;  // last time values were published to MQTT
// Measured values:
float cotwo, temp, hum;
// Previously measured data values:
float prefCotwo, prefHum, prefTemp;


/*
     Interrupt service routine, gets called on the FALLING clock.
     Not placed in the VCO100 class for simplicity's sake.
*/
void IRAM_ATTR ISR_readdata() {
  val = digitalRead(DATA_PIN);
  dirty = !dirty; // toggle flag
} // ISD_readdata


/* Main sketch */

VCO100 Voltcraft;

void setup() {
  pinMode(DATA_PIN, INPUT);   // Data out, Voltcraft
  pinMode(CLOCK_PIN, INPUT);  // Clock out, Voltcraft
  pinMode(ledPin, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), ISR_readdata, FALLING);

  Serial.begin(115200);
  Serial.println("Serial ready.");
}


// loop() implements a FSM using switch/case statements.
void loop() {

  switch (nextStatus) {
    case 0: // wait (blocking) for a moment when CLOCK_PIN has been HIGH for > MINWAIT us
      beginTime = micros();
      while (digitalRead(CLOCK_PIN)) {
        if (micros() - beginTime > MINWAIT) {
          nextStatus = 10;
          break;
        } // if
      }  // while HIGH
      break;
    case 10:
      // Read data: on the FALLING clock read the DATA_PIN.
      prefdirty = dirty;  // flag voor data change
      Voltcraft.reset();  // reset internals for next round

      // Read values until we had enough.
      while ( Voltcraft.readmore() ) {
        if (prefdirty != dirty) { // new value read by the ISR
          Voltcraft.store(val);   // store it
          prefdirty = dirty;
        }
      }

      nextStatus = 20;
      break;
    case 20:
      cotwo = Voltcraft.getValue('P');
      hum = Voltcraft.getValue('A');
      temp = Voltcraft.getValue('B');

      /* Publish values
          Only:
          - if more than PUBLISH_DELAY_MILLIS has passed since previous publication
          - if data is valid
          - if data has changed wrt. previous values
      */
      if (millis() - publishTime > PUBLISH_DELAY_MILLIS) {
        if (cotwo && cotwo != prefCotwo) {
          Serial.print("CO2 [ppm]: ");
          Serial.println(cotwo);
          prefCotwo = cotwo;
          publishTime = millis();
        }
        if (hum && hum != prefHum) {
          Serial.print("Hum [%]: ");
          Serial.println(hum);
          prefHum = hum;
          publishTime = millis();
        }
        if (temp && temp != prefTemp) {
          Serial.print("Temp. [degC]: ");
          Serial.println(Voltcraft.getValue('1'));
          prefTemp = temp;
          publishTime = millis();
        }
      }
      nextStatus = 0;
      break;
    default:  // we should never get here
      digitalWrite(ledPin, HIGH);
      delay(1000);
      digitalWrite(ledPin, LOW);
      break;
  } // switch

} // loop
