#include "SensorManager.h"
#include "HardwareSetup.h"
#include <Wire.h>

// Externí instance senzorů a periferií definované v HardwareSetup.cpp
#if defined(ENABLE_DHT) || defined(ENABLE_DHT22)
  #include <DHT.h>
  extern DHT dht;
#endif

#ifdef ENABLE_LSM6DS3
  #include <Adafruit_LSM6DS3.h>
  extern Adafruit_LSM6DS3 lsm6ds3;
#endif

#ifdef ENABLE_VL53L0X
  #include <VL53L0X.h>
  extern VL53L0X vl53;
#endif

#ifdef ENABLE_TCS34725
  #include <Adafruit_TCS34725.h>
  extern Adafruit_TCS34725 tcs;
#endif

#ifdef ENABLE_WS2812B
  #include <Adafruit_NeoPixel.h>
  extern Adafruit_NeoPixel strip;
#endif

#ifdef ENABLE_LCD1602
  #include <LiquidCrystal_I2C.h>
  extern LiquidCrystal_I2C lcd;
#endif

// Globální instance
SensorManager sensorManager;

SensorManager::SensorManager() {
}

// =============================================================================
// 1. ČTENÍ VŠECH SENZORŮ (INPUTS)
// =============================================================================

// Teplota a vlhkost (DHT)
void SensorManager::readDHT() {
#if defined(ENABLE_DHT) || defined(ENABLE_DHT22)
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
        globalState.updateTemperature(t, h);
    }
#endif
}

