#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h> // Knihovna pro Mutexy a Semafory

// ---------------------------------------------------------
// 1. Definice všech možných stavů aplikace (Stavový automat)
// ---------------------------------------------------------
enum AppMode {
    MODE_MAIN_MENU,
    MODE_SENSORS,
    MODE_GAME_SNAKE,
    MODE_GAME_FLAPPY,
    MODE_2048,
    MODE_VZDALENOST,
    MODE_WIFI_SPOJENI,
    MODE_SERVA,
    MODE_MOTOR,
    MODE_BAREVNY,
    MODE_SLEEP
};

// ---------------------------------------------------------
// 2. Struktura držící aktuální data všech senzorů, tlačítek a výstupů
// ---------------------------------------------------------
struct SensorData {
    // --- Teplota a vlhkost (DHT11 / DHT22) ---
    float temperature;
    float humidity;

    // --- 3x Jednoduché LED diody (Stav zapnuto / vypnuto) ---
    bool led1;
    bool led2;
    bool led3;

    // --- Vzdálenostní senzory ---
    float irDistanceCm;          // Infračervený senzor vzdálenosti
    float ultrasonicDistanceCm;  // HC-SR04 ultrazvuk
    uint16_t laserDistanceMm;    // VL53L0X laser (ToF)

    // --- Gyroskop a Akcelerometr (LSM6DS3) ---
    float accelX, accelY, accelZ;
    float gyroX, gyroY, gyroZ;

    // --- 2x Analogové fotorezistory ---
    uint16_t photo1;
    uint16_t photo2;

    // --- Tlačítko na horním panelu ---
    bool btnTop;

    // --- Digitální infračervený senzor překážek ---
    bool irObstacle;
    bool irSensor1; // Alias pro zpětnou kompatibilitu

    // --- 8x RGB LED pásek - HORNÍ PANEL (WS2812B) ---
    uint32_t ledStripTop[8];
    uint8_t  ledStripTopBrightness;

    // --- Barevný senzor (TCS34725) ---
    uint16_t colorR, colorG, colorB;
    uint16_t colorClear;

    // --- Bzučák (Stav a frekvence) ---
    bool     buzzerActive;
    uint16_t buzzerFreq;

    // --- Sedmisegmentový displej (74HC595 hodnota) ---
    int segmentValue;

    // --- Analogový Joystick (X, Y) a stisk ---
    int16_t joyX;
    int16_t joyY;
    bool joyBtn;

    // --- 5x Tlačítka na dolním panelu ---
    bool btnDown[5]; // Index 0..4 (Tlačítka 1 až 5)

    // --- Rotační enkodér (Pozice, změna/delta, stisk) ---
    int32_t encoderPos;
    int16_t encoderDelta;
    bool encoderBtn;

    // --- Potenciometr ---
    uint16_t potentiometer;

    // --- Chytré servo ---
    int16_t smartServoAngle;

    // --- Hloupé (klasické hobby) servo (0-180°) ---
    uint8_t servoAngle;

    // --- 8x RGB LED pásek - DOLNÍ PANEL (WS2812B) ---
    uint32_t ledStripBottom[8];
    uint8_t  ledStripBottomBrightness;

    // --- 0.96" OLED displej - DOLNÍ PANEL (Textové řádky) ---
    char bottomOledLine1[17];
    char bottomOledLine2[17];

    // --- Motor (Rychlost PWM) ---
    int16_t motorSpeed;

    // --- Kontinuální servo (360° servo, rychlost / směr) ---
    int8_t continuousServoSpeed;

    // --- 2x Switche (Páčkové přepínače) ---
    bool switch1;
    bool switch2;
};

// ---------------------------------------------------------
// 3. Hlavní sdílená třída (Globální Stav)
// ---------------------------------------------------------
class SystemState {
private:
    AppMode currentMode;
    AppMode lastMode;
    int menuCursorIndex;
    SensorData sensors;
    bool uiNeedsUpdate;
    bool bottomNeedsTx;
    SemaphoreHandle_t stateMutex;

public:
    SystemState() {
        currentMode = MODE_MAIN_MENU;
        lastMode = MODE_MAIN_MENU;
        menuCursorIndex = 0;
        uiNeedsUpdate = true;
        bottomNeedsTx = true;
        memset(&sensors, 0, sizeof(SensorData));
        stateMutex = xSemaphoreCreateMutex();
    }

    // -----------------------------------------------------
    // Gettery (Čtení dat z jiných vláken)
    // -----------------------------------------------------
    
