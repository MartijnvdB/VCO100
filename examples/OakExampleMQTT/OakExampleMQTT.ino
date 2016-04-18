/*
   Name:    OakExampleMQTT
   Date:    April 2016
   Author:  MBUR

   Example sketch for use with the Voltcraft CO-100 class, VCO100, in the form of
   a Finite State Machine (FSM).

   instantiate the object,
   wait for the moment that a new data frame has arrived,
   sample the data frame,
   output the data wirelessly to an MQTT topic.

*/



#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <VCO100.h>


/* MQTT variables */
const char* mqtt_server = "192.168.178.190";    // raspberrypi, Domoticz
const unsigned int mqtt_port = 11883;
const char* connection_id = "ESP8266Client";
const char* client_name = "digistumpoak";
const char* client_password = "yrhft%43";
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
#define MINWAIT_MILLIS 20        // minimum wait time between data words

const float tempOffset = 1.0;     // Temp indication on the device seems to be on the high side.

/*
   Domoticz configuration
*/


/*
    Global variables
*/
int prefdirty;
volatile int val;                     // pin value read in the ISR
volatile int dirty;                   // dirty flag for data processing
volatile unsigned long latestTrigger; // latest time the IST executed
unsigned long now;                    // timestamp
int nextStatus = 0;                   // status variable for FSM
float cotwo, temp, hum;               // measured data values
float prefCotwo, prefHum, prefTemp;   // previously measured data values



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

  // Connect here, in case we want to have some publishing capabilities outside
  // the publishToMQTT function.
  reconnect();

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
      yield();
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
      yield();
      break;

    /* Formatting and publishing to MQTT. */
    case 30:
      char msg[81];

      // Access measured data in object.
      cotwo = Voltcraft.getValue('P');  // CO2
      hum = Voltcraft.getValue('A');    // Hum
      temp = floor(10 * Voltcraft.getValue('1')) / 10;   // Temp, degC, get rid of superfluous decimals

      // If we have a valid (non-zero) temperature then apply the temperature correction.
      if (temp) {
        temp -= tempOffset;
      }

      /* Publish values
         Only:
         - if data is valid
         - if data has changed wrt. previous values
      */
      if (cotwo && ( abs(cotwo - prefCotwo) >= 5) ) {
        static char outstr[5];
        dtostrf(cotwo, sizeof(cotwo), 0, outstr);  // convert float to string
        snprintf (msg, 50, "{\"name\":\"Voltcraft CO2\",\"idx\":%i,\"nvalue\":%s}", domoIdxCO2, outstr); // format output message
        publishToMQTT(msg);
        prefCotwo = cotwo;
        yield();
      }
      if (hum && temp && ( (hum != prefHum) || ( abs(temp - prefTemp) > 0.15 ) ) ) { // temp. tends to flap...
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

        dtostrf(temp, sizeof(temp), 1, outstr);  // convert float to string

        snprintf (msg, 80, "{\"name\":\"Voltcraft Temp en Vocht\",\"idx\":%i,\"nvalue\":0,\"svalue\":\"%s;%i;%i\"}", domoIdxTempHum, outstr, (int)hum, humStatus);
        publishToMQTT(msg);

        prefHum = hum;
        prefTemp = temp;
        yield();
      }

      nextStatus = 0;
      break;

    default:  // PANIC - we should never get here!
      while (1) {
        digitalWrite(BUILTIN_LED, HIGH);
        delay(100);
        yield();
        digitalWrite(BUILTIN_LED, LOW);
        delay(50);
        yield();
      }
  } // switch

  yield();  // Let Oak handle it's WiFi stuff

} // loop



/* publish a formatted message to MQTT
*/
void publishToMQTT(char* msg) {

  // Reconnect, if necessary.
  if (!client.connected()) {
    reconnect();
  }

  client.publish(outTopic, msg);

} // publishToMQTT



/*
   Reconnect to MQTT
*/
void reconnect() {
  if (client.connect(connection_id, client_name, client_password)) {
    client.publish(statusTopic, "Oak has (re)connected");
  }
} // reconnect



/*
   Interrupt service routine, gets called on the FALLING clock.
   Not placed in the VCO100 class for simplicity's sake.
*/
void ISR_readdata() {
  val = digitalRead(DATA_PIN);
  dirty = !dirty; // toggle flag
  latestTrigger = micros(); // this does work inside the ISR.
} // ISD_readdata



