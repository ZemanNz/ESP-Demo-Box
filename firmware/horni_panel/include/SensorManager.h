#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "SystemState.h"
#include "config.h"

// ---------------------------------------------------------
// Třída SensorManager pro kompletní čtení i ovládání hardware horního panelu
// ---------------------------------------------------------
class SensorManager {
public:
    SensorManager();

    // =====================================================
    // 1. ČTENÍ VŠECH SENZORŮ (INPUTS) -> ZÁPIS DO SYSTEMSTATE
    // =====================================================
    void readDHT();             // Teplota a vlhkost (DHT11 / DHT22)
    void readUltrasonic();      // Ultrazvukový senzor vzdálenosti (HC-SR04)
    void readVL53L0X();         // Laserový ToF senzor vzdálenosti (VL53L0X)
    void readIRDistance();      // Infračervený senzor vzdálenosti
    void readIRObstacle();      // Digitální IR senzor detekce překážky
    void readPhotoresistors();  // 2x Analogový fotorezistor
    void readTopButton();       // Tlačítko na horním panelu
    void readColorSensor();     // Barevný senzor (TCS34725)
    void readIMU();             // Gyroskop a akcelerometr (LSM6DS3)

    // Hromadné přečtení všech senzorů
    void updateAllSensors();
    void updateAll() { updateAllSensors(); } // Alias

    // =====================================================
    // 2. OVLÁDÁNÍ VÝSTUPŮ (OUTPUTS) -> HW AKCE + ZÁPIS STAVU DO SYSTEMSTATE
    // =====================================================
    
    // LED diody (Fyzicky rozsvítí/zhasne a uloží stav do SensorData)
    void setLeds(bool l1, bool l2, bool l3);
    void setLed1(bool on);
    void setLed2(bool on);
    void setLed3(bool on);

    // 8-LED RGB pásek WS2812B (Fyzicky zobrazí a uloží barvy do SensorData)
    void setLedStrip(const uint32_t leds[8], uint8_t brightness = 60);
    void setLedStripColor(uint32_t color, uint8_t brightness = 60);
    void setLedStripPixel(uint8_t index, uint32_t color);

    // Pasivní bzučák (Spustí/zastaví tón a uloží stav do SensorData)
    void setBuzzer(bool active, uint16_t freq = 1000);
    void beep(uint16_t freq = 1000, uint32_t durationMs = 100);

    // Sedmisegmentový displej přes 74HC595 (Zobrazí číslo a uloží do SensorData)
    void set7Segment(int value);

    // I2C LCD 1602 displej
    void writeLCD1602(const char* line1, const char* line2 = nullptr);

    ///////////////////////
    // Ovládáni spodních periferii (OUTPUTS)
    //////////////////////
    void setBottomServo(uint8_t angle);
    void setBottomSmartServo(int16_t angle);
    void setBottomMotor(int16_t speed);
    void setBottomContinuousServo(int8_t speed);
    void setBottomLedStrip(const uint32_t leds[8], uint8_t brightness);
    void setBottomOledText();
};

extern SensorManager sensorManager;

#endif // SENSOR_MANAGER_H