// Ultrazvukový senzor (HC-SR04)
void SensorManager::readUltrasonic() {
#ifdef ENABLE_ULTRASONIC
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    delayMicroseconds(4);
    digitalWrite(PIN_ULTRASONIC_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    
    long duration = pulseIn(PIN_ULTRASONIC_ECHO, HIGH, 30000UL);
    float distCm = (duration == 0) ? -1.0f : (duration * 0.0343f / 2.0f);
    globalState.updateUltrasonicDistance(distCm);
#endif
}

// Laserový dálkoměr VL53L0X
void SensorManager::readVL53L0X() {
#ifdef ENABLE_VL53L0X
    uint16_t distMm = vl53.readRangeContinuousMillimeters();
    if (!vl53.timeoutOccurred()) {
        globalState.updateLaserDistance(distMm);
    }
#endif
}

// Infračervený senzor vzdálenosti (např. analogový Sharp nebo fototranzistor)
void SensorManager::readIRDistance() {
#ifdef ENABLE_IR_SENSORS
    // Pokud je zapojen analogový IR dálkoměr, přečteme ADC a přepočteme na cm
    #ifdef PIN_IR_ANALOG
        int raw = analogRead(PIN_IR_ANALOG);
        float distCm = (raw > 0) ? (12343.85f / (raw - 15.0f)) : -1.0f;
        globalState.updateIRDistance(distCm);
    #else
        // Fallback pro digitální IR stav (1.0 cm = překážka, -1.0 cm = volno)
        bool obstacle = (digitalRead(PIN_IR1) == LOW);
        globalState.updateIRDistance(obstacle ? 1.0f : -1.0f);
    #endif
#endif
}

// Digitální IR senzor překážek
void SensorManager::readIRObstacle() {
#ifdef ENABLE_IR_SENSORS
    bool obstacle = (digitalRead(PIN_IR1) == LOW); // LOW = detekována překážka
    globalState.updateIRObstacle(obstacle);
#endif
}

// Analogové fotorezistory
void SensorManager::readPhotoresistors() {
#ifdef ENABLE_PHOTORESISTORS
    uint16_t p1 = analogRead(PIN_PHOTO1);
    uint16_t p2 = analogRead(PIN_PHOTO2);
    globalState.updatePhotoresistors(p1, p2);
#endif
}

// Tlačítko na horním panelu
void SensorManager::readTopButton() {
#ifdef ENABLE_BUTTONS
    bool pressed = (digitalRead(PIN_BTN1) == LOW); // PULLUP -> LOW = stisknuto
    globalState.updateTopButton(pressed);
#endif
}

// Barevný senzor TCS34725
void SensorManager::readColorSensor() {
#ifdef ENABLE_TCS34725
    float r = 0, g = 0, b = 0;
    tcs.getRGB(&r, &g, &b);
    globalState.updateColor((uint16_t)r, (uint16_t)g, (uint16_t)b);
#endif
}

// Gyroskop a Akcelerometr LSM6DS3
void SensorManager::readIMU() {
#ifdef ENABLE_LSM6DS3
    sensors_event_t a, g, temp;
    lsm6ds3.getEvent(&a, &g, &temp);
    globalState.updateIMU(a.acceleration.x, a.acceleration.y, a.acceleration.z,
                          g.gyro.x, g.gyro.y, g.gyro.z);
#endif
}

// Hromadné přečtení všech senzorů
void SensorManager::updateAllSensors() {
    readDHT();
    readUltrasonic();
    readVL53L0X();
    readIRDistance();
    readIRObstacle();
    readPhotoresistors();
    readTopButton();
    readColorSensor();
    readIMU();
}

// =============================================================================
// 2. OVLÁDÁNÍ VÝSTUPŮ (OUTPUTS) S AUTOMATICKÝM ZÁPISEM DO SYSTEMSTATE
// =============================================================================

// 3x LED diody
void SensorManager::setLeds(bool l1, bool l2, bool l3) {
#ifdef ENABLE_LEDS
    digitalWrite(PIN_LED1, l1 ? HIGH : LOW);
    digitalWrite(PIN_LED2, l2 ? HIGH : LOW);
    digitalWrite(PIN_LED3, l3 ? HIGH : LOW);
#endif
    globalState.updateLeds(l1, l2, l3); // Uloží stav do SensorData
}

void SensorManager::setLed1(bool on) {
    SensorData current = globalState.getSensorData();
    setLeds(on, current.led2, current.led3);
}

void SensorManager::setLed2(bool on) {
    SensorData current = globalState.getSensorData();
    setLeds(current.led1, on, current.led3);
}

void SensorManager::setLed3(bool on) {
    SensorData current = globalState.getSensorData();
    setLeds(current.led1, current.led2, on);
}

// 8-LED WS2812B RGB pásek
void SensorManager::setLedStrip(const uint32_t leds[8], uint8_t brightness) {
#ifdef ENABLE_WS2812B
    strip.setBrightness(brightness);
    for (int i = 0; i < 8; i++) {
        strip.setPixelColor(i, leds[i]);
    }
    strip.show();
#endif
    globalState.updateLedStripTop(leds, brightness); // Uloží stav do SensorData
}

void SensorManager::setLedStripColor(uint32_t color, uint8_t brightness) {
    uint32_t leds[8];
    for (int i = 0; i < 8; i++) {
        leds[i] = color;
    }
    setLedStrip(leds, brightness);
}

void SensorManager::setLedStripPixel(uint8_t index, uint32_t color) {
    if (index >= 8) return;
#ifdef ENABLE_WS2812B
    strip.setPixelColor(index, color);
    strip.show();
#endif
    globalState.updateLedStripTopLed(index, color); // Uloží stav do SensorData
}

// Pasivní bzučák
void SensorManager::setBuzzer(bool active, uint16_t freq) {
#ifdef ENABLE_BUZZER
    if (active && freq > 0) {
        tone(PIN_BUZZER, freq);
    } else {
        noTone(PIN_BUZZER);
    }
#endif
    globalState.updateBuzzer(active, freq); // Uloží stav do SensorData
}

void SensorManager::beep(uint16_t freq, uint32_t durationMs) {
#ifdef ENABLE_BUZZER
    tone(PIN_BUZZER, freq, durationMs);
#endif
    globalState.updateBuzzer(true, freq);
}

// Sedmisegmentový displej přes posuvný registr 74HC595
void SensorManager::set7Segment(int value) {
#ifdef ENABLE_74HC595
    static const byte segDigits[] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
    };
    digitalWrite(PIN_SEG_LATCH, LOW);
    int temp = value;
    for (int i = 0; i < SEG_NUM_DIGITS; i++) {
        int d = temp % 10;
        byte pattern = (d >= 0 && d <= 9) ? segDigits[d] : 0x00;
        if (SEG_COMMON_ANODE) pattern = ~pattern;
        shiftOut(PIN_SEG_DATA, PIN_SEG_CLK, MSBFIRST, pattern);
        temp /= 10;
    }
    digitalWrite(PIN_SEG_LATCH, HIGH);
#endif
    globalState.update7Segment(value); // Uloží číslo do SensorData
}

// I2C LCD 1602
void SensorManager::writeLCD1602(const char* line1, const char* line2) {
#ifdef ENABLE_LCD1602
    if (line1) {
        lcd.setCursor(0, 0);
        lcd.print("                ");
        lcd.setCursor(0, 0);
        lcd.print(line1);
    }
    if (line2) {
        lcd.setCursor(0, 1);
        lcd.print("                ");
        lcd.setCursor(0, 1);
        lcd.print(line2);
    }
#endif

////////////////////////////////////////////////
void setBottomServo(uint8_t angle){
    global.state.updateServo(angle);
    global.state.popBottomNeedsTx(true);
}
void setBottomSmartServo(int16_t angle);
void setBottomMotor(int16_t speed);
void setBottomContinuousServo(int8_t speed);
void setBottomLedStrip(const uint32_t leds[8], uint8_t brightness);
void setBottomOledText();
///////////////////////////////////////////////
}
