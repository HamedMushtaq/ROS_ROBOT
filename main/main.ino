#include <FastLED.h>
#include <WiFi.h>
#include "MotorDriver.cpp"
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// ================= LED =================
const int ledPin = 48;
const int numLeds = 1;
CRGB leds[numLeds];


// ================ MOTORS ===============
Motor m1(7, 15, 0, true);
Motor m2(39, 38, 2, false);
Motor m3(4, 5, 1, true);
Motor m4(42, 41, 3, false);

Robot robot(m1, m2, m3, m4);
WebSocketsServer webSocket = WebSocketsServer(81);


// ================= WIFI =================
const char* ssid = "devTools";
const char* password = "admin!5052";

// ================= LED FUNCTION =================
void setLED(bool red, bool green) {
  if (red && !green) leds[0] = CRGB::Red;
  else if (!red && green) leds[0] = CRGB::Green;
  else leds[0] = CRGB::Black;

  FastLED.show();
}

void flashGreen3Times() {
  for (int i = 0; i < 3; i++)
  {
    setLED(false, true);
    delay(300);
    setLED(false, false);
    delay(300);
  }
}

void blinkRedForever() {
  while (true)
  {
    setLED(true, false);
    delay(500);
    setLED(false, false);
    delay(500);
  }
}

// =============== WS 
void onWebSocketEvent(
    uint8_t num,
    WStype_t type,
    uint8_t * payload,
    size_t length) {
    if(type != WStype_TEXT)
        return;

    JsonDocument doc;

    auto err = deserializeJson(doc,payload);

    if (err)
    {
        Serial.print("JSON Error: ");
        Serial.println(err.c_str());
        return;
    }

    float x = doc["x"] | 0;
    float y = doc["y"] | 0;

    int speed = doc["speed"] | 100;

    robot.setMaxSpeed(speed);
    robot.drive(x,y);
}

void testMotors() {
  m1.set(100, true);
  delay(2000);
  robot.stop();
  m2.set(100, true);
  delay(2000);
  m3.set(100, true);
  delay(2000);
  m4.set(100, true);
  delay(2000);
}

void setup() {
  Serial.begin(115200);

  m1.begin();
  m2.begin();
  m3.begin();
  m4.begin();

  // LED init
  FastLED.addLeds<WS2812, ledPin, GRB>(leds, numLeds);
  FastLED.clear();
  FastLED.show();

  testMotors();

  // Start WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  unsigned long start = millis();

  setLED(true, false);
  while (WiFi.status() != WL_CONNECTED)
  {
    setLED(true, false);
    
    delay(300);
    Serial.print(".");

    if (millis() - start > 10000)
    {
      Serial.println("\nWiFi Failed!");
      setLED(true, false);
      break;
    }
    
  }
  

  Serial.println("\nWiFi Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  // SUCCESS INDICATION
  flashGreen3Times();


}

void loop() {
  webSocket.loop();

  robot.update();
  // keep green ON if connected
  if (WiFi.status() == WL_CONNECTED)
  {
    setLED(false, true);
  } else {
    setLED(true, false);
  }

}


