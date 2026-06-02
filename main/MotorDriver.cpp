#include <Arduino.h>

class Motor {
private:
  int speedPin;
  int dirPin;
  int channel;
  bool inverted;

  int speedToPWM(int sp) {
    sp = constrain(sp, 0, 100);

    int pwm = map(sp, 0, 100, 0, 4095);

    if (pwm > 0 && pwm < 2000)
        pwm = 2000;

    if (sp == 0)
        pwm = 0;

    return pwm;
  }

public:
  Motor(int speedPin, int dirPin, int channel, bool inverted) {
    this->speedPin = speedPin;
    this->dirPin = dirPin;
    this->channel = channel;
    this->inverted = inverted;
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

public:
  Robot(Motor &m1,
        Motor &m2,
        Motor &m3,
        Motor &m4)
      : m1(m1), m2(m2), m3(m3), m4(m4)
  {}

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

  void right(int speed)
  {
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
};