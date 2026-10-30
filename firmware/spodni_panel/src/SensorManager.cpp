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

// =============================================================================
// Stavový automat pro rotační enkodér (přesně z větve encoder_upgrade)
// =============================================================================
#ifdef ENABLE_ENCODER
#define ENC_DIR_NONE 0x00
#define ENC_DIR_CW   0x10
#define ENC_DIR_CCW  0x20

static const uint8_t PROGMEM enc_state_table[7][4] = {
    // 00: START
    {0x0, 0x2, 0x4, 0x0},
    // 01: CW_FINAL
    {0x3, 0x0, 0x1, 0x10},
    // 02: CW_BEGIN
    {0x3, 0x2, 0x0, 0x0},
    // 03: CW_NEXT
    {0x3, 0x2, 0x1, 0x0},
    // 04: CCW_BEGIN
    {0x6, 0x0, 0x4, 0x0},
    // 05: CCW_FINAL
    {0x6, 0x5, 0x0, 0x20},
    // 06: CCW_NEXT
    {0x6, 0x5, 0x4, 0x0},
};

static volatile int32_t isrEncoderPos = 0;
static volatile uint8_t isrEncoderState = 0;
static volatile unsigned long lastIsrStepTime = 0;

static void IRAM_ATTR encoderISR() {
    uint8_t pinState = (digitalRead(PIN_ENC_CLK) << 1) | digitalRead(PIN_ENC_DT);
    isrEncoderState = pgm_read_byte(&enc_state_table[isrEncoderState & 0x0F][pinState]);
    uint8_t result = isrEncoderState & 0x30;

    if (result == ENC_DIR_CW || result == ENC_DIR_CCW) {
        unsigned long nowUs = micros();
        // Ochranné okno 60 ms (60 000 us) přímo v přerušení:
        // Spolehlivě eliminuje dozvuky mechanické pružinky zubu (cca 35-50 ms),
        // ale zachovává bleskovou odezvu na lidské otáčení.
        if (nowUs - lastIsrStepTime >= 60000) {
            lastIsrStepTime = nowUs;
            if (result == ENC_DIR_CW) {
                isrEncoderPos = (isrEncoderPos + 1) % 12;
            } else {
                isrEncoderPos = (isrEncoderPos - 1 + 12) % 12;
            }
        }
    }
}

void setupEncoderInterrupts() {
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_DT), encoderISR, CHANGE);
}
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

void SensorManager::readLimitSwitches() {
#ifdef ENABLE_LIMIT_SWITCHES
    bool minLimit = (digitalRead(PIN_LIMIT_MIN) == LOW);
    bool maxLimit = (digitalRead(PIN_LIMIT_MAX) == LOW);
    globalState.updateLimitSwitches(minLimit, maxLimit);
#endif
}

void SensorManager::readEncoder() {
#ifdef ENABLE_ENCODER
    // Bezpečné vyčtení z přerušení (atomická operace)
    noInterrupts();
    encoderPos = isrEncoderPos;
    interrupts();

    bool btn = false;
#ifdef ENABLE_JOYSTICK
    // Tlačítko joysticku / enkodéru, pokud je zapojeno
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
    readLimitSwitches();
}

// =============================================================================
// 2. ŘÍZENÍ VÝSTUPŮ (OUTPUTS)
// =============================================================================

void SensorManager::setSmartServo(int16_t angle) {
#ifdef ENABLE_SMART_SERVO
    int16_t clampedAngle = constrain(angle, SMART_SERVO_MIN_DEG, SMART_SERVO_MAX_DEG);
    servoBus.set(0, Angle::deg((float)clampedAngle));
#endif
}

void SensorManager::setServoClassic(uint8_t angle) {
#ifdef ENABLE_SERVO_CLASSIC
    servoClassic.write(constrain(angle, 0, 180));
#endif
}

void SensorManager::setServoCont(int8_t speed) {
#ifdef ENABLE_SERVO_CONT
    // Bezpečnostní kontrola koncových spínačů:
    // Pokud je sepnut limitMin (GPIO 25, vlevo, speed > 0), nelze jet dál doleva.
    // Pokud je sepnut limitMax (GPIO 35, vpravo, speed < 0), nelze jet dál doprava.
    BottomInputs in = globalState.getInputs();
    if (in.limitMin && speed > 0) {
        speed = 0; // Zastavit pohyb vlevo
    }
    if (in.limitMax && speed < 0) {
        speed = 0; // Zastavit pohyb vpravo
    }

    // speed -100 až +100 % mapováno na 0..180 (90 = stop)
    int val = map(constrain(speed, -100, 100), -100, 100, 0, 180);
    servoCont.write(val);
#endif
}

#ifdef ENABLE_MOTOR_CTRL
void SensorManager::setMotor(int16_t speed) {
    analogWrite(PIN_MOTOR_CTRL, constrain(speed, 0, 255));
}
#endif

void SensorManager::setLedStrip(const uint32_t leds[8], uint8_t brightness) {
#ifdef ENABLE_LED_STRIP
    strip.setBrightness(brightness);
    for (int i = 0; i < 8; i++) {
        strip.setPixelColor(i, leds[i]);
    }
    strip.show();
#endif
}

void SensorManager::updateOled(const char* line1, const char* line2, const char* line3, const char* line4) {
#ifdef ENABLE_OLED_SSD1306
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("=== SPODNI PANEL ==="));
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    if (line1 && strlen(line1) > 0) {
        display.setCursor(0, 14);
        display.println(line1);
    }
    if (line2 && strlen(line2) > 0) {
        display.setCursor(0, 26);
        display.println(line2);
    }
    if (line3 && strlen(line3) > 0) {
        display.setCursor(0, 38);
        display.println(line3);
    }
    if (line4 && strlen(line4) > 0) {
        display.setCursor(0, 50);
        display.println(line4);
    }
    display.display();
#endif
}

void SensorManager::setOledPower(bool on) {
#ifdef ENABLE_OLED_SSD1306
    if (on) {
        display.ssd1306_command(SSD1306_DISPLAYON);
    } else {
        display.clearDisplay();
        display.display();
        display.ssd1306_command(SSD1306_DISPLAYOFF);
    }
#endif
}

void SensorManager::updateAutoStopParams(int16_t diffCentideg, uint8_t readings) {
#ifdef ENABLE_SMART_SERVO
    SmartServoBus::AutoStopParams params = {
        .max_diff_centideg = diffCentideg,
        .max_diff_readings = readings
    };
    servoBus.setAutoStopParams(params);
#endif
}

void SensorManager::applyOutputsFromState() {
    BottomOutputs out = globalState.getOutputs();
    setSmartServo(out.targetSmartServoAngle);
    setServoClassic(out.targetServoAngle);
    setServoCont(out.targetContinuousServo);
#ifdef ENABLE_MOTOR_CTRL
    setMotor(out.targetMotorSpeed);
#endif
    setLedStrip(out.ledStrip, out.ledBrightness);
    updateOled(out.oledLine1, out.oledLine2);
}
