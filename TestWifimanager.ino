#include <FS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <MD5Builder.h>
#include <WiFiClient.h>
#include <ESP.h>
WiFiClient client;
WebServer Server(80);
HTTPClient httpGet;
HTTPClient httpPost;
HTTPClient http;

#define Gsense 3

const int buttonPin = 27;
int buttonState = LOW;
char nvr_ip[40] = "http://192.168.1.101";
char nvr_username[20] = "admin";
char nvr_password[20] = "Passw0rd";
bool shouldSaveConfig = false; //flag for saving data
String authReq;
String authorization;

char *username = "admin";
char *password = "Passw0rd";
const char *server = "http://192.168.1.101";
const char *uri = "/stw-cgi/system.cgi?msubmenu=eventlog&action=view";


//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


String exractParam(String& authReq, const String& param, const char delimit) {
  int _begin = authReq.indexOf(param);
  if (_begin == -1) {
    return "";
  }
  return authReq.substring(_begin + param.length(), authReq.indexOf(delimit, _begin + param.length()));
}


String getCNonce(const int len) {
  static const char alphanum[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";
  String s = "";

  for (int i = 0; i < len; ++i) {
    s += alphanum[rand() % (sizeof(alphanum) - 1)];
  }

  return s;
}


String getDigestAuth(String& authReq, const String& username, const String& password, const String& uri, unsigned int counter) {
  // extracting required parameters for RFC 2069 simpler Digest
  String realm = exractParam(authReq, "realm=\"", '"');
  String nonce = exractParam(authReq, "nonce=\"", '"');
  String cNonce = getCNonce(8);

  char nc[9];
  snprintf(nc, sizeof(nc), "%08x", counter);

  // parameters for the RFC 2617 newer Digest
  MD5Builder md5;
  md5.begin();
  md5.add(username + ":" + realm + ":" + password);  // md5 of the user:realm:user
  md5.calculate();
  String h1 = md5.toString();

  md5.begin();
  md5.add(String("GET:") + uri);
  md5.calculate();
  String h2 = md5.toString();

  md5.begin();
  md5.add(h1 + ":" + nonce + ":" + String(nc) + ":" + cNonce + ":" + "auth" + ":" + h2);
  md5.calculate();
  String response = md5.toString();

  String authorization = "Digest username=\"" + username + "\", realm=\"" + realm + "\", nonce=\"" + nonce +
                         "\", uri=\"" + uri + "\", algorithm=\"MD5\", qop=auth, nc=" + String(nc) + ", cnonce=\"" + cNonce + "\", response=\"" + response + "\"";
  Serial.println(authorization);

  return authorization;
}


String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}


