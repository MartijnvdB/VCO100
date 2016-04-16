# VCO100 library for the Voltcraft CO-100
This library provides a class for reading temperature, relative humidity and CO2 level from the Voltcraft CO-100 air quality measuring device.

## Target systems
The code is written for the Arduino and Digistump Oak. It should be possible to run this on many other devices as well.

## Examples
The code comes with two examples that read the data from the CO-100:

- a sketch for Arduino which writes measured values to serial output.
- a sketch for Digistump Oak which publishes measured values to a Mosquitto MQTT topic.

## License
This code is released under the GPL v3.
