/*
   Name:    OakExampleMQTT
   Date:    April 2016
   Author:  MBUR

   Example sketch for use with the Voltcraft CO-100 class, VCO100, in the form of
   a Finite State Machine (FSM):

   - wait for the moment that a new data frame has arrived
   - reset internal data
   - sample the data frame
   - format the data for publication
   - publish the data wirelessly to an MQTT topic.

*/



#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <VCO100.h>


/* MQTT variables */
const char* mqtt_server = "192.168.178.190";    // raspberrypi, Domoticz
const unsigned int mqtt_port = 11883;
const char* connection_id = "ESP8266Client";
const char* client_name = "digistumpoak";
const char* client_password = "xxxxx";
const char* outTopic = "domoticz/in";           // MQTT topic for Domoticz
const char* statusTopic = "outTopic";           // General (debug/status) topic


const int domoIdxCO2 = 201;                 // Domoticz device identifier
const int domoIdxTempHum = 203;             // Domoticz device identifier



WiFiClient espClient;
PubSubClient client(espClient);



/*
    Oak <-> Voltcraft interface
*/
#define CLOCK_PIN 9     // clock IN from CO-100
#define DATA_PIN 8      // data IN from CO-100
#define BUILTIN_LED 1   // Oak's LED
#define MINWAIT_MILLIS 20       // minimum wait time between data words
#define PUBLISH_DELAY_MILLIS 50 // Forced delay to allow Oak to do it's WiFi stuff
#define DELAY_BETWEEN_CONNECTS_MILLIS 5000  // time between MQTT reconnect attempts


const float tempOffset = 1.0;     // Temp indication on the device seems to be on the high side.


/*
   Domoticz configuration
*/


/*
    Global variables
*/
int prefdirty;
volatile int val;                         // pin value read in the ISR
volatile int dirty;                       // dirty flag for data processing
volatile unsigned long latestTrigger;     // latest time the IST executed
unsigned long previousReconnectAttempt = 0; // last time we tried to reconnect to MQTT
unsigned long now;                    // timestamp
int prevStatus = 0;                   // status variable for FSM
int nextStatus = 0;                   // status variable for FSM
float cotwo, temp, hum;               // measured data values
float prefCotwo, prefHum, prefTemp;   // previously measured data values
char msg[81];                         // MQTT payload message


/*
   Main sketch
*/

VCO100 Voltcraft;   // instantiate a VCO100 object

void setup() {
  pinMode(DATA_PIN, INPUT);   // Data out, Voltcraft
  pinMode(CLOCK_PIN, INPUT);  // Clock out, Voltcraft
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output

  client.setServer(mqtt_server, mqtt_port);

  attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), ISR_readdata, FALLING);

}


