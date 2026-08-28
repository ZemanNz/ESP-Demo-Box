#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "SystemState.h"
#include "config.h"

// ---------------------------------------------------------
// Třída SensorManager pro čtení všech vstupů i řízení akčních členů dolního panelu
// ---------------------------------------------------------
class SensorManager {
private:
    int lastClkState;
    int32_t encoderPos;
    int32_t lastReportedEncoderPos;

public:
    SensorManager();

    // =====================================================
    // 1. ČTENÍ VŠECH VSTUPŮ (INPUTS) -> ZÁPIS DO SYSTEMSTATE
    // =====================================================
    void readJoystick();
    void readPotentiometer();
    void readButtons();
    void readSwitches();
    void readEncoder();

    // Přímé vyčtení reálného úhlu z chytrého serva LX-16A přes UART
    int16_t getSmartServoAngle();

    void updateAllInputs();
    void updateAll() { updateAllInputs(); } // Alias

    // =====================================================
    // 2. ŘÍZENÍ VÝSTUPŮ (OUTPUTS)
    // =====================================================
    void setSmartServo(int16_t angle);
    void setServoClassic(uint8_t angle);
    void setServoCont(int8_t speed);
    void setMotor(int16_t speed);
    void setLedStrip(const uint32_t leds[8], uint8_t brightness = 50);
    void updateOled(const char* line1, const char* line2 = nullptr);

    // Aplikuje všechny požadované stavy ze SystemState na fyzický HW
    void applyOutputsFromState();
};

extern SensorManager sensorManager;

#endif // SENSOR_MANAGER_H
