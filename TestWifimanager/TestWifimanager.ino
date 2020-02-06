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
#include <PubSubClient.h>
extern "C" {
#include "crypto/base64.h"
}

#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())

WiFiClient mqttWiFi;
WiFiClient client;
PubSubClient mqttclient(mqttWiFi);
WebServer Server(80);
HTTPClient httpGet;
HTTPClient httpPost;
HTTPClient http;

char mqtt_server_str[50];
const int buttonPin = 27;
int buttonState = LOW;
char nvr_ip[40] = "http://192.168.1.101";
char nvr_username[20] = "admin";
char nvr_password[20] = "Passw0rd";
char mqtt[40] = "";
bool shouldSaveConfig = false;
String authReq;
String authorization;
String nvr_server;
String mqtt_server;
//const char *uri = "/stw-cgi/system.cgi?msubmenu=eventlog&action=view";
const char *uri = "/push/event";



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



void mqttcallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonBuffer<200> jsonBuffer;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  //Serial.println(1);
  String dest = "";
  //Serial.println(2);
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    dest += (char)payload[i];
  }
  //Serial.println(3);
  Serial.println(dest);
  JsonObject& actuator = jsonBuffer.parseObject(dest);
  const char* action = actuator["action"];
  const char* device_id = actuator["device_id"];
  const char* platform = actuator["platform"];
  Serial.println(action);
  Serial.println(device_id);
  Serial.println(platform);
  Serial.println();
  if (action == "1" && device_id == "99") {
    digitalWrite(23, LOW);
    digitalWrite(19, LOW);
    digitalWrite(21, LOW);
    digitalWrite(22, LOW);
  } else if (action == "2" && device_id == "99") {
    digitalWrite(23, HIGH);
    digitalWrite(19, HIGH);
    digitalWrite(21, HIGH);
    digitalWrite(22, HIGH);
  } else if (action == "1" && device_id == "5") {
    digitalWrite(18, LOW);
  }
  else if (action == "2" && device_id == "5") {
    digitalWrite(18, HIGH);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttclient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttclient.connect("arduinoClient")) {
      String  topic = "topic/homeguard/" + String(ESP_getChipId());
      const char * c_topic = topic.c_str();
      Serial.println("MQTT Topic is " + topic);
      Serial.println("connected");
      mqttclient.subscribe(c_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttclient.state());
      Serial.println(" try again next loop");
      // Wait 5 seconds before retrying
      delay(2000);
      break;
    }
  }
}