// loop() implements a FSM using switch/case statements.
void loop() {
  client.loop();    // MQTT, maintain server connection

  switch (nextStatus) {

    /* Wait until CLOCK has been HIGH for long enough. */
    case 0:
      // When CLOCK is HIGH and has been for more than MINWAIT_MILLIS we continue.
      // This is like a non-blocking wait state. Do the '>' comparison to capture micros() rollover.
      now = micros();
      if (digitalRead(CLOCK_PIN) == HIGH && now > latestTrigger && (now - latestTrigger > MINWAIT_MILLIS) ) {
        nextStatus = 10;
        break;
      }

    /* Reset object for next frame sampling. */
    case 10:
      prefdirty = dirty;  // flag voor data change
      Voltcraft.reset();  // reset internals for next round

      nextStatus = 20;
      break;

    /* Frame sampling. */
    case 20:
      if (Voltcraft.readmore()) {
        if (prefdirty != dirty) {
          prefdirty = dirty;
          Voltcraft.store(val);   // store it
        }
      }
      else {
        nextStatus = 30;
      }
      break;

    /* Formatting and publishing to MQTT. */
    case 30:
      // Access measured data in object.
      cotwo = Voltcraft.getValue('P');  // CO2
      hum = Voltcraft.getValue('A');    // Hum
      temp = floor(10 * Voltcraft.getValue('1')) / 10;   // Temp, degC, get rid of superfluous decimals

      // If we have a valid (non-zero) temperature then apply the temperature correction.
 //     if (temp) {
 //       temp -= tempOffset;
 //     }

      /* Publish values
         Only:
         - if data is valid
         - if data has changed enough wrt. previous values to prevent flapping and too much data accumulation
      */
      if (cotwo && ( abs(cotwo - prefCotwo) >= 5) ) {
        static char outstr[5];
        dtostrf(cotwo, sizeof(cotwo), 0, outstr);  // convert float to string
        snprintf (msg, 50, "{\"name\":\"Voltcraft CO2\",\"idx\":%i,\"nvalue\":%s}", domoIdxCO2, outstr); // format output message
        prefCotwo = cotwo;
        nextStatus = 40;
      }
      else if (hum && temp && ( (hum != prefHum) || ( abs(temp - prefTemp) > 0.15 ) ) ) { // temp. tends to flap...
        static char outstr[4];
        int humStatus;  // humidy status

        // Humidity qualifiers. Specific to Domoticz.
        // https://www.domoticz.com/wiki/Domoticz_API/JSON_URL's#Retrieve_status_of_specific_device
        if (hum < 30) {
          humStatus = 2;  // dry
        }
        else if (hum > 70) {
          humStatus = 3; // wet
        }
        else if (hum > 40 && hum < 60) {
          humStatus = 1;  // confortable
        }
        else {
          humStatus = 0;  // normal
        }

        dtostrf( (temp-tempOffset), sizeof(temp), 1, outstr);  // convert temperature float value to string, applying offset
        snprintf (msg, 80, "{\"name\":\"Voltcraft Temp en Vocht\",\"idx\":%i,\"nvalue\":0,\"svalue\":\"%s;%i;%i\"}", domoIdxTempHum, outstr, (int)hum, humStatus);

        prefHum = hum;
        prefTemp = temp;

        nextStatus = 40;  // go publish
      }
      else { // Nothing to publish
        nextStatus = 0;   // start over
      }
      break;

    /* Publish the data */
    case 40:
      if ( !client.connected() ) {    // need to establish connection first, before publishing
        prevStatus = 40;  // so we can return from the connection attempt
        nextStatus = 100;
      }
      else {
        client.publish(outTopic, msg);
        nextStatus = 0;
      }
      break;

    /* Reconnect to MQTT */
    case 100:
      if ( abs(millis() - previousReconnectAttempt) > DELAY_BETWEEN_CONNECTS_MILLIS) {  // non-blocking delay, we can still do stuff in loop()
        if (client.connect(connection_id, client_name, client_password)) {
          previousReconnectAttempt = millis();
          client.publish(statusTopic, "Oak has (re)connected");
          nextStatus = prevStatus;  // go back and attempt to publish our payload again
        }
        else {  // connect failed
          nextStatus = 0;  // give up, get new data, try again later
        }
      }
      break;
      
    default:  // PANIC - we should never get here!
      for (int i = 0; i < 5; i++) {
        digitalWrite(BUILTIN_LED, HIGH);
        delay(200);
        yield();
        digitalWrite(BUILTIN_LED, LOW);
        delay(100);
        yield();
      }
      nextStatus = 0;
  } // switch


  // Allow Oak to handle WiFi stuff in the loop()
  // while we're not busy reading data.
  if (! Voltcraft.readmore() ) {
    delay(PUBLISH_DELAY_MILLIS);
  }

} // loop


/*
   Interrupt service routine, gets called on the FALLING clock.
   Not placed in the VCO100 class for simplicity's sake.
*/
void ISR_readdata() {
  val = digitalRead(DATA_PIN);
  dirty = !dirty; // toggle flag
  latestTrigger = micros(); // this does work inside the ISR.
} // ISD_readdata



