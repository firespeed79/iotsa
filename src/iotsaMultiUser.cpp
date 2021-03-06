#include "iotsaMultiUser.h"
#include "iotsaConfigFile.h"

IotsaMultiUserMod::IotsaMultiUserMod(IotsaApplication &_app)
:	IotsaAuthMod(_app),
  users(NULL)
#ifdef IOTSA_WITH_API
  ,
  api(this, _app, this)
#endif
{
	configLoad();
}

#ifdef IOTSA_WITH_WEB
void
IotsaMultiUserMod::handler() {
  String command = server->arg("command");
  if (command == "") {
  
    String message = "<html><head><title>Edit users</title></head><body><h1>Edit users</h1>";
    message += "<table><tr><th>User</th><th>rights</th><th>New password<br>New rights</th></tr>";
    IotsaUser *u;
    for (u=users; u; u=u->next) {
      message += "<tr><td>";
      message += htmlEncode(u->username);
      message += "</td><td>";
      message += htmlEncode(u->rights);
      message += "</td><td><form method='get'>";
      message += "<form method='get'>";
      message += "<input type='hidden' name='command' value='add'>";
      message += "<input type='hidden' name='username' value='";
      message += htmlEncode(u->username);
      message += "'>";
      message += "<input name='password' type='password'><br>";
      message += "<input name='rights'><br>";
      message += "<input type='submit' value='Change'>";
      message += "</form></td></tr>";
    }
    message += "</table><br><hr>";

    message += "<form method='get'>";
    message += "<input type='hidden' name='command' value='add'>";
    message += "Username: <input name='username'><br>";
    message += "Password: <input name='password' type='password'><br>";
    message += "Rights: <input name='rights'><br>";
    message += "<input type='submit' value='Add'>";
    message += "</form><hr>";

    server->send(200, "text/html", message);
    return;
  }

  if (needsAuthentication("users")) return;

  if (command == "add") {
    IotsaUser *u = new IotsaUser();
    u->username = server->arg("username");
    u->password = server->arg("password");
    u->rights = server->arg("rights");
    if (u->username != "") {
      IotsaUser **up = &users;
      int count;
      for(count=0; *up; count++, up = &(*up)->next);
      u->apiEndpoint = String("/api/users/")+String(count);
      *up = u;
      configSave();
#ifdef IOTSA_WITH_API
      api.setup(u->apiEndpoint.c_str(), true, true);
#endif
    }
    server->send(200, "text/plain", "OK\r\n");
    return; 
  } else
  if (command == "change") {
    String username = server->arg("username");
    for(IotsaUser *u=users; u; u=u->next) {
      if (u->username == username) {
        String a = server->arg("password");
        if (a != "") u->password = a;
        a = server->arg("rights");
        if (a != "") u->rights = a;
        configSave();
        server->send(200, "text/plain", "OK\r\n");
        return; 
      }
    }
    server->send(404, "text/plain", "No such user\r\n");
    return;
  }
  server->send(400, "text/plain", "Unknown command");
}