void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  pinMode(buttonPin, INPUT_PULLUP);
  delay(1000);
  ///////////////////////////////////////////////////////////////////////////////////////   WIFI CONFIG  ////////////////////////////////////////////////////////////////////////////////
  buttonState = digitalRead(buttonPin);
  Serial.println(buttonState);
  if (buttonState == LOW) {
    WiFi.begin();
    delay(2000);
    WiFi.disconnect(true, true);
    delay(3000);
    ESP.restart();
  } else {
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
            strcpy(mqtt, json["mqtt"]);

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
    WiFiManagerParameter custom_nvr_ip("nvr_ip", "NVR_IP", nvr_ip, 40);
    WiFiManagerParameter custom_nvr_username("nvr_username", "NVR_Username", nvr_username, 20);
    WiFiManagerParameter custom_nvr_password("nvr_password", "NVR_Password", nvr_password, 20);
    WiFiManagerParameter custom_mqtt("mqtt", "MQTT", mqtt, 40);


    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    //add new parameter to wifi-config page
    wifiManager.addParameter(&custom_nvr_ip);
    wifiManager.addParameter(&custom_nvr_username);
    wifiManager.addParameter(&custom_nvr_password);
    wifiManager.addParameter(&custom_mqtt);

    if (!wifiManager.autoConnect("HomeGuard", "H!@#$%^G")) {
      delay(3000);
      ESP.restart();
      delay(5000);
    }

    //Serial.println("WiFi connected");
    strcpy(nvr_ip, custom_nvr_ip.getValue());
    strcpy(nvr_username, custom_nvr_username.getValue());
    strcpy(nvr_password, custom_nvr_password.getValue());
    strcpy(mqtt, custom_mqtt.getValue());

    if (shouldSaveConfig) {
      Serial.println("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json["nvr_ip"] = nvr_ip;
      json["nvr_username"] = nvr_username;
      json["nvr_password"] = nvr_password;
      json["mqtt"] = mqtt;
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
    nvr_server = "http://" + String(nvr_ip) + String(uri);

    if (WiFi.status() == WL_CONNECTED) {
      http.begin(client, String(nvr_server));
      const char *keys[] = {"WWW-Authenticate"};
      http.collectHeaders(keys, 1);

      // Serial.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      http.setConnectTimeout(5000);
      int httpCode = http.GET();

      if (httpCode > 0) {
        authReq = http.header("WWW-Authenticate");
        //Serial.println(authReq);

        authorization = getDigestAuth(authReq, String(nvr_username), String(nvr_password), String(uri), 1);
        Serial.println(authorization);
        http.end();
      }

      char * toDecode = mqtt;
      size_t outputLength;

      unsigned char * decoded = base64_decode((const unsigned char *)toDecode, strlen(toDecode), &outputLength);
      const char * decoced_str = reinterpret_cast<const char*>(decoded);


      Serial.print("Length of decoded message: ");
      Serial.println(outputLength);

      Serial.printf("%.*s", outputLength, decoded);
      //String test = decoded;
      String part_1 = getValue(decoced_str, '.', 0);
      String part_2 = getValue(decoced_str, '.', 1);
      String part_3 = getValue(decoced_str, '.', 2);
      String part_4 = getValue(decoced_str, '.', 3);
      String mqtt_server = part_4 + "." + part_3 + "." + part_2 + "." + part_1;
      mqtt_server.toCharArray(mqtt_server_str, 50);
      Serial.println(mqtt_server_str);
      free(decoded);

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
      WiFi.setSleep(false);

      mqttclient.setServer(mqtt_server_str, 1883);
      mqttclient.setCallback(mqttcallback);
    }


  }
}

void loop() {
  if (!mqttclient.connected()) {
    reconnect();
  }
  if (WiFi.status() == WL_CONNECTED) {
    String payload;
    buttonState = digitalRead(buttonPin);
    Serial.println(buttonState);
    if (buttonState == LOW) {
      delay(5000);
      buttonState = digitalRead(buttonPin);
      Serial.println(buttonState);
      if (buttonState == LOW) {
        WiFi.disconnect(true, true);
        delay(2000);
        ESP.restart();
      }
    }
    
        httpGet.begin(client, String(nvr_server)) ;
        httpGet.addHeader("Authorization", authorization);
        httpGet.setConnectTimeout(5000);
        int httpCode = httpGet.GET();
        if (httpCode > 0) {
          payload = httpGet.getString();
          payload.remove(5000);
          Serial.println(payload);
        } else {
          Serial.printf("[HTTP] GET... failed, error2: %s\n", httpGet.errorToString(httpCode).c_str());
        }
        httpGet.end();

        String payload_id = String(ESP_getChipId()) + "\n" + payload;
        Serial.println(payload_id);
        //String payload_id = String(ESP_getChipId() + "\r\n" + payload);
        httpPost.begin("http://192.168.43.225:3000/push");
        //httpPost.begin("http://52.220.134.31:3000/push");
        httpPost.setConnectTimeout(5000);
        httpPost.addHeader("Content-Type", "text/plain");
        int httpResponseCode = httpPost.POST(payload_id);
        httpPost.end();
    
    uint32_t period = 5000L;
    Serial.println("waiting");
    for ( uint32_t tStart = millis();  (millis() - tStart) < period;  ) {
      buttonState  = digitalRead(buttonPin);
              delay(1000);
      if (buttonState == LOW) {
        delay(5000);
        buttonState = digitalRead(buttonPin);
        Serial.println(buttonState);
        if (buttonState == LOW) {
          WiFi.disconnect(true, true);
          delay(2000);
          ESP.restart();
        }
      } else {
        mqttclient.loop();
      }
    }
  } else {
    WiFi.begin();
    Serial.print("try to connect to ap ");
    Serial.print(WiFi.SSID().c_str());
    Serial.print(".");
    buttonState  = digitalRead(buttonPin);
    Serial.println(buttonState);
    if (buttonState == LOW) {
      delay(5000);
      buttonState = digitalRead(buttonPin);
      Serial.println(buttonState);
      if (buttonState == LOW) {
        WiFi.disconnect(true, true);
        delay(2000);
        ESP.restart();
      }
    }
    delay(3000);
  }
  Serial.println("END LOOP");
}
