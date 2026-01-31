/*
********************************************
DISTANCE SENSOR SWITCH
********************************************
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "network.h"

#define TRIGGER   5     // D1
#define ECHO      4     // D2
#define LED       14    // D4
#define BUTTON    0     // D3
#define PHOTOCELL A0    // A0

const int MAINDELAY = 10;
const int BUTTONDELAY = 10;
const int WIFIDELAY = 1000;
const int TOGGLEDELAY = 1000;
const int PHOTOCELLDELAY = 1000 * 60;

const char* DAY_MODE = "day_mode";
const char* NIGHT_MODE = "night_mode";
String currentMode = NIGHT_MODE;

long duration, distance;
long triggerDistance = 10;
long maxDistance = 20;
long oldDistance = 0;
long range = 30;
bool state = false;

int photocellValue;
int photocellCount = 0;

bool buttonState;
bool buttonStateOld;

bool ledStateOld = true;
float ledBrightness = 0.000001;

String floorlampID = "12"; // Living room floor Lamp
String bedroomID = "84"; // Bedroom group
String plug4ID = "15"; // Plug4
String nightstandID = "8"; // Nightstand
String wallID = "5"; // Wall
String windowID = "10"; // window
String shelfID = "10"; // shelf
String bedroomLights[4] = {nightstandID, wallID, windowID, shelfID};


String currentLightID = wallID;

void setup() {
  Serial.begin (9600);
  delay(10);

  Serial.println('\n');
  Serial.println("Hello");
  Serial.println('\n');

  initWifi();
  initSensor();

  setNightMode();
}

void loop() {
  digitalWrite(TRIGGER, LOW);  
  delayMicroseconds(2); 
  
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10); 
  
  digitalWrite(TRIGGER, LOW);
  duration = pulseIn(ECHO, HIGH);
  distance = (duration/2) / 29.1;

  handleButton(digitalRead(BUTTON));

  if(currentMode == NIGHT_MODE) {
    handleDistance(distance);
  }

  if(currentMode == DAY_MODE) {
    handlePhotocell(); 
  }
  
  delay(MAINDELAY);
}

void initWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);             // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(SSID); Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(WIFIDELAY);
    Serial.print(++i); Serial.print('\n');
  }

  Serial.println("Connection established!");  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer
}

void initSensor() {
  pinMode(TRIGGER, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(BUILTIN_LED, OUTPUT);
  
  pinMode(LED_BUILTIN, OUTPUT); // or pinMode(2, OUTPUT); or pinMode(D4, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); 
}

void handleDistance(long distance) {
  bool oldState = state;

  if(distance < triggerDistance && oldDistance >= maxDistance) {
    bool state = true;
    String payload = getLightState(currentLightID);

    int statusStart = payload.indexOf("\"on\":");
    int statusEnd = payload.indexOf(",");
    String status = payload.substring(statusStart + 5, statusEnd);

    if(status == "false") {
      state = false;
    }

    if(currentLightID == plug4ID) {
      setLightState(!state, currentLightID);
      state = !state;
    }

    if(currentLightID == wallID) {
      if(state==true) {
        setLightState(false, currentLightID);
        state=false;
      } else {
        setLightBrightness(1.0, currentLightID);
        setLightState(true, currentLightID);
        state=true;
      }
    }

    delay(TOGGLEDELAY);
  }

  // if(state==true && oldState == true && distance < range) {
  //   float brightness = float(distance) / float(maxDistance) * 254.0;
  //   if(brightness > 254.0) { brightness = 254.0; }
  //   setLightBrightness(brightness, currentLightID);
  // }
  
  // Serial.print("Centimeters: ");
  // Serial.println(distance);
  oldDistance = distance;
}

void handleButton(bool state) {
  buttonStateOld = buttonState;
  buttonState = state;
  
  if(buttonState == 0 && buttonStateOld == 1) {
    // toggleLED();
    // handlePhotocell();
    toggleMode();
  }
}

void handlePhotocell() {
  /**
  Set a goal for the photocell to read. Update the brightness of the lights so that the photocell reads a certain value.

  If we want the photocell to read 512 then how much do we turn the lights up or down to achieve this?
  Maybe I should test the photocell reading during nighttime with the lights 100% bright to set a benchmark?
  Maybe do 5% of either brightness or dimness towards the goal value if it's more than 5% away from that goal value
  */
  photocellCount += 10;
  if(photocellCount >= PHOTOCELLDELAY) {
    float unit = .02; // percentage of brightness 

    // these are for readings from the sensor
    int reading = getPhotocellReading();
    int readingMax = 1023;
    int readingTarget = 125;
    int readingUnit = readingMax * unit;  // the amount of change that should be read from the sensor from an adjustment to the brightness

    // these are for the hue api brightness levels
    int brightness = 127;
    int brightnessMax = 254;
    int brightnessUnit = brightnessMax * unit;

    // these are for building the header data
    String payload = getGroupState(bedroomID);  // get the state of the room from the Hue api
    int briStart = payload.indexOf("bri\":"); // find where the brightness level reading starts is in the string
    int briEnd = payload.indexOf(",", briStart);  // find where it ends
    String currentBrightness = payload.substring(briStart + 5, briEnd); // get the number from the string
    int currentBrightnessInt = currentBrightness.toInt(); // convert it to an int
    int newBrightness = currentBrightnessInt; // set up a new variable which will be adjusted if needed

    // this is the amount of change that we would like to see in the sensor
    int delta = abs(reading - readingTarget);

    // Serial.print("reading: ");
    // Serial.println(reading);

    // Serial.print("readingTarget: ");
    // Serial.println(readingTarget);

    // Serial.print("readingUnit: ");
    // Serial.println(readingUnit);

    // Serial.print("delta: ");
    // Serial.println(delta);

    // Serial.print("payload: ");
    // Serial.println(payload);

    // Serial.print("briStart: ");
    // Serial.println(briStart);

    // Serial.print("briEnd: ");
    // Serial.println(briEnd);

    // Serial.print("currentBrightness: ");
    // Serial.println(currentBrightness);

    // Serial.print("currentBrightnessInt: ");
    // Serial.println(currentBrightnessInt);

    if(delta > readingUnit) { // if the change is greater than one unit of adjustment
      if(reading < readingTarget) {
        // turn lights up
        newBrightness += brightnessUnit;
      }

      if(reading > readingTarget) {
        // turn lights down
        newBrightness -= brightnessUnit;
      }
    }

    if(newBrightness >= (int(brightnessMax * .5))) {
      // don't let it get too bright
      newBrightness = currentBrightnessInt;
    }

    // Serial.print("newBrightness: ");
    // Serial.println(newBrightness);

    if(newBrightness != currentBrightnessInt) {
      if(newBrightness <= 1) {
        // if the lights should be turned off at this point
        for(String light: bedroomLights) {
          setLightState(false, light);
        }
      } else {
        // make sure the lights are on first
        for(String light: bedroomLights) {
          setLightState(true, light);
        }
        setGroupBrightness(newBrightness, bedroomID);
      }
    }

    // Serial.println("*****");

    photocellCount = 0;
  }
}