String IotsaMultiUserMod::info() {
  String message = "<p>Multiple Usernames/passwords/rights enabled.";
  message += " See <a href=\"/users\">/users</a> to change.";
  message += "</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

#ifdef IOTSA_WITH_API
bool IotsaMultiUserMod::getHandler(const char *path, JsonObject& reply) {
  if (strcmp(path, "/api/users") == 0) {
    reply["multi"] = true;
    JsonArray usersList = reply.createNestedArray("users");
    for (IotsaUser *u=users; u; u=u->next) {
      JsonObject user = usersList.createNestedObject();
      user["username"] = u->username;
      bool hasPassword = u->password.length() > 0;
      user["has_password"] = hasPassword;
      user["rights"] = u->rights;
    }
    return true;
  }
  for (IotsaUser *u=users; u; u=u->next) {
    if (strcmp(u->apiEndpoint.c_str(), path) == 0) {
      reply["username"] = u->username;
      bool hasPassword = u->password.length() > 0;
      reply["has_password"] = hasPassword;
      reply["rights"] = u->rights;
      return true;
    }
  }
  return false;
}

bool IotsaMultiUserMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (strncmp(path, "/api/users/", 11) != 0) return false;
  if (!iotsaConfig.inConfigurationMode()) return false;
  String num(path);
  num.remove(0, 11);
  int idx = num.toInt();

  bool anyChanged = false;
  IotsaUser *u = users;
  while (u && idx > 0) { u = u->next; idx--; }
  if (u == NULL) return false;
  JsonObject reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("username")) {
    u->username = reqObj["username"].as<String>();
    anyChanged = true;
  }
  if (reqObj.containsKey("password")) {
    u->password = reqObj["password"].as<String>();
    anyChanged = true;
  }
  if (reqObj.containsKey("rights")) {
    u->rights = reqObj["rights"].as<String>();
    anyChanged = true;
  }
  if (anyChanged) {
    configSave();
  }
  return anyChanged;
}
bool IotsaMultiUserMod::postHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (strcmp(path, "/api/users") != 0) return false;
  if (!iotsaConfig.inConfigurationMode()) return false;
  bool anyChanged = false;
  IotsaUser *u = new IotsaUser();
  JsonObject reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("username")) {
    u->username = reqObj["username"].as<String>();
    anyChanged = true;
  }
  if (reqObj.containsKey("password")) {
    u->password = reqObj["password"].as<String>();
    anyChanged = true;
  }
  if (reqObj.containsKey("rights")) {
    u->rights = reqObj["rights"].as<String>();
    anyChanged = true;
  }
  if (anyChanged) {
    IotsaUser **up = &users;
    int count;
    for(count=0; *up; count++, up = &(*up)->next);
    u->apiEndpoint = String("/api/users/")+String(count);
    *up = u;
    configSave();
    api.setup(u->apiEndpoint.c_str());
  }
  return anyChanged;
}
#endif // IOTSA_WITH_API

void IotsaMultiUserMod::setup() {
  configLoad();
}

void IotsaMultiUserMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/users", std::bind(&IotsaMultiUserMod::handler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/users", true, false, true);
  name = "users";
  IotsaUser *u = users;
  while(u) {
    api.setup(u->apiEndpoint.c_str(), true, true, false);
  }
#endif // IOTSA_WITH_API
}

void IotsaMultiUserMod::configLoad() {
  IotsaConfigFileLoad cf("/config/users.cfg");
  // xxxjack should really free old users...
  IotsaUser **up = &users;
  for(int cnt=0; ; cnt++) {
    String username;
    String arg = "user" + String(cnt);
    cf.get(arg, username, "");
    if (username == "") break;

    *up = new IotsaUser();
    (*up)->username = username;
    (*up)->apiEndpoint = "/api/users/" + String(cnt); 
    arg = "password" + String(cnt);
    cf.get(arg, (*up)->password, "");
    arg = "rights" + String(cnt);
    cf.get(arg, (*up)->rights, "");

    IotsaSerial.print("Username=");
    IotsaSerial.print((*up)->username);
    IotsaSerial.print(", password length=");
    IotsaSerial.println((*up)->password.length());
    IotsaSerial.print("Rights=");
    IotsaSerial.println((*up)->rights);

    up = &(*up)->next;
  }
}

void IotsaMultiUserMod::configSave() {
  IotsaConfigFileSave cf("/config/users.cfg");
  IotsaUser *u = users;
  int cnt;
  for(cnt=0; u; cnt++, u=u->next) {
    String arg = "user" + String(cnt);
    cf.put(arg, u->username);
    arg = "password" + String(cnt);
    cf.put(arg, u->password);
    arg = "rights" + String(cnt);
    cf.put(arg, u->rights);
  }
  IotsaSerial.print("Saved users.cfg, #users=");
  IotsaSerial.println(cnt);
}

void IotsaMultiUserMod::loop() {
}

bool IotsaMultiUserMod::allows(const char *right) {
  // No users means everything is allowed.
  if (users == NULL) return true;
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  // Otherwise we loop over all users until we find one that matches.
  IotsaUser *u = users;
  while (u) {
    if (server->authenticate(u->username.c_str(), u->password.c_str())) {
      String rightField("/");
      rightField += right;
      rightField += "/";
      if (u->rights == "*" || u->rights.indexOf(rightField) >= 0) {
        return true;
      }
      break;
    }
    u = u->next;
  }
  server->requestAuthentication();
  IotsaSerial.print("Return 401 Unauthorized for right=");
  IotsaSerial.println(right);

#endif // WITH_HTTP_OR_HTTPS
  return false;
}

bool IotsaMultiUserMod::allows(const char *obj, IotsaApiOperation verb) {
  return allows("api");
}