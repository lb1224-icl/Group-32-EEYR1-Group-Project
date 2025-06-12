#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "ArduinoJson.h"

//Pins
#define IR_PIN 13
#define RF_PIN 25
#define NAME_PIN 3

//Wi-Fi
const char *ssid = "Group32";
const char *password = "Group32Password";

AsyncWebServer server(80);
AsyncEventSource events("/events");

//Magnetic 
String currentPole = "North";

//IR and Radio
unsigned long currentIR = 0;
unsigned long currentRF = 0;

//updates frequency read for IR
void updateIR() {
  //code from ir.ino
}

//updates frequency read for radio
void updateRF() {
  //code from radio.ino
}

//updates pole for magnet
void updateMagnetic() {
  //code from magnetic.ino
}

//works to get the Ultrasonic name
void updateUltrasonic() {
  //code from ultrasonic.io
}

//gets sensor readings into a JSON file and serializes to a string
String getSensorReadings() {
  StaticJsonDocument<256> jsonDoc;
  jsonDoc["IR"] = String(currentIR);
  jsonDoc["RF"] = String(currentRF);
  jsonDoc["NAME"] = currentName;
  jsonDoc["MAGNETIC"] = currentPole;

  String jsonString;
  serializeJson(jsonDoc, jsonString);
  return jsonString;
}

//handles start of soft AP wi-fi
void startWifi() {
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
}

//handles the data from joysticks
void handleControllerDataPOST(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  static String body = "";

  if (index == 0) {
    body = ""; 
  }

  body += String((const char*)data).substring(0, len);

  if (index + len == total) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      Serial.println("Failed to parse JSON");
      request->send(400, "text/plain", "Invalid JSON");
      return;
    }

    int joystickX1 = doc["joystickX1"] | 0;
    int joystickY1 = doc["joystickY1"] | 0;
    int joystickX2 = doc["joystickX2"] | 0;
    int joystickY2 = doc["joystickY2"] | 0;

    Serial.printf("Joystick1 X:%d Y:%d\n", joystickX1, joystickY1);
    Serial.printf("Joystick2 X:%d Y:%d\n", joystickX2, joystickY2);

    //code from drive.ino

    request->send(200, "text/plain", "OK");
  }
}

//handles the data from buttons
void handleButtonPressPOST(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  static String body = "";

  if (index == 0) {
    body = "";
  }

  body += String((const char*)data).substring(0, len);

  if (index + len == total) {
    StaticJsonDocument<100> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      Serial.println("Failed to parse JSON");
      request->send(400, "text/plain", "Invalid JSON");
      return;
    }

    String buttonIndex = doc["buttonIndex"];

    Serial.println("Button " + buttonIndex + " was pressed!");

    //send event for JS clients
    events.send(buttonIndex.c_str(), "buttonPressed", millis());

    request->send(200, "text/plain", "Button acknowledged");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(IR_PIN, INPUT);
  pinMode(RF_PIN, INPUT);
  pinMode(NAME_PIN, INPUT);

  startWifi();

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", String(), false);
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.serveStatic("/", SPIFFS, "/");

  //controller data POST
  server.on("/controllerdata", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      //no immediate response, all handled in handleControllerDataPOST
    },
    NULL,
    handleControllerDataPOST
  );

  //button press POST 
  server.on("/buttonpress", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      //no immediate response, all handled in handleButtonPressPOST
    },
    NULL,
    handleButtonPressPOST
  );

  events.onConnect([](AsyncEventSourceClient *client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    client->send("hello!", NULL, millis(), 1000);
  });

  server.addHandler(&events);
  server.begin();
}

void loop() {
  if ((millis() - lastSensorTime) > sensorTimerDelay) {
    updateRF();
    updateIR();
    updateMagnetic();
    updateUltrasonic();

    lastSensorTime = millis();

    events.send(getSensorReadings().c_str(), "newSensorReadings", millis());
  }

  receiveUSBits();
}
