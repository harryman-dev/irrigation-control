#include <ESP8266WiFi.h>                                // WiFi library
#include <PubSubClient.h>                               // MQTT library
#include <ESP8266WebServer.h>                           // Webserver (fallback to weekup the MQTT server)

const char* SSID = "<<SSID>>";                                // WiFi SID
const char* PASSWORD = "<<Password>>";              // WiFi Password
const char* MQTT_BROKER = "xx.xx.xx.xx";                // IP address MQTT broker

const char* clientId = "<<Name>>";                          // name of this device = unique client identifier
                                                          // Set your Static IP address, gateway, subnet
IPAddress ip(xx, xx, xx, xx);                             // IP address of this device (Wemos D1 mini)
IPAddress gateway(xx, xx, xx, xx);                       // IP address gateway (e.g. router)
IPAddress subnet(255, 255, 255, 0);

int const aRelayPin[8]={16,5,4,0,2,14,12,13};           // Array of used relay pins (GPIO)
                                                        // GPIO: D0:16, D1:5, D2:4, D3:0, D4:2, D5:14, D6:12, D7:13
                                                        // D4:2 = BUILDIN_LED

int const NoOfPins = 8;

#define MAXMIN (1000UL * 60 * 40)                       // max pump / valve ON time (40 min)

uint32_t lMaxTimeRelay,lStartTimeProg1,lStartTimeProg2,lMaxTimeProg1,lMaxTimeProg2;

bool bIsRelayOn = false, bIsValveRelayOn = false, bProg1 = false, bProg2 = false;

int i, j, iStepProg1=0, iStepProg2=0;
uint32_t iTime, aTime[3];

String sTopic, sPayload, sRelayStatus;

WiFiClient espClient;
PubSubClient client(espClient);
ESP8266WebServer webServer(80);

