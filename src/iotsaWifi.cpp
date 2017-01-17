#include <ESP.h>
#include <ESP8266mDNS.h>
#include <FS.h>

#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaWifi.h"

#define WIFI_TIMEOUT 30                  // How long to wait for our WiFi network to appear

//
// Global variables, because other modules need them too.
//
String hostName;  // mDNS hostname, and also used for AP network name if needed.
bool configurationMode;        // True if we have no config, and go into AP mode
int rebootConfigTimeout;		// Timeout (in seconds) for rebooting in configuration mode
config_mode  tempConfigurationMode;    // Current configuration mode (i.e. after a power cycle)
unsigned long tempConfigurationModeTimeout;  // When we reboot out of current configuration mode
int tempConfigurationModeReason;
config_mode  nextConfigurationMode;    // Next configuration mode (i.e. before a power cycle)
unsigned long nextConfigurationModeTimeout;  // When we abort nextConfigurationMode and revert to normal operation



static void wifiDefaultHostName() {
  hostName = "esp8266-";
  hostName += String(ESP.getChipId());
}

void IotsaWifiMod::setup() {
  // Load configuration parameters, and clear any temporary configuration mode (if requested)
  tempConfigurationMode = nextConfigurationMode = TMPC_NORMAL;
  tempConfigurationModeTimeout = nextConfigurationModeTimeout = 0;
  tempConfigurationModeReason = 0;
  configLoad();
  if (tempConfigurationMode) {
  	IFDEBUG Serial.println("tmpConfigMode, re-saving wifi.cfg without it");
  	configSave();
  }
  // If factory reset is requested format the Flash and reboot
  if (tempConfigurationMode == TMPC_RESET) {
  	IFDEBUG Serial.println("Factory-reset requested");
  	delay(1000);
  	IFDEBUG Serial.println("Formatting SPIFFS...");
  	SPIFFS.format();
  	IFDEBUG Serial.println("Format done, rebooting.");
  	delay(2000);
  	ESP.restart();
  }
  // Try and connect to an existing Wifi network, if known and not in configuration mode
  if (ssid.length() && tempConfigurationMode != TMPC_CONFIG) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), ssidPassword.c_str());
    IFDEBUG Serial.println("");
  
    // Wait for connection
    int count = WIFI_TIMEOUT;
    while (WiFi.status() != WL_CONNECTED && count > 0) {
      delay(1000);
      IFDEBUG Serial.print(".");
      count--;
    }
    if (count) {
      // Connection to WiFi network succeeded.
      IFDEBUG Serial.println("");
      IFDEBUG Serial.print("Connected to ");
      IFDEBUG Serial.println(ssid);
      IFDEBUG Serial.print("IP address: ");
      IFDEBUG Serial.println(WiFi.localIP());
      IFDEBUG Serial.print("Hostname ");
      IFDEBUG Serial.println(hostName);
      
      WiFi.setAutoReconnect(true);

      if (MDNS.begin(hostName.c_str())) {
        MDNS.addService("http", "tcp", 80);
        IFDEBUG Serial.println("MDNS responder started");
        haveMDNS = true;
      }
      return;
    }
    tempConfigurationMode = TMPC_CONFIG;
    tempConfigurationModeReason = WiFi.status();
    IFDEBUG Serial.print("Cannot join ");
    IFDEBUG Serial.print(ssid);
    IFDEBUG Serial.print("status=");
    IFDEBUG Serial.println(tempConfigurationModeReason);
  }
  
  // Connection to WiFi network failed, or we are in (temp) cofiguration mode. Setup our own network.
  if (tempConfigurationMode) {
    tempConfigurationModeTimeout = millis() + 1000*rebootConfigTimeout;
  	IFDEBUG Serial.print("tempConfigMode=");
  	IFDEBUG Serial.print((int)tempConfigurationMode);
  	IFDEBUG Serial.print(", timeout at ");
  	IFDEBUG Serial.println(tempConfigurationModeTimeout);
  }
  configurationMode = true;
  String networkName = "config-" + hostName;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(networkName.c_str());
  IFDEBUG Serial.print("\nCreating softAP for network ");
  IFDEBUG Serial.println(networkName);
  IFDEBUG Serial.print("IP address: ");
  IFDEBUG Serial.println(WiFi.softAPIP());
#if 0
  // Despite reports to the contrary it seems mDNS isn't working in softAP mode
  if (MDNS.begin(hostName.c_str())) {
    MDNS.addService("http", "tcp", 80);
    IFDEBUG Serial.println("MDNS responder started");
    haveMDNS = true;
  }
