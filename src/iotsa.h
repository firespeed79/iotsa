#ifndef _IOTSA_H_
#define _IOTSA_H_

#include "iotsaVersion.h"
#include "iotsaBuildOptions.h"
#include <Print.h>

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#ifdef IOTSA_WITH_HTTP_OR_HTTPS
#ifdef ESP32
#include <ESP32WebServer.h>
typedef ESP32WebServer IotsaWebServer;
#else
#include <ESP8266WebServer.h>
typedef ESP8266WebServer IotsaWebServer;
#endif
#endif

//
// Global defines, changes some behaviour in the whole library
//
#ifdef IOTSA_WITH_DEBUG
#define IFDEBUG if(1)
#else
#define IFDEBUG if(0)
#endif

#define CONFIGURATION_MODE_TIMEOUT 300  // How long to go to temp configuration mode at reboot

// Magic to allow logging to be kept in-core, if wanted, by using
// IotsaSerial in stead of Serial.
extern Print *iotsaOverrideSerial;
#define IotsaSerial (*iotsaOverrideSerial)

class IotsaBaseMod;
class IotsaConfigMod;

//
// Operations allowed via the API
//
typedef enum IotsaApiOperation {
  IOTSA_API_GET,
  IOTSA_API_PUT,
  IOTSA_API_POST,
  IOTSA_API_DELETE
} IotsaApiOperation;

//
// Status indication interface.
//
class IotsaStatusInterface {
public:
  virtual void showStatus() = 0;
};

class IotsaApplication {
friend class IotsaBaseMod;
friend class IotsaConfigMod;
public:
  IotsaApplication(const char *_title);
  // Explicitly disable copy constructor and assignment
  IotsaApplication(const IotsaApplication& that) = delete;
  IotsaApplication& operator=(const IotsaApplication& that) = delete;

  void addMod(IotsaBaseMod *mod);
  void addModEarly(IotsaBaseMod *mod);
  void setup();
  void serverSetup();
  void loop();
  void enableOta() { haveOTA = true; }
  bool otaEnabled() { return haveOTA; }
  IotsaStatusInterface *status;
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  IotsaWebServer *server;
#endif
protected:
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  void webServerSetup();
  void webServerLoop();
  void webServerNotFoundHandler();
#endif
#ifdef IOTSA_WITH_WEB
  void webServerRootHandler();
#endif
  IotsaBaseMod *firstModule;
  IotsaBaseMod *firstEarlyModule;
  String title;
  bool haveOTA;
};

class IotsaAuthMod;

class IotsaAuthenticationProvider {
public:
  IotsaAuthenticationProvider() {}
  IotsaAuthenticationProvider(const IotsaAuthenticationProvider& that) = delete;
  IotsaAuthenticationProvider& operator=(const IotsaAuthenticationProvider& that) = delete;

  virtual ~IotsaAuthenticationProvider() {}
  virtual bool allows(const char *right=NULL) = 0;
  virtual bool allows(const char *obj, IotsaApiOperation verb) = 0;
};

class IotsaBaseMod {
friend class IotsaApplication;
friend class IotsaConfigMod;
public:
  IotsaBaseMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
  : app(_app), 
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  	server(_app.server), 
#endif
  	auth(_auth), 
  	nextModule(NULL)
  {
    if (early) {
      app.addModEarly(this);
    } else {
      app.addMod(this);
    }
  }
  IotsaBaseMod& operator=(const IotsaBaseMod& that) = delete;

  virtual void setup() = 0;
  virtual void loop() = 0;
#ifdef IOTSA_WITH_WEB
  virtual String info();
#endif
  virtual void serverSetup();
  virtual bool needsAuthentication(const char *right=NULL);
  virtual bool needsAuthentication(const char *obj, IotsaApiOperation verb);

protected:
  IotsaApplication &app;
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  IotsaWebServer *server;
#endif
  IotsaAuthenticationProvider *auth;
  IotsaBaseMod *nextModule;
  String name;
};

class IotsaMod : public IotsaBaseMod {
public:
  IotsaMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
  : IotsaBaseMod(_app, _auth, early)
  {
  }
  IotsaMod& operator=(const IotsaMod& that) = delete;
  virtual void serverSetup() = 0;

#ifdef IOTSA_WITH_WEB
  virtual String info() = 0;
  static String htmlEncode(String data); // Helper - convert strings to HTML-safe representation
#endif

protected:
};

class IotsaAuthMod : public IotsaMod, public IotsaAuthenticationProvider {
public:
  using IotsaMod::IotsaMod;	// Inherit constructor
};

typedef enum { IOTSA_MODE_NORMAL, IOTSA_MODE_CONFIG, IOTSA_MODE_OTA, IOTSA_MODE_FACTORY_RESET } config_mode;

class IotsaConfig {
public:
  bool wifiPrivateNetworkMode;
  config_mode configurationMode;
  unsigned long configurationModeEndTime;
  config_mode nextConfigurationMode;
  unsigned long nextConfigurationModeEndTime;
  String hostName;
  int configurationModeTimeout;

  bool inConfigurationMode() { return configurationMode == IOTSA_MODE_CONFIG; }
  uint32_t getStatusColor() {
    if (!WiFi.isConnected()) return 0x3f1f00; // Orange: not connected to WiFi
    if (configurationMode == IOTSA_MODE_FACTORY_RESET) return 0x3f0000; // Red: Factory reset mode
    if (configurationMode == IOTSA_MODE_CONFIG) return 0x3f003f;	// Magenta: user-requested configuration mode
    if (configurationMode == IOTSA_MODE_OTA) return 0x003f3f;	// Cyan: OTA mode
    if (wifiPrivateNetworkMode) return 0x3f3f00; // Yellow: configuration mode (not user requested)
    return 0; // Off: all ok.
  }
};

#define WITH_IOTSA_CONFIG_STRUCT

extern IotsaConfig iotsaConfig;
extern String& hostName;
#endif
