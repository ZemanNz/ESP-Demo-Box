#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/semphr.h>
#include "SystemState.h"
#include "config.h"

/**
 * @brief Režim buzení bzučáku přes LEDC PWM.
 */
enum BuzzerDriveMode : uint8_t {
    BUZZER_MODE_TONE = 0, // Tónový režim: mění frekvenci tónu (pasivní bzučák) i střídu 0..50 %
    BUZZER_MODE_PWM  = 1  // Výkonový PWM režim: pevná vysoká nosná (18 kHz), mění střídu 0..100 %
};

/**
 * @brief Jedna nota v neblokující herní nebo testovací melodii.
 */
struct BuzzerNote {
    uint16_t freq;       // Frekvence v Hz (0 = pauza/ticho po dobu durationMs)
    uint16_t durationMs; // Délka znění tónu v ms
    uint16_t pauseMs;    // Krátká mezera ticha za tónem v ms
};

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
    bool readTopButtonRaw();       // Přečte čistý stav pinu PIN_BTN1 (true = stisknuto)
    void readColorSensor();     // Barevný senzor (TCS34725)
    // Gyroskop a akcelerometr
    void calibrateIMU();
    void readIMU();
    void resetIMUAngles(float x = 0.0f, float y = 0.0f, float z = 0.0f);

    // Hromadné přečtení všech senzorů
    void updateAllSensors();
    void updateAll() { updateAllSensors(); } // Alias
    void readAllSensors() { updateAllSensors(); } // Alias

    // =====================================================
    // 2. OVLÁDÁNÍ VÝSTUPŮ (OUTPUTS) -> HW AKCE + ZÁPIS STAVU DO SYSTEMSTATE
    // =====================================================
    
    // LED diody (Fyzicky rozsvítí/zhasne a uloží stav do SensorData)
    void setLeds(bool l1, bool l2, bool l3);
    void setLed1(bool on);
    void setLed2(bool on);
    void setLed3(bool on);
    void setColorSensorLed(bool on);
    void setColorSensorLedBrightness(uint8_t brightness); // PWM regulace 0 až 255 (limitováno na TCS_LED_MAX_BRIGHT)

    // 8-LED RGB pásek WS2812B (Fyzicky zobrazí a uloží barvy do SensorData)
    void setLedStrip(const uint32_t leds[8], uint8_t brightness = 60);
    void setLedStripColor(uint32_t color, uint8_t brightness = 60);
    void setLedStripPixel(uint8_t index, uint32_t color);

    // Vestavěná RGB LED na desce ESP32-S3 (GPIO 38)
    void setBuiltinRgb(uint8_t r, uint8_t g, uint8_t b);

    // Bzučák (LEDC PWM řízení hlasitosti, tónů a neblokujících herních melodií)
    void initBuzzer();
    void setBuzzer(bool active, uint16_t freq = 2000);
    void setBuzzerEx(bool active, uint16_t freq, uint8_t volume, BuzzerDriveMode mode = BUZZER_MODE_TONE);
    void beep(uint16_t freq = 1000, uint32_t durationMs = 100);
    void beep(uint16_t freq, uint32_t durationMs, uint8_t volume, BuzzerDriveMode mode = BUZZER_MODE_TONE);
    void playMelody(const BuzzerNote* notes, uint8_t count, uint8_t volume = 255, BuzzerDriveMode mode = BUZZER_MODE_TONE);
    void playScoreSound();
    void playSnakeEatSound();
    void playTetrisLineSound(uint8_t lines = 1);
    void playGameStartSound();
    void playGameOverSound();
    void playHighScoreResetSound();
    void playMotionAlertSound();
    void stopBuzzer();
    bool isBuzzerPlaying() const;
    void setBuzzerVolume(uint8_t volume);
    uint8_t getBuzzerVolume() const { return buzzerVolume; }
    void setBuzzerDriveMode(BuzzerDriveMode mode) { buzzerDriveMode = mode; }
    BuzzerDriveMode getBuzzerDriveMode() const { return buzzerDriveMode; }
    void prepareBuzzerForSleep();
    void restoreBuzzerAfterSleep();

    // Sedmisegmentový displej přes 74HC595 (Zobrazí číslo a uloží do SensorData)
    void set7Segment(int value);
    void set7SegmentText(const char* text);

    // I2C LCD 1602 displej
    void writeLCD1602(const char* line1, const char* line2 = nullptr);
    void writeLCD1602(const String& line1, const String& line2 = "");
    void setLCDBacklight(bool on);

    ///////////////////////
    // Ovládáni spodních periferii (OUTPUTS)
    //////////////////////
    void setBottomServo(uint8_t angle);
    void setBottomSmartServo(int16_t angle);
    void setBottomMotor(int16_t speed);
    void setBottomContinuousServo(int8_t speed);
    void setBottomLedStrip(const uint32_t leds[8], uint8_t brightness);
    void setBottomOledText();

private:
    // Stav pro komplementární filtr a integraci IMU (LSM6DS3)
    float imuAngleX = 0.0f;
    float imuAngleY = 0.0f;
    float imuAngleZ = 0.0f;
    float gyroBiasX = 0.0f;
    float gyroBiasY = 0.0f;
    float gyroBiasZ = 0.0f;
    float accelInitAngleX = 0.0f;
    float accelInitAngleZ = 0.0f;
    float lastGx = 0.0f;
    float lastGy = 0.0f;
    float lastGz = 0.0f;
    bool imuCalibrated = false;
    unsigned long lastImuTimeUs = 0;

    // Stav LEDC PWM bzučáku a neblokující fronty melodií
    static constexpr uint8_t BUZZER_MAX_MELODY_NOTES = 12;
    portMUX_TYPE buzzerMux = portMUX_INITIALIZER_UNLOCKED;
    SemaphoreHandle_t buzzerHwMutex = nullptr;
    esp_timer_handle_t buzzerTimer = nullptr;
    bool buzzerInitialized = false;
    bool buzzerAttached = false;
    bool buzzerSlept = false;
    uint32_t currentBuzzerLedcFreq = 0;
    uint8_t buzzerVolume = BUZZER_DEFAULT_VOLUME;
    BuzzerDriveMode buzzerDriveMode = BUZZER_MODE_TONE;

    BuzzerNote melodyQueue[BUZZER_MAX_MELODY_NOTES] = {};
    uint8_t melodyLen = 0;
    uint8_t melodyIdx = 0;
    bool melodyInGap = false;
    bool melodyPlaying = false;
    uint8_t activeMelodyVol = BUZZER_DEFAULT_VOLUME;
    BuzzerDriveMode activeMelodyMode = BUZZER_MODE_TONE;

    void applyBuzzerHardware(bool active, uint16_t freq, uint8_t volume, BuzzerDriveMode mode);
    void startNextMelodyStep();
    static void buzzerTimerCallback(void* arg);
};

extern SensorManager sensorManager;

#endif // SENSOR_MANAGER_H