#endif
}

void
IotsaWifiMod::handlerConfigMode() {
  bool anyChanged = false;
  for (uint8_t i=0; i<server.args(); i++){
    if( server.argName(i) == "ssid") {
      ssid = server.arg(i);
      anyChanged = true;
    }
    if( server.argName(i) == "ssidPassword") {
      ssidPassword = server.arg(i);
      anyChanged = true;
    }
    if( server.argName(i) == "hostName") {
      hostName = server.arg(i);
      anyChanged = true;
    }
    if( server.argName(i) == "rebootTimeout") {
      rebootConfigTimeout = server.arg(i).toInt();
      anyChanged = true;
    }
    if (anyChanged) {
    	nextConfigurationMode = TMPC_NORMAL;
    	configSave();
	}
  }
  String message = "<html><head><title>WiFi configuration</title></head><body><h1>WiFi configuration</h1>";
  if (anyChanged) {
    message += "<p>Settings saved to EEPROM. <em>Rebooting device to activate new settings.</em></p>";
  }
  message += "<form method='get'>Network: <input name='ssid' value='";
  message += ssid;
  message += "'><br>Password: <input name='ssidPassword' value='";
  message += ssidPassword;
  message += "'><br>Hostname: <input name='hostName' value='";
  message += hostName;
  message += "'><br>Configuration mode timeout: <input name='rebootTimeout' value='";
  message += String(rebootConfigTimeout);
  message += "'><br><input type='submit'></form>";
  message += "</body></html>";
  server.send(200, "text/html", message);
  if (anyChanged) {
    IFDEBUG Serial.print("Restart in 2 seconds");
    delay(2000);
    ESP.restart();
  }
}

void
IotsaWifiMod::handlerNormalMode() {
  bool anyChanged = false;
  bool factoryReset = false;
  bool iamsure = false;
  for (uint8_t i=0; i<server.args(); i++) {
    if( server.argName(i) == "config") {
    	if (needsAuthentication()) return;
      	nextConfigurationMode = config_mode(atoi(server.arg(i).c_str()));
      	nextConfigurationModeTimeout = millis() + rebootConfigTimeout*1000;
      	anyChanged = true;
    }
    if( server.argName(i) == "factoryreset" && atoi(server.arg(i).c_str()) == 1) {
    	// Note: factoryReset does NOT require authenticationso users have a way to reclaim
    	// hardware for which they have lost the username/password. The device will, however,
    	// be reset to factory settings, so no information can be leaked.
    	factoryReset = true;
    	anyChanged = true;
	}
    if( server.argName(i) == "iamsure" && atoi(server.arg(i).c_str()) == 1) {
    	// Note: does not set anyChanged, so only has a function if factoryReset is also set
    	iamsure = true;
	}
  }
  if (factoryReset && iamsure) {
  	nextConfigurationMode = TMPC_RESET;
	nextConfigurationModeTimeout = millis() + rebootConfigTimeout*1000;
  }
  if (anyChanged) configSave();
  String message = "<html><head><title>WiFi configuration</title></head><body><h1>WiFi configuration</h1>";
  if (nextConfigurationMode == TMPC_CONFIG) {
  	message += "<p><em>Power-cycle device within " + String((nextConfigurationModeTimeout-millis())/1000) + " seconds to activate configuration mode for " + String(rebootConfigTimeout) + " seconds.</em></p>";
  	message += "<p><em>Connect to WiFi network config-" + hostName + ", device 192.168.4.1 to change settings during that configuration period. </em></p>";
  } else if (nextConfigurationMode == TMPC_OTA) {
  	message += "<p><em>Power-cycle device within " + String((nextConfigurationModeTimeout-millis())/1000) + "seconds to activate OTA mode for " + String(rebootConfigTimeout) + " seconds.</em></p>";
  } else if (nextConfigurationMode == TMPC_RESET) {
  	message += "<p><em>Power-cycle device within " + String((nextConfigurationModeTimeout-millis())/1000) + "seconds to reset to factory settings.</em></p>";
  }
  message += "<form method='get'><input name='config' type='checkbox' value='1'> Enter configuration mode after next reboot.<br>";
  if (app.otaEnabled()) {
	  message += "<input name='config' type='checkbox' value='2'> Enable over-the-air update after next reboot.</br>";
  }
  message += "<input name='factoryreset' type='checkbox' value='1'> Factory-reset and clear all files. <input name='iamsure' type='checkbox' value='1'> Yes, I am sure.</br>";
  message += "<br><input type='submit'></form></body></html>";
  server.send(200, "text/html", message);
}