int readingToBrightness(int reading) {
  float brightness = reading / 1023.0 * 254.0;
  if(brightness > 254.0) { brightness = 254.0; }
  if(brightness < 0.0) { brightness = 0.0; }

  return brightness;
}

int getPhotocellReading() {
  return analogRead(PHOTOCELL);
}

void toggleMode() {
  if(currentMode == NIGHT_MODE) {
    setDayMode();
  } else {
    setNightMode();
  }

  // Serial.println();
  // Serial.println("toggleMode()");
  // Serial.println(currentMode);
  // Serial.println();
}

void setNightMode() {
  currentMode = NIGHT_MODE;
  setLEDBrightness(ledBrightness);
}

void setDayMode() {
  currentMode = DAY_MODE;
  photocellCount = PHOTOCELLDELAY;
  setLEDBrightness(0);
}

void toggleLED() {
  if(ledStateOld==false) {
    setLEDBrightness(ledBrightness);
    ledStateOld = true;
  } else {
    setLEDBrightness(0);
    ledStateOld = false;
  }
}

void setLEDBrightness(float percent) {
  analogWrite(LED, (1 - percent) * 255);
}

String getLightState(String lightID) {
  // Serial.println("getLightState()");
  // bool state = true;
  String payload;

  if(WiFi.status()== WL_CONNECTED){
    String url = BASE_URL + "lights/" + lightID;

    // Serial.print("url: ");
    // Serial.println(url);

    payload = httpGet(url);
    // int statusStart = payload.indexOf("\"on\":");
    // int statusEnd = payload.indexOf(",");
    // String status = payload.substring(statusStart + 5, statusEnd);

    // if(status == "false") {
    //   state = false;
    // }
  }
  else {
    // Serial.println("WiFi Disconnected");
  }

    // return state;
    return payload;
}

