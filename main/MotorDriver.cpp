#include <Arduino.h>

class Motor {
private:
  int speedPin;
  int dirPin;
  int channel;
  bool inverted;

  int speedToPWM(int sp) {
    sp = constrain(sp, 0, 100);
    if (sp == 0)
      return 0;
    return map(sp, 1, 100, 2000, 4095);
  }

public:
  Motor(int speedPin, int dirPin, int channel, bool inverted) {
    this->speedPin = speedPin;
    this->dirPin = dirPin;
    this->channel = channel;
    this->inverted = inverted;
  }

  void setSignedSpeed(float speed) {
    speed = constrain(speed, -100.0f, 100.0f);

    bool forward = speed >= 0;

    int absSpeed = abs((int)speed);

    set(absSpeed, forward);
  }

  void begin(int freq = 5000, int resolution = 12) {
    pinMode(dirPin, OUTPUT);

    ledcSetup(channel, freq, resolution);
    ledcAttachPin(speedPin, channel);
    stop();
  }

  void set(int speedPercent, bool forward) {
    bool dir = inverted ? !forward : forward;
    digitalWrite(dirPin, dir ? LOW : HIGH);
    ledcWrite(channel, speedToPWM(speedPercent));
  }

  void stop() {
    ledcWrite(channel, 0);
  }
};

class Robot {
private:
  Motor &m1;
  Motor &m2;
  Motor &m3;
  Motor &m4;

  float targetLeft = 0;
  float targetRight = 0;

  float currentLeft = 0;
  float currentRight = 0;

  float accelStep = 1.0f;

  unsigned long lastUpdate = 0;

public:

    int maxSpeed = 100;

    Robot(Motor &m1,
          Motor &m2,
          Motor &m3,
          Motor &m4)
      : m1(m1),
        m2(m2),
        m3(m3),
        m4(m4)
    {
    }


  void stop() {
    m1.stop();
    m2.stop();
    m3.stop();
    m4.stop();
  }

  void backward(int speed) {
    m1.set(speed, true);
    m2.set(speed, true);
    m3.set(speed, true);
    m4.set(speed, true);
  }

  void forward(int speed) {
    m1.set(speed, false);
    m2.set(speed, false);
    m3.set(speed, false);
    m4.set(speed, false);
  }

  void right(int speed) {
    m1.set(speed, false);
    m2.set(speed, true);
    m3.set(speed, false);
    m4.set(speed, true);
  }

  void left(int speed) {
    m1.set(speed, true);
    m2.set(speed, false);
    m3.set(speed, true);
    m4.set(speed, false);
  }

  void rampForward(int startSpeed, int endSpeed, int stepDelay) {
    startSpeed = constrain(startSpeed, 0, 100);
    endSpeed   = constrain(endSpeed, 0, 100);

    if (startSpeed < endSpeed) {
      for (int sp = startSpeed; sp <= endSpeed; sp++)
      {
        forward(sp);
        delay(stepDelay);
      }
    } else {
      for (int sp = startSpeed; sp >= endSpeed; sp--)
      {
        forward(sp);
        delay(stepDelay);
      }
    }
  }      

  void setMaxSpeed(int value) {
    maxSpeed = constrain(value,0,100);
  }

  void drive(float x, float y) {
    x = constrain(x,-100,100);
    y = constrain(y,-100,100);

    float left = y + x;
    float right = y - x;

    left = constrain(left,-100,100);
    right = constrain(right,-100,100);

    left *= maxSpeed / 100.0f;
    right *= maxSpeed / 100.0f;

    targetLeft = left;
    targetRight = right;
  }

  void emergencyStop() {
    targetLeft = 0;
    targetRight = 0;

    currentLeft = 0;
    currentRight = 0;

    m1.stop();
    m2.stop();
    m3.stop();
    m4.stop();
  }

  void update() {
    if(millis() - lastUpdate < 20)
      return;

    lastUpdate = millis();

    if(currentLeft < targetLeft)
        currentLeft = min(currentLeft + accelStep, targetLeft);
    else if(currentLeft > targetLeft)
        currentLeft = max(currentLeft - accelStep, targetLeft);

    if(currentRight < targetRight)
        currentRight = min(currentRight + accelStep, targetRight);
    else if(currentRight > targetRight)
        currentRight = max(currentRight - accelStep, targetRight);

    m1.setSignedSpeed(currentLeft);
    m2.setSignedSpeed(currentLeft);

    m3.setSignedSpeed(currentRight);
    m4.setSignedSpeed(currentRight);
  }
};