void IotsaWifiMod::serverSetup() {
//  server.on("/hello", std::bind(&IotsaWifiMod::handler, this));
  if (configurationMode) {
    server.on("/wificonfig", std::bind(&IotsaWifiMod::handlerConfigMode, this));
  } else {
    server.on("/wificonfig", std::bind(&IotsaWifiMod::handlerNormalMode, this));
  }
}

String IotsaWifiMod::info() {
  IPAddress x;
  String message = "<p>IP address is ";
  uint32_t ip = WiFi.localIP();
  message += String(ip&0xff) + "." + String((ip>>8)&0xff) + "." + String((ip>>16)&0xff) + "." + String((ip>>24)&0xff);
  if (haveMDNS) {
    message += ", hostname is ";
    message += hostName;
    message += ".local. ";
  } else {
    message += " (no mDNS, so no hostname). ";
  }
  message += "See <a href=\"/wificonfig\">/wificonfig</a> to change network parameters.</p>";
  if (tempConfigurationMode) {
  	message += "<p>In configuration mode " + String((int)tempConfigurationMode) + "(reason: " + String(tempConfigurationModeReason) + "), will timeout in " + String((tempConfigurationModeTimeout-millis())/1000) + " seconds.</p>";
  } else if (nextConfigurationMode) {
  	message += "<p>Configuration mode " + String((int)nextConfigurationMode) + " requested, enable by rebooting within " + String((nextConfigurationModeTimeout-millis())/1000) + " seconds.</p>";
  } else if (tempConfigurationModeTimeout) {
  	message += "<p>Strange, no configuration mode but timeout is " + String(tempConfigurationModeTimeout-millis()) + "ms.</p>";
  }
  return message;
}

void IotsaWifiMod::configLoad() {
  IotsaConfigFileLoad cf("/config/wifi.cfg");
  int tcm;
  cf.get("mode", tcm, (int)TMPC_NORMAL);
  tempConfigurationMode = (config_mode)tcm;
  cf.get("ssid", ssid, "");
  cf.get("ssidPassword", ssidPassword, "");
  cf.get("hostName", hostName, "");
  cf.get("rebootTimeout", rebootConfigTimeout, CONFIGURATION_MODE_TIMEOUT);
  if (hostName == "") wifiDefaultHostName();
 
}

void IotsaWifiMod::configSave() {
  IotsaConfigFileSave cf("/config/wifi.cfg");
  cf.put("mode", nextConfigurationMode); // Note: nextConfigurationMode, which will be read as tempConfigurationMode
  cf.put("ssid", ssid);
  cf.put("ssidPassword", ssidPassword);
  cf.put("hostName", hostName);
  cf.put("rebootTimeout", rebootConfigTimeout);
  IFDEBUG Serial.println("Saved wifi.cfg");
}

void IotsaWifiMod::loop() {
  if (tempConfigurationModeTimeout && millis() > tempConfigurationModeTimeout) {
    IFDEBUG Serial.println("Configuration mode timeout. reboot.");
    tempConfigurationMode = TMPC_NORMAL;
    tempConfigurationModeTimeout = 0;
    ESP.restart();
  }
  if (nextConfigurationModeTimeout && millis() > nextConfigurationModeTimeout) {
    IFDEBUG Serial.println("Next configuration mode timeout. Clearing.");
    nextConfigurationMode = TMPC_NORMAL;
    nextConfigurationModeTimeout = 0;
    configSave();
  }
  if (haveMDNS) MDNS.update();
  if (tempConfigurationMode == TMPC_NORMAL && !configurationMode) {
  	// Should be in normal mode, check that we have WiFi
  	static int disconnectedCount = 0;
  	if (WiFi.status() == WL_CONNECTED) {
  		if (disconnectedCount) {
  			IFDEBUG Serial.println("Wifi reconnected");
  		}
  		disconnectedCount = 0;
	} else {
		if (disconnectedCount) {
			IFDEBUG Serial.println("Wifi connection lost");
		}
		disconnectedCount++;
		if (disconnectedCount > 32000) {
			IFDEBUG Serial.println("Wifi connection lost too long. Reboot.");
			ESP.restart();
		}
	}
  }
}
