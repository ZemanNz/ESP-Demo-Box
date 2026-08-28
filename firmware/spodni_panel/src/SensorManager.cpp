#include "SensorManager.h"
#include "HardwareSetup.h"
#include <Wire.h>

// Externí instance definované v HardwareSetup.cpp
#ifdef ENABLE_OLED_SSD1306
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  extern Adafruit_SSD1306 display;
#endif

#ifdef ENABLE_LED_STRIP
  #include <Adafruit_NeoPixel.h>
  extern Adafruit_NeoPixel strip;
#endif

#ifdef ENABLE_SMART_SERVO
  #include "SmartServoBus.hpp"
  using namespace lx16a;
  extern SmartServoBus servoBus;
#endif

#if defined(ENABLE_SERVO_CLASSIC) || defined(ENABLE_SERVO_CONT)
  #include <ESP32Servo.h>
#endif

#ifdef ENABLE_SERVO_CLASSIC
  extern Servo servoClassic;
#endif

#ifdef ENABLE_SERVO_CONT
  extern Servo servoCont;
#endif

SensorManager sensorManager;

SensorManager::SensorManager() {
    lastClkState = HIGH;
    encoderPos = 0;
    lastReportedEncoderPos = 0;
}

// =============================================================================
// 1. ČTENÍ VŠECH VSTUPŮ (INPUTS)
// =============================================================================

void SensorManager::readJoystick() {
#ifdef ENABLE_JOYSTICK
    int x = analogRead(PIN_JOY_X);
    int y = analogRead(PIN_JOY_Y);
    bool btn = (digitalRead(PIN_JOY_SW) == LOW);
    globalState.updateJoystick(x, y, btn);
#endif
}

void SensorManager::readPotentiometer() {
#ifdef ENABLE_POTENTIOMETER
    int pot = analogRead(PIN_POTENTIOMETER);
    globalState.updatePotentiometer((uint16_t)pot);
#endif
}

void SensorManager::readButtons() {
#ifdef ENABLE_BUTTONS
    bool btns[5];
    btns[0] = (digitalRead(PIN_BTN_1) == LOW);
    btns[1] = (digitalRead(PIN_BTN_2) == LOW);
    btns[2] = (digitalRead(PIN_BTN_3) == LOW);
    btns[3] = (digitalRead(PIN_BTN_4) == LOW);
    btns[4] = (digitalRead(PIN_BTN_5) == LOW);
    globalState.updateButtons(btns);
#endif
}

void SensorManager::readSwitches() {
#ifdef ENABLE_SWITCHES
    bool s1 = (digitalRead(PIN_SWITCH_1) == HIGH);
    bool s2 = (digitalRead(PIN_SWITCH_2) == HIGH);
    globalState.updateSwitches(s1, s2);
#endif
}

void SensorManager::readEncoder() {
#ifdef ENABLE_ENCODER
    int clkState = digitalRead(PIN_ENC_CLK);
    if (clkState != lastClkState && clkState == LOW) {
        if (digitalRead(PIN_ENC_DT) != clkState) {
            encoderPos++;
        } else {
            encoderPos--;
        }
    }
    lastClkState = clkState;

    bool btn = false;
#ifdef ENABLE_JOYSTICK
    // Joystick tlačítko nebo enkodér tlačítko
#endif
    int16_t delta = (int16_t)(encoderPos - lastReportedEncoderPos);
    lastReportedEncoderPos = encoderPos;

    globalState.updateEncoder(encoderPos, delta, btn);
#endif
}

int16_t SensorManager::getSmartServoAngle() {
#ifdef ENABLE_SMART_SERVO
    Angle cur = servoBus.pos(0);
    return cur.isNaN() ? -1 : (int16_t)cur.deg();
#else
    return -1;
#endif
}

void SensorManager::updateAllInputs() {
    readJoystick();
    readPotentiometer();
    readButtons();
    readSwitches();
    readEncoder();
}

// =============================================================================
// 2. ŘÍZENÍ VÝSTUPŮ (OUTPUTS)
// =============================================================================

void SensorManager::setSmartServo(int16_t angle) {
#ifdef ENABLE_SMART_SERVO
    if (angle >= 0 && angle <= 240) {
        servoBus.set(0, Angle::deg((float)angle));
    }
#endif
}

void SensorManager::setServoClassic(uint8_t angle) {
#ifdef ENABLE_SERVO_CLASSIC
    servoClassic.write(constrain(angle, 0, 180));
#endif
}

void SensorManager::setServoCont(int8_t speed) {
#ifdef ENABLE_SERVO_CONT
    // speed -100 až +100 % mapováno na 0..180 (90 = stop)
    int val = map(constrain(speed, -100, 100), -100, 100, 0, 180);
    servoCont.write(val);
#endif
}

void SensorManager::setMotor(int16_t speed) {
#ifdef ENABLE_MOTOR_CTRL
    analogWrite(PIN_MOTOR_CTRL, constrain(speed, 0, 255));
#endif
}

void SensorManager::setLedStrip(const uint32_t leds[8], uint8_t brightness) {
#ifdef ENABLE_LED_STRIP
    strip.setBrightness(brightness);
    for (int i = 0; i < 8; i++) {
        strip.setPixelColor(i, leds[i]);
    }
    strip.show();
#endif
}

void SensorManager::updateOled(const char* line1, const char* line2) {
#ifdef ENABLE_OLED_SSD1306
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("=== SPODNI PANEL ==="));
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    if (line1 && strlen(line1) > 0) {
        display.setCursor(0, 18);
        display.println(line1);
    }
    if (line2 && strlen(line2) > 0) {
        display.setCursor(0, 32);
        display.println(line2);
    }
    display.display();
#endif
}

void SensorManager::applyOutputsFromState() {
    BottomOutputs out = globalState.getOutputs();
    setSmartServo(out.targetSmartServoAngle);
    setServoClassic(out.targetServoAngle);
    setServoCont(out.targetContinuousServo);
    setMotor(out.targetMotorSpeed);
    setLedStrip(out.ledStrip, out.ledBrightness);
    updateOled(out.oledLine1, out.oledLine2);
}