    AppMode getMode() {
        AppMode mode = MODE_MAIN_MENU;
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            mode = currentMode;
            xSemaphoreGive(stateMutex);
        }
        return mode;
    }

    AppMode getLastMode() {
        AppMode mode = MODE_MAIN_MENU;
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            mode = lastMode;
            xSemaphoreGive(stateMutex);
        }
        return mode;
    }

    int getMenuCursorIndex() {
        int idx = 0;
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            idx = menuCursorIndex;
            xSemaphoreGive(stateMutex);
        }
        return idx;
    }

    bool popUiNeedsUpdate() {
        bool needsUpdate = false;
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            needsUpdate = uiNeedsUpdate;
            uiNeedsUpdate = false;
            xSemaphoreGive(stateMutex);
        }
        return needsUpdate;
    }

    bool popBottomNeedsTx() {
        bool needsUpdate = false;
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            needsUpdate = bottomNeedsTx;
            bottomNeedsTx = false;
            xSemaphoreGive(stateMutex);
        }
        return needsUpdate;
    }    
    SensorData getSensorData() {
        SensorData dataCopy;
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            dataCopy = sensors;
            xSemaphoreGive(stateMutex);
        }
        return dataCopy;
    }

    // -----------------------------------------------------
    // Settery pro stavový automat a menu
    // -----------------------------------------------------

    void setMode(AppMode newMode) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            if (currentMode != MODE_SLEEP) {
                lastMode = currentMode; // Ukládáme jen platný pracovní mód
            }
            currentMode = newMode;
            uiNeedsUpdate = true;
            bottomNeedsTx = true;
            xSemaphoreGive(stateMutex);
        }
    }

    void setMenuCursorIndex(int idx) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            menuCursorIndex = idx;
            uiNeedsUpdate = true;
            xSemaphoreGive(stateMutex);
        }
    }

    void moveMenuCursor(int step) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            menuCursorIndex += step;
            uiNeedsUpdate = true;
            xSemaphoreGive(stateMutex);
        }
    }

    // -----------------------------------------------------
    // Settery pro SENZORY A VSTUPY
    // -----------------------------------------------------

    // Teplota a vlhkost (DHT)
    void updateTemperature(float temp, float hum) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.temperature = temp;
            sensors.humidity = hum;
            xSemaphoreGive(stateMutex);
        }
    }

    // Infračervený senzor vzdálenosti
    void updateIRDistance(float distCm) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.irDistanceCm = distCm;
            xSemaphoreGive(stateMutex);
        }
    }

    // Laserový senzor (VL53L0X)
    void updateLaserDistance(uint16_t distMm) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.laserDistanceMm = distMm;
            xSemaphoreGive(stateMutex);
        }
    }

    // Ultrazvukový senzor (HC-SR04)
    void updateUltrasonicDistance(float distCm) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.ultrasonicDistanceCm = distCm;
            xSemaphoreGive(stateMutex);
        }
    }

    // Barevný senzor (TCS34725)
    void updateColor(uint16_t r, uint16_t g, uint16_t b, uint16_t clearVal = 0) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.colorR = r;
            sensors.colorG = g;
            sensors.colorB = b;
            sensors.colorClear = clearVal;
            xSemaphoreGive(stateMutex);
        }
    }

    // Fotorezistory
    void updatePhotoresistors(uint16_t p1, uint16_t p2) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.photo1 = p1;
            sensors.photo2 = p2;
            xSemaphoreGive(stateMutex);
        }
    }

    // Tlačítko na horním panelu
    void updateTopButton(bool pressed) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.btnTop = pressed;
            xSemaphoreGive(stateMutex);
        }
    }

    // Digitální IR senzor překážek
    void updateIRObstacle(bool obstacle) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.irObstacle = obstacle;
            sensors.irSensor1 = obstacle;
            xSemaphoreGive(stateMutex);
        }
    }

    void updateIRSensor(bool ir1) {
        updateIRObstacle(ir1);
    }

    // IMU jednotka (LSM6DS3)
    void updateIMU(float ax, float ay, float az, float gx, float gy, float gz) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.accelX = ax;
            sensors.accelY = ay;
            sensors.accelZ = az;
            sensors.gyroX = gx;
            sensors.gyroY = gy;
            sensors.gyroZ = gz;
            xSemaphoreGive(stateMutex);
        }
    }

    // Joystick (Dolní panel)
    void updateJoystick(int16_t x, int16_t y, bool btn) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.joyX = x;
            sensors.joyY = y;
            sensors.joyBtn = btn;
            xSemaphoreGive(stateMutex);
        }
    }

    // 5x Tlačítka na dolním panelu
    void updateDownButtons(bool b1, bool b2, bool b3, bool b4, bool b5) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.btnDown[0] = b1;
            sensors.btnDown[1] = b2;
            sensors.btnDown[2] = b3;
            sensors.btnDown[3] = b4;
            sensors.btnDown[4] = b5;
            xSemaphoreGive(stateMutex);
        }
    }

    void updateDownButton(uint8_t index, bool pressed) {
        if (index >= 5) return;
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.btnDown[index] = pressed;
            xSemaphoreGive(stateMutex);
        }
    }

    // Rotační enkodér
    void updateEncoder(int32_t pos, int16_t delta, bool btn) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.encoderPos = pos;
            sensors.encoderDelta = delta;
            sensors.encoderBtn = btn;
            xSemaphoreGive(stateMutex);
        }
    }

    // Potenciometr
    void updatePotentiometer(uint16_t value) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.potentiometer = value;
            xSemaphoreGive(stateMutex);
        }
    }

    // Chytré servo (Dolní panel)
    void updateSmartServo(int16_t angle) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.smartServoAngle = angle;
            bottomNeedsTx = true;
            xSemaphoreGive(stateMutex);
        }
    }

    // Hloupé (klasické) servo (Dolní panel)
    void updateServo(uint8_t angle) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.servoAngle = angle;
            bottomNeedsTx = true;
            xSemaphoreGive(stateMutex);
        }
    }

    // DC / Krokový motor (Dolní panel)
    void updateMotor(int16_t speed) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.motorSpeed = speed;
            bottomNeedsTx = true;
            xSemaphoreGive(stateMutex);
        }
    }

    // Kontinuální servo (360°) (Dolní panel)
    void updateContinuousServo(int8_t speed) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.continuousServoSpeed = speed;
            bottomNeedsTx = true;
            xSemaphoreGive(stateMutex);
        }
    }

    // 2x Přepínače (Switche)
    void updateSwitches(bool s1, bool s2) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.switch1 = s1;
            sensors.switch2 = s2;
            xSemaphoreGive(stateMutex);
        }
    }

    // -----------------------------------------------------
    // Settery pro STAVY VÝSTUPŮ A PERIFERIÍ
    // -----------------------------------------------------

    // 3x LED diody na horním panelu
    void updateLeds(bool l1, bool l2, bool l3) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.led1 = l1;
            sensors.led2 = l2;
            sensors.led3 = l3;
            xSemaphoreGive(stateMutex);
        }
    }

    // 8x RGB LED pásek - HORNÍ PANEL (WS2812B)
    void updateLedStripTopLed(uint8_t index, uint32_t color) {
        if (index >= 8) return;
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.ledStripTop[index] = color;
            xSemaphoreGive(stateMutex);
        }
    }

    void updateLedStripTop(const uint32_t leds[8], uint8_t brightness = 60) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            for (int i = 0; i < 8; i++) {
                sensors.ledStripTop[i] = leds[i];
            }
            sensors.ledStripTopBrightness = brightness;
            xSemaphoreGive(stateMutex);
        }
    }

    // 8x RGB LED pásek - DOLNÍ PANEL (WS2812B)
    void updateLedStripBottomLed(uint8_t index, uint32_t color) {
        if (index >= 8) return;
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.ledStripBottom[index] = color;
            bottomNeedsTx = true;
            xSemaphoreGive(stateMutex);
        }
    }

    void updateLedStripBottom(const uint32_t leds[8], uint8_t brightness = 60) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            for (int i = 0; i < 8; i++) {
                sensors.ledStripBottom[i] = leds[i];
            }
            sensors.ledStripBottomBrightness = brightness;
            bottomNeedsTx = true;
            xSemaphoreGive(stateMutex);
        }
    }

    // 0.96" OLED displej na dolním panelu
    void updateBottomOled(const char* line1, const char* line2 = nullptr) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            if (line1) {
                strncpy(sensors.bottomOledLine1, line1, 16);
                sensors.bottomOledLine1[16] = '\0';
            }
            if (line2) {
                strncpy(sensors.bottomOledLine2, line2, 16);
                sensors.bottomOledLine2[16] = '\0';
            }
            bottomNeedsTx = true;
            xSemaphoreGive(stateMutex);
        }
    }

    // Bzučák
    void updateBuzzer(bool active, uint16_t freq = 0) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.buzzerActive = active;
            sensors.buzzerFreq = freq;
            xSemaphoreGive(stateMutex);
        }
    }

    // 7-segmentový displej
    void update7Segment(int value) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.segmentValue = value;
            xSemaphoreGive(stateMutex);
        }
    }
};

