/*
// ================= MOTOR PINS =================
const int m1_speed = 7;
const int m1_dir   = 15;

const int m2_speed = 4;
const int m2_dir   = 5;

const int m3_speed = 39;
const int m3_dir   = 38;

const int m4_speed = 42;
const int m4_dir   = 41;

// ================= PWM CONFIG =================
const int pwmFreq = 5000;
const int pwmResolution = 12; // 0–4095

// PWM channels (ESP32-S3 needs separate channels)
const int ch1 = 0;
const int ch2 = 1;
const int ch3 = 2;
const int ch4 = 3;

// ================= MOTOR SETTINGS =================
bool m1_inverted = true;
bool m2_inverted = false;
bool m3_inverted = true;
bool m4_inverted = false;

// ================= HELPERS =================
// convert 0–100% to usable PWM (with dead zone >2000)
int speedToPWM(int sp) {
  sp = constrain(sp, 0, 100);
  // IMPORTANT:  motors starts ~2000
  int pwm = map(sp, 0, 100, 0, 4095);
  if (pwm > 0 && pwm < 2000) pwm = 2000; // overcome dead zone
  if (sp == 0) pwm = 0;
  return pwm;
}

void setMotor(int dirPin, int channel, bool forward, int speedPercent, bool inverted) {
  bool dir = inverted ? !forward : forward;
  digitalWrite(dirPin, dir ? LOW : HIGH);
  int pwm = speedToPWM(speedPercent);
  ledcWrite(channel, pwm);
}

void stopAll() {
  ledcWrite(ch1, 0);
  ledcWrite(ch2, 0);
  ledcWrite(ch3, 0);
  ledcWrite(ch4, 0);
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(m1_dir, OUTPUT);
  pinMode(m2_dir, OUTPUT);
  pinMode(m3_dir, OUTPUT);
  pinMode(m4_dir, OUTPUT);

  ledcSetup(ch1, pwmFreq, pwmResolution);
  ledcSetup(ch2, pwmFreq, pwmResolution);
  ledcSetup(ch3, pwmFreq, pwmResolution);
  ledcSetup(ch4, pwmFreq, pwmResolution);

  ledcAttachPin(m1_speed, ch1);
  ledcAttachPin(m2_speed, ch2);
  ledcAttachPin(m3_speed, ch3);
  ledcAttachPin(m4_speed, ch4);

  stopAll();
}

// ================= MAIN LOOP =================
void loop() {
  // ===== STOP =====
  Serial.println("STOP");
  stopAll();
  delay(1000);

  // ===== FORWARD =====
  Serial.println("FORWARD");
  setMotor(m1_dir, ch1, true, 60, m1_inverted);
  setMotor(m2_dir, ch2, true, 60, m2_inverted);
  setMotor(m3_dir, ch3, true, 60, m3_inverted);
  setMotor(m4_dir, ch4, true, 60, m4_inverted);
  delay(10000);

  // ===== STOP =====
  Serial.println("STOP");
  stopAll();
  delay(1000);

  // ===== BACKWARD =====
  Serial.println("BACKWARD");
  setMotor(m1_dir, ch1, false, 60, m1_inverted);
  setMotor(m2_dir, ch2, false, 60, m2_inverted);
  setMotor(m3_dir, ch3, false, 60, m3_inverted);
  setMotor(m4_dir, ch4, false, 60, m4_inverted);
  delay(10000);

  // ===== SPEED TEST (0–100%) =====
  Serial.println("SPEED RAMP");
  for (int sp = 0; sp <= 100; sp += 10)
  {
    Serial.println(sp);
    setMotor(m1_dir, ch1, true, sp, m1_inverted);
    setMotor(m2_dir, ch2, true, sp, m2_inverted);
    setMotor(m3_dir, ch3, true, sp, m3_inverted);
    setMotor(m4_dir, ch4, true, sp, m4_inverted);
    delay(1000);
  }
}
*/
#include <FastLED.h>
#include <WiFi.h>
#include "MotorDriver.cpp"
#include <WebSocketsServer.h>

// ================= LED =================
const int ledPin = 48;
const int numLeds = 1;
CRGB leds[numLeds];


// ================ MOTORS ===============
Motor m1(7, 15, 0, true);
Motor m2(4, 5, 1, false);
Motor m3(39, 38, 2, true);
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

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  if (type == WStype_TEXT)
  {
    String cmd = (char*)payload;

    Serial.println(cmd);

    if (cmd == "W") robot.forward(100);
    if (cmd == "S") robot.backward(100);
    if (cmd == "A") robot.left(100);
    if (cmd == "D") robot.right(100);
    if (cmd == "X") robot.stop();
  }
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

  // keep green ON if connected
  if (WiFi.status() == WL_CONNECTED)
  {
    setLED(false, true);
  } else {
    setLED(true, false);
    //blinkRedForever();
  }



  /*
  Serial.println("STOP");
  robot.stop();
  delay(1000);

  Serial.println("RAMP 10% → 100%");
  robot.rampForward(10, 100, 100); // step every 100ms
  delay(2000);

  Serial.println("STOP");
  robot.stop();
  delay(2000);

  Serial.println("LEFT");
  robot.left(50);
  delay(5000);
  robot.stop();

  Serial.println("RIGHT");
  robot.right(50);
  delay(5000);
  robot.stop();

  
  Serial.println("BACKWARD RAMP");

  for (int sp = 10; sp <= 100; sp++)
  {
    robot.backward(sp);
    delay(100);
  }
  delay(2000);
  */


}