String getGroupState(String groupID) {
  String payload;

  if(WiFi.status()== WL_CONNECTED){
    String url = BASE_URL + "groups/" + groupID;
    payload = httpGet(url);
  }
  else {
    // Serial.println("WiFi Disconnected");
  }

    return payload;
}

void setLightState(bool newState, String lightID) {
  // Serial.println("setLightState()");
  if(WiFi.status()== WL_CONNECTED){

    String url = BASE_URL + "lights/" + lightID + "/state";

    // Serial.print("url: ");
    // Serial.println(url);

    String httpRequestData = "{\"on\":false}";
    if(newState == true) {
      httpRequestData = "{\"on\":true}";
    }

    httpPut(url, httpRequestData);
  }
  else {
    // Serial.println("WiFi Disconnected");
  }
}

void setLightBrightness(long brightness, String lightID) {
  if(WiFi.status()== WL_CONNECTED){

      String url = BASE_URL + "lights/" + lightID + "/state";

      String httpRequestData = String("{\"bri\":") + String(brightness) + String("}");
      // Serial.print("httpRequestData: ");
      // Serial.println(httpRequestData);
      
      httpPut(url, httpRequestData);
    }
    else {
      // Serial.println("WiFi Disconnected");
    }
}

void setGroupState(bool newState, String groupID) {
  Serial.println("setGroupState()");
  if(WiFi.status()== WL_CONNECTED){

    String url = BASE_URL + "groups/" + groupID + "/action";

    Serial.print("url: ");
    Serial.println(url);

    String httpRequestData = "{\"on\":false}";
    if(newState == true) {
      httpRequestData = "{\"on\":true}";
    }

    httpPut(url, httpRequestData);
  }
  else {
    // Serial.println("WiFi Disconnected");
  }
}

void setGroupBrightness(long brightness, String groupID) {
  if(WiFi.status()== WL_CONNECTED){

      String url = BASE_URL + "groups/" + groupID + "/action";

      String httpRequestData = String("{\"bri\":") + String(brightness) + String("}");
      // Serial.print("httpRequestData: ");
      // Serial.println(httpRequestData);
      
      httpPut(url, httpRequestData);
    }
    else {
      // Serial.println("WiFi Disconnected");
    }
}

String httpGet(String url) {

  WiFiClient client;
  HTTPClient http;
  String payload;

  // Your Domain name with URL path or IP address with path
  http.begin(client, url.c_str());

  // If you need Node-RED/server authentication, insert user and password below
  //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");
    
  // Send HTTP GET request
  int httpResponseCode = http.GET();
  
  if (httpResponseCode>0) {
    // Serial.print("HTTP Response code: ");
    // Serial.println(httpResponseCode);

    payload = http.getString();
    int statusStart = payload.indexOf("\"on\":");
    int statusEnd = payload.indexOf(",");
    String status = payload.substring(statusStart + 5, statusEnd);

    if(status == "false") {
      state = false;
    }
  } else {
    // Serial.print("Error code: ");
    // Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

int httpPut(String url, String httpRequestData) {
  WiFiClient client;
  HTTPClient http;

  // Your Domain name with URL path or IP address with path
  http.begin(client, url.c_str());
  http.addHeader("Content-Type", "application/json");

  // If you need Node-RED/server authentication, insert user and password below
  //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");
    
  // Send HTTP PUT request
  int httpResponseCode = http.PUT(httpRequestData);
  
  if (httpResponseCode>0) {
    // Serial.print("HTTP Response code: ");
    // Serial.println(httpResponseCode);

    String payload = http.getString();
    // Serial.println(payload);
  } else {
    // Serial.print("Error code: ");
    // Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
  delayMicroseconds(10); 

  return httpResponseCode;
}
