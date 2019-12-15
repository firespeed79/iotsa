#ifndef _IOTSAWIFI_H_
#define _IOTSAWIFI_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaConfigMod.h"

#ifdef IOTSA_WITH_WIFI
#ifdef IOTSA_WITH_API
#define IotsaWifiModBaseMod IotsaApiMod
#else
#define IotsaWifiModBaseMod IotsaMod
#endif

class IotsaWifiMod : public IotsaWifiModBaseMod {
public:
  IotsaWifiMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL) 
  : IotsaWifiModBaseMod(_app, _auth, true),
    configMod(_app, _auth)
  {}
	void setup();
	void serverSetup();
	void loop();
  String info();
protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
private:
  void configLoad();
  void configSave();
  void handler();

  String ssid;
  String ssidPassword;
  bool haveMDNS;
  IotsaConfigMod configMod;
};
#elif IOTSA_WITH_PLACEHOLDERS
class IotsaWifiMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
  void setup() {}
  void serverSetup() {}
  void loop() {}
  String info() {return "";}
};
#endif // IOTSA_WITH_WIFI || IOTSA_WITH_PLACEHOLDERS

#endif