void test() {
  if (Server.arg("action") == "1" && Server.arg("device_id") == "1") {
    digitalWrite(23, LOW);
    char content[] = "Turn on 1.";
    Server.send(200, "text/plain", content);
  }
  else if (Server.arg("action") == "2" && Server.arg("device_id") == "1") {
    digitalWrite(23, HIGH);
    char content[] = "Turn off 1.";
    Server.send(200, "text/plain", content);
  }
  else if (Server.arg("action") == "1" && Server.arg("device_id") == "2") {
    digitalWrite(22, LOW);
    char content[] = "Turn on 2.";
    Server.send(200, "text/plain", content);
  }
  else if (Server.arg("action") == "2" && Server.arg("device_id") == "2") {
    digitalWrite(22, HIGH);
    char content[] = "Turn off 2.";
    Server.send(200, "text/plain", content);
  }
  else if (Server.arg("action") == "1" && Server.arg("device_id") == "3") {
    digitalWrite(21, LOW);
    char content[] = "Turn on 3.";
    Server.send(200, "text/plain", content);
  }
  else if (Server.arg("action") == "2" && Server.arg("device_id") == "3") {
    digitalWrite(21, HIGH);
    char content[] = "Turn off 3.";
    Server.send(200, "text/plain", content);
  }
  else if (Server.arg("action") == "1" && Server.arg("device_id") == "4") {
    digitalWrite(19, LOW);
    char content[] = "Turn on 4.";
    Server.send(200, "text/plain", content);
  }
  else if (Server.arg("action") == "2" && Server.arg("device_id") == "4") {
    digitalWrite(19, HIGH);
    char content[] = "Turn off 4.";
    Server.send(200, "text/plain", content);
  }
  else if (Server.arg("action") == "1" && Server.arg("device_id") == "5") {
    digitalWrite(18, LOW);
    char content[] = "Turn on 5.";
    Server.send(200, "text/plain", content);
  }
  else if (Server.arg("action") == "2" && Server.arg("device_id") == "5") {
    digitalWrite(18, HIGH);
    char content[] = "Turn off 5.";
    Server.send(200, "text/plain", content);
  }
  else if (Server.arg("action") == "1" && Server.arg("device_id") == "0") {
    digitalWrite(18, LOW);
    digitalWrite(19, LOW);
    digitalWrite(21, LOW);
    digitalWrite(22, LOW);
    digitalWrite(23, LOW);
    char content[] = "Turn on all.";
    Server.send(200, "text/plain", content);
  }
  else if (Server.arg("action") == "2" && Server.arg("device_id") == "0") {
    digitalWrite(18, HIGH);
    digitalWrite(19, HIGH);
    digitalWrite(21, HIGH);
    digitalWrite(22, HIGH);
    digitalWrite(23, HIGH);
    char content[] = "Turn off all";
    Server.send(200, "text/plain", content);
  }
  else if (Server.arg("action") == "1" && Server.arg("device_id") == "99") {
    digitalWrite(23, LOW);
    digitalWrite(19, LOW);
    digitalWrite(21, LOW);
    digitalWrite(22, LOW);
    char content[] = "Turn on all lights.";
    Server.send(200, "text/plain", content);
  }
  else if (Server.arg("action") == "2" && Server.arg("device_id") == "99") {
    digitalWrite(23, HIGH);
    digitalWrite(19, HIGH);
    digitalWrite(21, HIGH);
    digitalWrite(22, HIGH);
    char content[] = "Turn off all lights";
    Server.send(200, "text/plain", content);
  }
  else {
    char content[] = "Invalid request.";
    Server.send(200, "text/plain", content);
  }
}


void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();

  ///////////////////////////////////////////////////////////////////////////////////////   WIFI CONFIG  ////////////////////////////////////////////////////////////////////////////////

  Serial.println("mounting FS...");
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(nvr_ip, json["nvr_ip"]);
          strcpy(nvr_username, json["nvr_username"]);
          strcpy(nvr_password, json["nvr_password"]);

          json.printTo(Serial);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }

  //initialize new parameter
  WiFiManagerParameter custom_nvr_ip("nvr", "NVR_IP", nvr_ip, 40);
  WiFiManagerParameter custom_nvr_username("nvr_username", "NVR_Username", nvr_username, 20);
  WiFiManagerParameter custom_nvr_password("nvr_password", "NVR_Password", nvr_password, 20);

  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add new parameter to wifi-config page
  wifiManager.addParameter(&custom_nvr_ip);
  wifiManager.addParameter(&custom_nvr_username);
  wifiManager.addParameter(&custom_nvr_password);

  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("HomeGuard", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }
  Serial.println("WiFi connected)");
  strcpy(nvr_ip, custom_nvr_ip.getValue());
  strcpy(nvr_username, custom_nvr_username.getValue());
  strcpy(nvr_password, custom_nvr_password.getValue());

  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["nvr_ip"] = nvr_ip;
    json["nvr_username"] = nvr_username;
    json["nvr_password"] = nvr_password;
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }
    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
  }
  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  pinMode(buttonPin, INPUT_PULLUP);
  //digitalWrite(buttonPin,HIGH);

  //////////////////////////////////////////////////////////////////////////////////  GET CREDENTIAL   //////////////////////////////////////////////////////////////////////////////////
  Serial.println(String(nvr_ip) + String(uri));
  Serial.println(String(server) + String(uri));

  http.begin(client, String(server) + String(uri));
  const char *keys[] = {"WWW-Authenticate"};
  http.collectHeaders(keys, 1);

  // Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();

  if (httpCode > 0) {
    authReq = http.header("WWW-Authenticate");
    //Serial.println(authReq);

    authorization = getDigestAuth(authReq, String(nvr_username), String(nvr_password), String(uri), 1);
    Serial.println(authorization);
    http.end();
  }
  Server.on("/test", test);
  Server.begin();
  pinMode(23, OUTPUT);
  digitalWrite(23, HIGH);
  pinMode(22, OUTPUT);
  digitalWrite(22, HIGH);
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  pinMode(19, OUTPUT);
  digitalWrite(19, HIGH);
  pinMode(18, OUTPUT);
  digitalWrite(18, HIGH);
  WiFi.persistent(true);
}