//------- Start initialize -----------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// setup_wifi: setup WiFi
//--------------------------------------------------------------------------------------------------------------
void setup_wifi() {                                       // try to start WiFi

  Serial.println("Connecting to WiFi");

  WiFi.mode(WIFI_STA);                                    // station mode = connect to existing access point (e.g. router)
  WiFi.begin(SSID, PASSWORD);

                                                          // Configures static IP address
  if (!WiFi.config(ip, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

//--------------------------------------------------------------------------------------------------------------
// setup_web: setup WebServer
//--------------------------------------------------------------------------------------------------------------
void setup_web() {                                       // try to start Webserver

  Serial.print("Webserver ");

  webServer.onNotFound([](){                             // 404-Page definition
    webServer.send(404, "text/plain", "Page not found!");  
  });
 
  webServer.on("/", []() {                              // Root-Page definition
    webServer.send(200, "text/plain", "Webserver is running!");
  });
  
  webServer.on("/restart", []() {                       // restart command (stop / restart MQTT connection)
    restartMQTT();
  });
  webServer.begin();                                    // start webserver
  Serial.println("Started");
}

//--------------------------------------------------------------------------------------------------------------
// setup_mqtt: setup MQTT server
//--------------------------------------------------------------------------------------------------------------
void setup_mqtt() {                                       // try to connect to an existing MQTT broker
  client.setServer(MQTT_BROKER, 1883);
  client.setCallback(callback);
}

//--------------------------------------------------------------------------------------------------------------
// connect_mqtt: connect / reconnect to a MQTT broker
//--------------------------------------------------------------------------------------------------------------
void connect_mqtt() {

  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

                                                          // connect and set last will message (mode)
    if (client.connect(clientId,NULL,NULL,"/aussen/mode",0,false,"Offline")) {
      Serial.println("connected");
         
      client.publish("/aussen/mode", "Online");                       // publish mode
      client.publish("/aussen/RSSI", String(WiFi.RSSI()).c_str());    // publish RSSI
     for (i=0;i<NoOfPins;i++) {                          // offset: relais are starting with one, pin array starts with index zero
        getRelayStatus(i+1);                              // subscribe all relays and waiting for commands
        sPayload = "/aussen/R"+String(i+1)+"/set";
        if (client.subscribe(sPayload.c_str())) Serial.println ("Subscribed R"+String(i+1));
      }
      if (client.subscribe("/aussen/P1/set")) Serial.println ("Subscribed P1");    // subscribe program 1
      if (client.subscribe("/aussen/P2/set")) Serial.println ("Subscribed P2");    // subscribe program 2
      if (client.subscribe("/aussen/STS/get")) Serial.println ("Subscribed STS");   // subscribe status request
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      delay(5000);                                         // Wait 5 seconds before retrying
    }
  }
}

//--------------------------------------------------------------------------------------------------------------
// setup: initilize Wifi, WebServer and MQTT server
//--------------------------------------------------------------------------------------------------------------
void setup() {

  Serial.begin(9600);
  Serial.println("start");

  for (i=0;i<NoOfPins;i++) {                                // Initialize Pins as inactive (OFF)
    digitalWrite(aRelayPin[i], HIGH);
    pinMode(aRelayPin[i], OUTPUT);
  }

  digitalWrite(BUILTIN_LED, LOW);                           // Turn LED on
  pinMode(BUILTIN_LED, OUTPUT);                             // Initialize the BUILTIN_LED pin as an output

  setup_wifi();
  setup_web();
  setup_mqtt();

  delay(2000);
  digitalWrite(BUILTIN_LED, HIGH);                          // Turn LED off
}

//------- main loop --------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// loop: listening on MQTT commands & emergency exits for all functions 
//--------------------------------------------------------------------------------------------------------------
void loop() {

  if (!client.connected()) {
    connect_mqtt();
  }

  client.loop();

  webServer.handleClient();                                                // waiting for webserver commands

  if (bProg1) {                                                            // run prog 1
     if (millis() >= lMaxTimeProg1) {                                      // emergency exit prog 1
      resetProg1();
      Serial.println("Emergency exit P1");
    } else {
      bProg1 = runProg1(iStepProg1, aTime, lStartTimeProg1);
    }
  }

  if (bProg2) {                                                            // run prog 2
    if ((millis() >= lMaxTimeProg2)) {                                     // emergency exit prog 2
      resetProg2();
      Serial.println("Emergency exit P2");
    } else {
      bProg2 = runProg2(iStepProg2, iTime, lStartTimeProg2);
    }
  }

  if (bIsRelayOn == true && millis() >= lMaxTimeRelay){                   // emergency exit for a single relay => protect the worst case: reservoir is empty
    for (i=8;i>=1;i--) {
      switchRelayOff(i);
    }
    Serial.println("switch off (max ON time) completed");
    bIsRelayOn = false;
  }

}

//--------------------------------------------------------------------------------------------------------------
// callback: MQTT callback function
//--------------------------------------------------------------------------------------------------------------
void callback(char* topic, byte* payload, unsigned int length) {
  char cRelayNo;
  int iRelayNo, aMin[3], iMin;
 
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");

  sTopic ="";
  for (int i = 0; i < strlen(topic); i++) {
    sTopic += (char)topic[i];
  }
  Serial.println(sTopic);

  sPayload = ""; 
  for (int i = 0; i < length; i++) {
    sPayload += (char)payload[i];
  }
  Serial.println(sPayload);
  
  if (sTopic.substring(8,10) == "P1") {                          // special program for watering three different cycles - sPayload e.g. 10|10|5 or 6|6|3 or 0|0|3 (minutes per cycle)
 
    lMaxTimeProg1 = 0;
        
    char* aChar = &sPayload[0];
    sscanf(aChar, "%d|%d|%d", &aMin[0], &aMin[1], &aMin[2]);      // split sPayload

    for (int i=0; i < 3; i++) {
      if (aMin[i]<0 || aMin[i]>20) aMin[i]=0;                     // value have to be in a valid range
      aTime[i] = (uint32_t)aMin[i] * 1000 * 60;
      lMaxTimeProg1 +=aTime[i];
    }

    if (aMin[0] <= 0 && aMin[1] <= 0 && aMin[2] <= 0) {
      resetProg1();
    } else {
      lStartTimeProg1 = millis();
      lMaxTimeProg1 = (uint32_t)lStartTimeProg1 + lMaxTimeProg1 + 10000; // emergency exit 10s after regular end
      if (aMin[0] > 0) iStepProg1 = 0;
      else if (aMin[1] > 0) iStepProg1 = 2;
      else if (aMin[2] > 0) iStepProg1 = 4;
      setProgramStatus(1, "ON");
      Serial.println ("Start P1 " + String(iStepProg1));
      bProg1 = true;
    }

  } else if (sTopic.substring(8,10) == "P2") { 
    iMin = sPayload.toInt();                                        // check sPayload for validity
    if (iMin<0 || iMin>20) iMin=0;                                  // value has to be in a valid range
    iTime = (uint32_t)iMin * 1000 * 60;
        
    if (iTime <= 0) {                                               // special program for watering five different cycles - sAction e.g. 12 (minutes per cycle)
      resetProg2();
    } else {
      lStartTimeProg2 = millis();
      lMaxTimeProg2 = (uint32_t)lStartTimeProg2 + (5*iTime) + 10000; // emergency exit 10s after regular end
      iStepProg2 = 0;
      setProgramStatus(2, "ON");
      Serial.println ("Start P2");
      bProg2 = true;
    }

  } else if (sTopic.substring(8,9) == "R") {                           // switch a single relay
    cRelayNo = sTopic.substring(9,10).charAt(0);
    if(isDigit(cRelayNo)) {
      iRelayNo = cRelayNo - '0';
      relayAction(iRelayNo, sPayload);
      if (sPayload =="ON") {
        lMaxTimeRelay = millis() + MAXMIN;                             // initialize emergency switch off after max time
        Serial.println(String(lMaxTimeRelay));
        bIsRelayOn = true;
      }
    }
 
  } else if (sTopic.substring(8,11) == "STS") {                       // ask for status of all relais (STS)
    getRelayStatusAll();
  }
}

//------- Start some additional functions ----------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// runProg1: run the program step by step (R2, R3, R4) + pump relay (P1)
//--------------------------------------------------------------------------------------------------------------
bool runProg1(int &iStep, uint32_t aTime[3], uint32_t &lStartTime) {
    if (iStep == 0 && aTime[0]>0) { 
      relayAction(2, "ON");                                                                                   // ON OFF OFF
      lStartTime = millis();
      iStep++;
      Serial.println("P1 start step 1 " + String(aTime[0]));
    } else if (iStep == 1 && aTime[0]>0 && (uint32_t)(millis() - lStartTime) >= aTime[0]) {                   // switch to the next relay
      relayAction(2,"OFF");
      iStep++;
      Serial.println("P1 end step 1 " + String(lStartTime));
    } else if (iStep == 2 && aTime[1]>0) {                                                                    // start with 2
      relayAction(3,"ON");
      lStartTime = millis();
      iStep++;
      Serial.println("P1 start step 2 " + String(lStartTime));
    } else if (iStep == 3 && aTime[1]>0 && (uint32_t)(millis() - lStartTime) >= aTime[1]) {                  // switch to the next relay
      relayAction(3,"OFF");
      iStep++;
      Serial.println("P1 end step 2 " + String(lStartTime));
    } else if (iStep == 4 && aTime[2]>0) {                                                                   // start with 3
      relayAction(4,"ON");
      lStartTime = millis();
      iStep++;
      Serial.println("P1 start step 3 " + String(lStartTime));
    } else if (iStep == 5 && aTime[2]>0 && (uint32_t)(millis() - lStartTime) >= aTime[2]) {
      relayAction(4,"OFF");                                                                                  // switch off, last step completed
      relayAction(1,"OFF");                                                                                 // switch off pump, program completed
      setProgramStatus(1, "OFF");
      Serial.println ("End P1");
      return false;
    }
    
    return true;
}

//--------------------------------------------------------------------------------------------------------------
// resetProg1: switch off R2, R3, R4 (valves) + R1 (pump)
//--------------------------------------------------------------------------------------------------------------
void resetProg1() {

    relayAction(2,"OFF");
    relayAction(3,"OFF");
    relayAction(4,"OFF");
    relayAction(1,"OFF");
    setProgramStatus(1, "OFF");
    Serial.println ("Reset P1");
    bProg1 = false;
}

//--------------------------------------------------------------------------------------------------------------
// runProg2: run the program step by step (R6, R6+R7, R6, R6+R8, R8)
//--------------------------------------------------------------------------------------------------------------
bool runProg2(int &iStep, uint32_t iTime, uint32_t lStartTime) {

    if (iStep == 0 && iTime>0) {                                                                       // ON OFF OFF
      relayAction(6,"ON");
      iStep = 1;
      Serial.println("P2 step 1 " + String(iTime));
     } else if (iStep == 1 && iTime>0 && (uint32_t)(millis() - lStartTime) >= iTime) {                // ON ON OFF
      relayAction(7,"ON");
      iStep = 2;
      Serial.println("P2 step 2 " + String(iTime*2));
    } else if (iStep == 2 && iTime>0 && (uint32_t)(millis() - lStartTime) >= (iTime*2)) {             // OFF ON OFF
      relayAction(6,"OFF");
      iStep = 3;
      Serial.println("P2 step 3 " + String(iTime*3));
    } else if (iStep == 3 && iTime>0 && (uint32_t)(millis() - lStartTime) >= (iTime*3)) {             // OFF ON ON
      relayAction(8,"ON");
      iStep = 4;
      Serial.println("P2 step 4 " + String(iTime*4));
    } else if (iStep == 4 && iTime>0 && (uint32_t)(millis() - lStartTime) >= (iTime*4)) {             // OFF OFF ON
      relayAction(7,"OFF");
      iStep = 5;
      Serial.println("P2 step 5 " + String(iTime*5));
    } else if (iStep == 5 && iTime>0 && (uint32_t)(millis() - lStartTime) >= (iTime*5)) {             // OFF OFF OFF
      relayAction(8,"OFF");
      setProgramStatus(2, "OFF");
      Serial.println ("End P2");
      return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------------------
// resetProg2: switch off R6, R7, R8 (valves)
//--------------------------------------------------------------------------------------------------------------
void resetProg2() {

    relayAction(6,"OFF");
    relayAction(7,"OFF");
    relayAction(8,"OFF");
    setProgramStatus(2, "OFF");
    Serial.println ("Reset P2");
    bProg2 = false;
}

//--------------------------------------------------------------------------------------------------------------
// setProgramStatus: get program status
//--------------------------------------------------------------------------------------------------------------
void setProgramStatus(int iProgNo, String sAction) {
  sTopic = "/aussen/P"+String(iProgNo)+"/state";
  if (client.publish(sTopic.c_str(), sAction.c_str())) Serial.println ("Published P"+String(iProgNo)+"  "+sAction);
  client.publish("/aussen/RSSI", String(WiFi.RSSI()).c_str());    // publish RSSI
}

//--------------------------------------------------------------------------------------------------------------
// relayAction: switch of a single relay to ON or OFF
//--------------------------------------------------------------------------------------------------------------
void relayAction(int iRelayNo, String sAction) {
  if (iRelayNo >= 1 && iRelayNo <= 8  && sAction != "") {
    if (sAction == "ON") {
      switchRelayOn(iRelayNo);
    } else if (sAction == "OFF") {
      switchRelayOff(iRelayNo);
    }
  }
}

//--------------------------------------------------------------------------------------------------------------
// switchPumpOn: switch pump relay to ON, if not yet ON
//--------------------------------------------------------------------------------------------------------------
void switchPumpOn(int iRelayNo) {
  if (digitalRead(aRelayPin[iRelayNo-1]) == HIGH) {
    digitalWrite(aRelayPin[iRelayNo-1], LOW);
    getRelayStatus(iRelayNo);
    delay(2000);                                              // two seconds between pump ON and any other relay ON to build pressure in the pipeline
  }
}

//--------------------------------------------------------------------------------------------------------------
// switchRelayOn: switch any relay ON
//--------------------------------------------------------------------------------------------------------------
void switchRelayOn(int iRelayNo) {
  if (iRelayNo == 1) {                                        // 1 = pump relay
    switchPumpOn(1);
  } else {
    if (iRelayNo <=5) {
      for (i=2;i<=5;i++) {                                    // first switch OFF all "others" relays in range of 2-5 to avoid pump overload
        if (i!=iRelayNo) {
           switchRelayOff(i);
        }
      }
      switchPumpOn(1);                                        // pump have to switch to ON before (if not yet ON)
    }
    
    if (digitalRead(aRelayPin[iRelayNo-1]) == HIGH) {
      digitalWrite(aRelayPin[iRelayNo-1], LOW);                 // set ONE relay only to ON
      getRelayStatus(iRelayNo);
    }
  }
}

//--------------------------------------------------------------------------------------------------------------
// switchRelayOff: switch any relay OFF
//--------------------------------------------------------------------------------------------------------------
void switchRelayOff(int iRelayNo) {
  if (digitalRead(aRelayPin[iRelayNo-1]) == LOW) {
    digitalWrite(aRelayPin[iRelayNo-1], HIGH);
    getRelayStatus(iRelayNo);
  }
}

//--------------------------------------------------------------------------------------------------------------
// getRelayStatus: get relay status
//--------------------------------------------------------------------------------------------------------------
void getRelayStatus(int iRelayNo) {
 
  if (digitalRead(aRelayPin[iRelayNo-1]) == LOW) {
    sRelayStatus = "ON";
  } else {
    sRelayStatus = "OFF";
  }
  sTopic = "/aussen/R"+String(iRelayNo)+"/state";
  if (client.publish(sTopic.c_str(), sRelayStatus.c_str())) Serial.println ("Published to R"+String(iRelayNo)+"  "+sRelayStatus);
  client.publish("/aussen/RSSI", String(WiFi.RSSI()).c_str());    // publish RSSI
}

//--------------------------------------------------------------------------------------------------------------
// getRelayStatusAll: get status of all relais
//--------------------------------------------------------------------------------------------------------------
void getRelayStatusAll() {
  for (i=0;i<NoOfPins;i++) {
    getRelayStatus(i+1);
  }
 }

//--------------------------------------------------------------------------------------------------------------
// restartMQTT: restart MQTT
//--------------------------------------------------------------------------------------------------------------
void restartMQTT(){ 
  client.disconnect();
  Serial.println ("MQTT disconnected");
  setup_mqtt();
  webServer.send(200, "text/plain", "OK");
}
