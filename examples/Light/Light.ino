//
// Boilerplate for configurable web server (probably RESTful) running on ESP8266.
//
// This server includes the wifi configuration module, and optionally the
// Over-The-Air update module (to allow uploading new code into the esp12 (or other
// board) from the Arduino IDE.
//
// A "light" module is added, which reads the light level with an LDR. Connect the
// LDR between ADC and 3.3v, with a pulldown resistor to earth (2k2 seemes to work
// well for the LDR I had. Use a value that gets close to zero readings for darkness,
// and needs a pretty bright light to make it go to the maximum value of 1024). 
//

#include <ESP.h>
#include "iotsa.h"
#include "iotsaWifi.h"
#include <DHT.h>

// CHANGE: Add application includes and declarations here

#define DHT_PIN 13
#define DHT_TYPE DHT21

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

ESP8266WebServer server(80);
IotsaApplication application(server, "Iotsa Light Server");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

//
// Light module. Gets light data from an LDR connected to an analog input. Normalizes it.
//

// Declaration of the Light module
#define DECAY 200 // The bigger this number the slower the decay. 200 is a few seconds.

class IotsaLightMod : public IotsaMod {
public:
  IotsaLightMod(IotsaApplication &_app) 
  : IotsaMod(_app),
  light(0),
  minLight(0xffff),
  maxLight(0)
  {}
	void setup();
	void serverSetup();
	void loop();
  String info();
private:
  void handler();
  void _update();
  unsigned int light;
  unsigned int minLight;
  unsigned int maxLight;
  float lightLevel;
};


void IotsaLightMod::_update() {
  light = analogRead(A0);
  if (light < minLight) minLight = light;
  if (light > maxLight) maxLight = light;
  if (minLight == maxLight) {
    lightLevel = 0.5;
  } else {
    float newLightLevel = (float)(light-minLight)/(maxLight-minLight);
    lightLevel = ((DECAY-1)*lightLevel+newLightLevel)/DECAY;
  }
  Serial.print("light ");
  Serial.print(light);
  Serial.print(" min ");
  Serial.print(minLight);
  Serial.print(" max ");
  Serial.print(maxLight);
  Serial.print(" level ");
  Serial.println(lightLevel);
}

// Implementation of the Light module
void IotsaLightMod::setup() {
	// Nothing to do during early initialization for this module
}

void
IotsaLightMod::handler() {
  // Handles the page that is specific to the Light module, greets the user and
  // optionally stores a new name to greet the next time.
  String message = "{\"light\":";
  message += String(light);
  message += ",\"minLight\":";
  message += String(minLight);
  message += ",\"maxLight\":";
  message += String(maxLight);
  message += ",\"lightLevel\":";
  message += String(lightLevel);
  message += "}\n";
  server.send(200, "application/json", message);
}

void IotsaLightMod::serverSetup() {
  // Setup the web server hooks for this module.
  server.on("/light", std::bind(&IotsaLightMod::handler, this));
}

String IotsaLightMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>Lightlevel is ";
  rv += String((int)(lightLevel*100));
  rv += "%. See <a href=\"/light\">/light</a> for JSON data.</p>";
  return rv;
}

void IotsaLightMod::loop() {
  _update();
}

// Instantiate the Light module, and install it in the framework
IotsaLightMod lightMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
  ESP.wdtEnable(WDTO_120MS);
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}