void loop() {
  //Serial.println("1");
  String payload;
  buttonState = digitalRead(buttonPin);
  //Serial.println("2");
  Serial.println(buttonState);
  if (buttonState == LOW) {
            WiFi.disconnect(true,true);
            delay(2000);
            ESP.restart();
       
    /*Serial.println("3");
    if (SPIFFS.begin()) {
      Serial.println("4");
      Serial.println("mounted file system");
      if (SPIFFS.exists("/config.json")) {
        //file exists, reading and loading
        Serial.println("reading config file");
        File configFile = SPIFFS.open("/config.json", "r");
        if (configFile) {
          Serial.println("opened config file");
          size_t size = configFile.size();
          // Allocate a buffer to store contents of the file.
          std::unique_ptr<char[]> buf(new char[size]);

          configFile.readBytes(buf.get(), size);
          DynamicJsonBuffer jsonBuffer;
          JsonObject& json = jsonBuffer.parseObject(buf.get());
          json.printTo(Serial);
          if (json.success()) {
            Serial.println("\nparsed json");

            strcpy(nvr_ip, json["nvr_ip"]);
            strcpy(nvr_username, json["nvr_username"]);
            strcpy(nvr_password, json["nvr_password"]);

            json.printTo(Serial);
          } else {
            Serial.println("failed to load json config");
          }
        }
      }
    } else {
      Serial.println("failed to mount FS");
    }

    WiFiManagerParameter custom_nvr_ip("nvr", "NVR_IP", nvr_ip, 40);
    WiFiManagerParameter custom_nvr_username("nvr_username", "NVR_Username", nvr_username, 20);
    WiFiManagerParameter custom_nvr_password("nvr_password", "NVR_Password", nvr_password, 20);
    //set config save notify callback
    WiFiManager wifiManager;
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    //add new parameter to wifi-config page
    wifiManager.addParameter(&custom_nvr_ip);
    wifiManager.addParameter(&custom_nvr_username);
    wifiManager.addParameter(&custom_nvr_password);

    wifiManager.setTimeout(180);
    if (!wifiManager.startConfigPortal("HomeGuard", "password")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
      delay(5000);
    }
    Serial.println("WiFi connected.");
    strcpy(nvr_ip, custom_nvr_ip.getValue());
    strcpy(nvr_username, custom_nvr_username.getValue());
    strcpy(nvr_password, custom_nvr_password.getValue());

    if (shouldSaveConfig) {
      Serial.println("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json["nvr_ip"] = nvr_ip;
      json["nvr_username"] = nvr_username;
      json["nvr_password"] = nvr_password;
      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("failed to open config file for writing");
      }
      json.printTo(Serial);
      json.printTo(configFile);
      configFile.close();
    }
    Serial.println("local ip");
    Serial.println(WiFi.localIP());
  */
  }
  //Serial.println("5");
  httpGet.begin(client, String(server) + String(uri));
  //Serial.println("6");
  httpGet.addHeader("Authorization", authorization);
  //Serial.println("7");
  int httpCode = httpGet.GET();
  //Serial.println("8");
  if (httpCode > 0) {
    //Serial.println("9");
    payload = httpGet.getString();
    //Serial.println("10");
    payload.remove(5000);
    //Serial.println("11");
    Serial.println(payload);
  } else {
    Serial.printf("[HTTP] GET... failed, error2: %s\n", httpGet.errorToString(httpCode).c_str());
  }
  //Serial.println("10");
  httpGet.end();

   httpPost.begin("http://52.220.134.31:3000/push");  //Specify destination for HTTP reques
  httpPost.addHeader("Content-Type", "text/plain");
  int httpResponseCode = httpPost.POST(payload);   //Send the actual POST request
  httpPost.end();
  
  
  uint32_t period = 5000L;       // 5s
  Serial.println("waiting");
  for ( uint32_t tStart = millis();  (millis() - tStart) < period;  ) {
    Server.handleClient();
  }
  Serial.println("11");
}
