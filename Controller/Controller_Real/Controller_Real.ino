#include "WiFi.h"
#include "HTTPClient.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"

//Wi-Fi
const char *ssid = "Group32";
const char *password = "Group32Password";

//pins
#define WIFI_PIN 5
#define POWER_PIN 25
#define LED_PIN 17

#define UP_PIN 18
#define RIGHT_PIN 21
#define DOWN_PIN 23
#define LEFT_PIN 19
#define LEFT_BUMPER_PIN 16
#define RIGHT_BUMPER_PIN 22

#define VRX_PIN_1 26
#define VRY_PIN_1 27
#define VRX_PIN_2 14
#define VRY_PIN_2 12

//timing
const unsigned long wifiHoldTime = 1500;
const unsigned long powerHoldTime = 1500;
const unsigned long joystickInterval = 100;

//states
bool wifiActionTaken = false;
bool powerActionTaken = false;
unsigned long wifiPressStart = 0;
unsigned long powerPressStart = 0;
unsigned long lastJoystickCheck = 0;

//joystick values
int xValue1 = 0, yValue1 = 0, xValue2 = 0, yValue2 = 0;

struct Button {
  const char* name;
  int pin;
  int index;
  bool lastState;
};

//array of basic buttons (not wi-fi and power)
Button buttons[] = {
  {"UP", UP_PIN, 0, HIGH},
  {"RIGHT", RIGHT_PIN, 1, HIGH},
  {"DOWN", DOWN_PIN, 2, HIGH},
  {"LEFT", LEFT_PIN, 3, HIGH},
  {"LEFT_BUMPER", LEFT_BUMPER_PIN, 4, HIGH},
  {"RIGHT_BUMPER", RIGHT_BUMPER_PIN, 5, HIGH}
};

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(WIFI_PIN, INPUT_PULLUP);
  pinMode(POWER_PIN, INPUT_PULLUP);

  for (int i = 0; i < 6; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
  }

  pinMode(VRX_PIN_1, INPUT);
  pinMode(VRY_PIN_1, INPUT);
  pinMode(VRX_PIN_2, INPUT);
  pinMode(VRY_PIN_2, INPUT);

  //setup wake from deep sleep when power button held low
  esp_sleep_enable_ext0_wakeup((gpio_num_t)POWER_PIN, 0);

  //attempt Wi-Fi connection once on boot
  connectToWiFi();
}

void loop() {
  unsigned long currentMillis = millis();

  handleWifiButton(currentMillis);
  handlePowerButton(currentMillis);
  handleButtons();

  if (currentMillis - lastJoystickCheck >= joystickInterval) {
    handleJoystick();
    lastJoystickCheck = currentMillis;
  }
}

//read joystick values, send to rover
void handleJoystick() {
  xValue1 = analogRead(VRX_PIN_1);
  yValue1 = analogRead(VRY_PIN_1);
  xValue2 = analogRead(VRX_PIN_2);
  yValue2 = analogRead(VRY_PIN_2);
  sendJoystickInfo();
}

//checks all digital buttons and reports presses
void handleButtons() {
  for (int i = 0; i < 6; i++) {
    bool current = digitalRead(buttons[i].pin);
    if (buttons[i].lastState == HIGH && current == LOW) {
      Serial.printf("%s pressed!\n", buttons[i].name);
      buttonPressed(buttons[i].index);
    }
    buttons[i].lastState = current;
  }
}

//handle Wi-Fi button logic with hold detection
void handleWifiButton(unsigned long now) {
  bool wifiPressed = digitalRead(WIFI_PIN) == LOW;

  if (wifiPressed) {
    if (!wifiActionTaken) {
      if (wifiPressStart == 0) {
        wifiPressStart = now;
      } else if (now - wifiPressStart >= wifiHoldTime) {
        Serial.println("WiFi button held - reconnecting...");
        reconnectWifi();
        wifiActionTaken = true;
      }
    }
  } else {
    wifiPressStart = 0;
    wifiActionTaken = false;
  }
}

//handle power button logic with hold detection
void handlePowerButton(unsigned long now) {
  bool powerPressed = digitalRead(POWER_PIN) == LOW;

  if (powerPressed) {
    if (!powerActionTaken) {
      if (powerPressStart == 0) {
        powerPressStart = now;
      } else if (now - powerPressStart >= powerHoldTime) {
        Serial.println("Power button held - entering deep sleep...");
        flashAndSleep();
        powerActionTaken = true;
      }
    }
  } else {
    powerPressStart = 0;
    powerActionTaken = false;
  }
}

//disconnect Wi-Fi and reconnect 
void reconnectWifi() {
  WiFi.disconnect(true);
  delay(100);
  connectToWiFi();
}

//initial Wi-Fi connection logic
void connectToWiFi() {
  Serial.println("Attempting WiFi connection...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 20;
  while (WiFi.status() != WL_CONNECTED && attempts > 0) {
    digitalWrite(LED_PIN, (attempts % 2 == 0) ? HIGH : LOW);
    delay(500);
    attempts--;
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
    WiFi.setSleep(true);
    Serial.println("Connected to Rover");
  } else {
    digitalWrite(LED_PIN, LOW);
    Serial.println("Failed to connect to Rover");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
}

//blink LED, shut down wifi, stop power leakage from ADC pins, enter deep sleep
void flashAndSleep() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(300);
    digitalWrite(LED_PIN, LOW);
    delay(300);
  }

  pinMode(VRX_PIN_1, INPUT);
  pinMode(VRY_PIN_1, INPUT);
  pinMode(VRX_PIN_2, INPUT);
  pinMode(VRY_PIN_2, INPUT);
  digitalWrite(VRX_PIN_1, LOW);
  digitalWrite(VRY_PIN_1, LOW);
  digitalWrite(VRX_PIN_2, LOW);
  digitalWrite(VRY_PIN_2, LOW);

  adc_power_off();
  digitalWrite(LED_PIN, LOW);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);  
  delay(100);

  esp_deep_sleep_start();
}

//send button press to the rover
void buttonPressed(int index) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://192.168.4.1/buttonpress");
    http.addHeader("Content-Type", "application/json");
    http.POST("{\"buttonIndex\": " + String(index) + "}");
    http.end();
  }
}

//send joystick position to the rover
void sendJoystickInfo() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://192.168.4.1/controllerdata");
    http.addHeader("Content-Type", "application/json");

    String jsonData = "{\"joystickX1\": " + String(xValue1) +
                      ", \"joystickY1\": " + String(yValue1) +
                      ", \"joystickX2\": " + String(xValue2) +
                      ", \"joystickY2\": " + String(yValue2) + "}";

    int httpResponseCode = http.POST(jsonData);
    Serial.println("Joystick response: " + String(httpResponseCode));
    http.end();
  }
}

//reduce power consumption by isolating ADC GPIOs
void adc_power_off() {
  rtc_gpio_isolate(GPIO_NUM_26);
  rtc_gpio_isolate(GPIO_NUM_27);
  rtc_gpio_isolate(GPIO_NUM_14);
  rtc_gpio_isolate(GPIO_NUM_12);
}