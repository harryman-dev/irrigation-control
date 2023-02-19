# Irrigation-control with Wemos D1 Mini using MQTT protocol
This sketch is used to control a relay board (Kuman Relay Shield Module 8 channels relay module board 5V).<br>
It was created with the Arduino IDE.<br>
The following board was selected: Tools >> Board >> ESP8266 Boards (2.5.0) >> Wemos D1 R1.

Relay functions:
- turn on/off a main contactor, which turn on/off a submersible pump
- up to seven electromagnetic valves for different water circuits

Communication takes place via MQTT protocol.<br>
Single relays can be controlled directly. 
This sketch ensures that a maximum of 2 relays are switched on at the same time in order to prevent 
overloading of the Wemos D1 Mini and the relay board.<br>
An additional program (Prog1) enables individual relays (valves respectively water circuits) to be started 
in a sequence step by step. The "ON" time of the individual relays will be transferred as payload.

Further information is available at https://smarthome.kaempf-nk.de/bewaesserungsanlage/programmierung-wemos-d1-mini.html (in German).
