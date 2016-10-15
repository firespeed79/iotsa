#include "iotsaUser.h"
#include "iotsaConfigFile.h"

#define PASSWORD_DEBUGGING	// Enables admin1/admin1 to always log in

String &defaultPassword() {
  static String dftPwd;
  if (dftPwd == "") {
	  randomSeed(ESP.getChipId());
	  dftPwd = String("password") + String(random(1000));
  }
  return dftPwd;
}

IotsaUserMod::IotsaUserMod(IotsaApplication &_app, const char *_username, const char *_password)
:	username(_username),
	password(_password),
	IotsaAuthMod(_app)
{
	configLoad();
}
	
void
IotsaUserMod::handler() {
  bool anyChanged = false;
  String pwold, pw1, pw2;
  bool passwordChanged = false;
  
  LED digitalWrite(led, 1);
  for (uint8_t i=0; i<server.args(); i++){
    if( server.argName(i) == "username") {
    	if (needsAuthentication()) return;
    	username = server.arg(i);
    	anyChanged = true;
    }
    if( server.argName(i) == "old") {
    	// password authentication is checked later.
    	pwold = server.arg(i);
    }
    if( server.argName(i) == "password") {
    	// password authentication is checked later.
    	pw1 = server.arg(i);
    	passwordChanged = true;
    }
    if( server.argName(i) == "again") {
    	pw1 = server.arg(i);
    	passwordChanged = true;
    }
  }
  if (passwordChanged && pw1 == pw2) {
  	if (password == pwold && password != pw1) {
    	if (needsAuthentication()) return;
		password = pw1;
		anyChanged = true;
	} else {
		anyChanged = false;
		passwordChanged = false;
	}

  }
  if (anyChanged) configSave();
  String message = "<html><head><title>Edit users and passwords</title></head><body><h1>Edit users and passwords</h1>";
  if (passwordChanged && !anyChanged) {
  	message += "<p><em>Passwords do not match, not changed.</em></p>";
  } else if (passwordChanged) {
  	message += "<p><em>Password has been changed.</em></p>";
}
  	
  message += "<form method='get'>Username: <input name='username' value='";
  message += username;
  message += "'>";
  if (password) {
  	message += "<br>Old Password: <input type='password' name='old' value=''";
  	message += "'>";
  } else if (configurationMode == TMPC_CONFIG) {
  	message += "<br>Password not set, default is '";
  	message += defaultPassword();
  	message += "'.";
  } else {
  	message += "<br>Password not set, reboot in configuration mode to see default password.";
  }
  message += "<br>New Password: <input type='password' name='password' value='";
  message += password;
  message += "'><br>Repeat New Password: <input type='password' name='again' value='";
  message += password;
  message += "'><br><input type='submit'></form>";
  server.send(200, "text/html", message);
  LED digitalWrite(led, 0);
}

void IotsaUserMod::setup() {
  configLoad();
}

void IotsaUserMod::serverSetup() {
  server.on("/users", std::bind(&IotsaUserMod::handler, this));
}

void IotsaUserMod::configLoad() {
  IotsaConfigFileLoad cf("/config/users.cfg");
  cf.get("argument", username, "");
 
}

void IotsaUserMod::configSave() {
  IotsaConfigFileSave cf("/config/users.cfg");
  cf.put("user0", username);
  cf.put("password0", password);
}

void IotsaUserMod::loop() {
}

String IotsaUserMod::info() {
  String message = "<p>Usernames/passwords enabled.";
  message += " See <a href=\"/users\">/users</a> to change.";
  if (configurationMode && password == "") {
  	message += "<br>Username and password are the defaults: ";
  	message += username;
  	message += "and ";
  	String &dfp = defaultPassword();
  	message += dfp;
  	message += ".";
  }
  message += "</p>";
  return message;
}

bool IotsaUserMod::needsAuthentication() {
  String &curPassword = password;
  if (curPassword == "")
  	curPassword = defaultPassword();
  if (!server.authenticate(username.c_str(), curPassword.c_str())
#ifdef PASSWORD_DEBUGGING
	  && !server.authenticate("admin1", "admin1")
#endif
  	) {
  	server.requestAuthentication();
  	return true;
  }
  return false;
}