// Zde deklarujeme, že globalState existuje (fyzicky ho vytvoříme v main.cpp)
extern SystemState globalState;

/*
================================================================================
  PŘÍKLADY POUŽITÍ (CHEATSHEET PRO SYSTEM STATE A MUTEXY)
================================================================================

1. ČTENÍ AKTUÁLNÍHO MÓDU (Např. v grafickém vlákně)
   if (globalState.getMode() == MODE_MAIN_MENU) {
       // Nakresli hlavní menu
   }

2. ZÁPIS HODNOT ZE SENZORU (Např. ve vlákně pro senzory)
   // Nemusíš řešit zamykání (Mutex). Objekt "globalState" to uvnitř
   // funkcí udělá automaticky za tebe!
   globalState.updateTemperature(25.4, 60.1);

3. ZMĚNA MÓDU (Např. když uživatel klikne v menu na "Spustit Hada")
   globalState.setMode(MODE_GAME_SNAKE);
   // setMode automaticky zvedne vlajku "uiNeedsUpdate = true", 
   // takže displej se hned v dalším cyklu překreslí.

4. JAK PŘEKRESLOVAT DISPLEJ JEN KDYŽ JE TO POTŘEBA (Šetří výkon)
   if (globalState.popUiNeedsUpdate()) {
       // Tento blok se spustí JEN TEHDY, když někdo změnil Mód, 
       // nebo pohnul kurzorem. Zde zavoláš překreslení menu.
   }
================================================================================
*/

#endif // SYSTEM_STATE_H
