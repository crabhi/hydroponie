#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>

#ifndef STASSID
#define STASSID "sedlakovi"
#define STAPSK  "83dpAwkE"
#endif

const char *ssid = STASSID;
const char *password = STAPSK;

const long utcOffsetInSeconds = 0;
const unsigned long ntpUpdateIntervalMillis = 2 * 60 * 60 * 1000;  // every two hours

#define TIME_SEGMENTS 1440  // every minute of day is a segment

WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds, ntpUpdateIntervalMillis);

ESP8266WebServer server(80);

const int led = LED_BUILTIN;
const int motorPin = led;

uint8_t manualFlow = 0;
bool isManualFlow = false;

uint8_t flowPattern[TIME_SEGMENTS] = {};


void handleRoot() {
  char temp[1000];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf(temp, 1000,

           "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>Hydroponic</title>\
    <style>\
    </style>\
  </head>\
  <body>\
    <h1>Hello from ESP8266!</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <p>Current time: %02d:%02d:%02d</p>\
    <p>Current flow: %d</p>\
    <p>Flow mode: %s</p>\
    <p>Current time segment: %d/%d</p>\
  </body>\
</html>",

           hr, min % 60, sec % 60,
           timeClient.getHours(), timeClient.getMinutes(), timeClient.getSeconds(),
           currentFlow(),
           isManualFlow ? "manual" : "auto",
           (unsigned int) (timeClient.getEpochTime() / 60) % TIME_SEGMENTS, TIME_SEGMENTS
          );
  server.send(200, "text/html", temp);
}

void setFlow() {
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "Expected argument `val`.");
    return;
  }

  auto flowPatternStr = server.arg("val");
  int8 newFlowPattern[TIME_SEGMENTS] = {};

  if (flowPatternStr.length() != (TIME_SEGMENTS * 3)) {
    server.send(400, "text/plain", "Flow pattern in `val` should be " + String(TIME_SEGMENTS) + " number triplets long.");
    return;
  }

  for (uint i = 0; i < TIME_SEGMENTS; i++) {
    long currentNum = flowPatternStr.substring(i * 3, (i + 1) * 3).toInt();
    if (currentNum > 255 || currentNum < 0) {
      server.send(400, "text/plain", "Flow pattern intensities should in the range [0, 255].");
      return;
    }

    newFlowPattern[i] = currentNum;
  }

  if(memcmp(EEPROM.getConstDataPtr(), &newFlowPattern, TIME_SEGMENTS) == 0) {
    server.send(200, "text/plain", "No change.");
    return;    
  }
  
  EEPROM.put(0, newFlowPattern);
  EEPROM.commit();
  memcpy(&flowPattern, &newFlowPattern, TIME_SEGMENTS);
  server.send(200, "text/plain", "Changed.");
  return;
}

void getFlow() {
  String response;
  response.reserve(TIME_SEGMENTS * 3);

  for (uint i = 0; i < TIME_SEGMENTS; i++) {
    response += String(flowPattern[i]);
  }
  
  server.send(200, "text/plain", response);
}


void setManualFlow() {
  if (!server.hasArg("mode")) {
    server.send(400, "text/plain", "Expected argument `mode`.");
    return;
  }

  if (server.arg("mode") == "manual") {
    if (!server.hasArg("val")) {
      server.send(400, "text/plain", "Expected argument `val`.");
      return;
    }
    auto newManualFlow = server.arg("val").toInt();
    if (newManualFlow < 0 || newManualFlow > 255) {
      server.send(400, "text/plain", "Flow should be in range [0, 255]");
      return;
    }
    
    isManualFlow = true;
    manualFlow = newManualFlow;
    server.send(200, "text/plain", "Flow mode set to manual(" + String(manualFlow) + "/255)");
    return;
  } else if (server.arg("mode") == "auto") {
    isManualFlow = false;
    manualFlow = 0;
    server.send(200, "text/plain", "Flow mode set to auto. The flow is set by time and /flowPattern");
    return;
  } else {
    server.send(400, "text/plain", "Expected `mode` to be one of manual, auto.");
    return;
  }
}


void getManualFlow() {
  server.send(200, "text/json", "{\"mode\": \"" + String(isManualFlow ? "manual" : "auto") + "\", \"val\": \"" + String(manualFlow) + "\"}");
}


void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}


void setup(void) {
  pinMode(led, OUTPUT);
  pinMode(motorPin, OUTPUT);

  analogWriteRange(255);  // Max value in each segment of flowPattern
  analogWrite(motorPin, 0);

  //digitalWrite(led, LOW);
  
  Serial.begin(115200);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println(String(LED_BUILTIN));

  if (MDNS.begin("hydroponic")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/flowPattern", HTTP_POST, setFlow);
  server.on("/flowPattern", HTTP_GET, getFlow);
  server.on("/manualFlow", HTTP_POST, setManualFlow);
  server.on("/manualFlow", HTTP_GET, getManualFlow);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
  
  timeClient.begin();
  EEPROM.begin(TIME_SEGMENTS);
  EEPROM.get(0, flowPattern);
}


uint8_t currentFlow() {
  if (isManualFlow) {
    return manualFlow;
  } else {
    unsigned int currentTimeSegment = (timeClient.getEpochTime() / 60) % TIME_SEGMENTS;
    return flowPattern[currentTimeSegment];
  }
}


void loop(void) {
  timeClient.update();
  server.handleClient();
  MDNS.update();

  analogWrite(motorPin, 255 - currentFlow());
}
