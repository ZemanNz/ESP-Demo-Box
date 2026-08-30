#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "UartProtocol.h"

// ---------------------------------------------------------
// 1. Struktury pro data spodního panelu
// ---------------------------------------------------------
enum AppMode {
    MODE_MAIN_MENU,
    MODE_SENSORS,
    MODE_GAME_SNAKE,
    MODE_GAME_FLAPPY,
    MODE_2048,
    MODE_VZDALENOST,
    MODE_WIFI_SPOJENI,
    MODE_WIFI_DETECTION,
    MODE_SERVA,
    MODE_MOTOR,
    MODE_BAREVNY,
    MODE_SLEEP
};

struct BottomInputs {
    // Joystick
    int16_t  joyX;
    int16_t  joyY;
    bool     joyBtn;

    // 5x Tlačítka
    bool     btnDown[5];

    // Rotační enkodér
    int32_t  encoderPos;
    int16_t  encoderDelta;
    bool     encoderBtn;

    // Potenciometr
    uint16_t potentiometer;

    // Páčkové přepínače
    bool     switch1;
    bool     switch2;

    // Reálný stav/zpětná vazba z akčních členů
    int16_t  currentSmartServoAngle;
    uint8_t  currentServoAngle;
    int8_t   currentContinuousServo;
    int16_t  currentMotorSpeed;
};

struct BottomOutputs {
    uint8_t  currentMode;
    bool     overrideAutonomy;

    // Požadované stavy z horního panelu
    int16_t  targetSmartServoAngle;
    uint8_t  targetServoAngle;
    int8_t   targetContinuousServo;
    int16_t  targetMotorSpeed;

    // LED pásek
    uint32_t ledStrip[8];
    uint8_t  ledBrightness;

    // 0.96" OLED displej
    char     oledLine1[17];
    char     oledLine2[17];
};

// ---------------------------------------------------------
// 2. Hlavní sdílená třída (Globální stav spodního panelu)
// ---------------------------------------------------------
class SystemState {
private:
    AppMode       Mode;
    BottomInputs  inputs;
    BottomOutputs outputs;
    bool          needsTx;
    bool         needsUiUpdate;
    SemaphoreHandle_t stateMutex;

public:
    SystemState() {
        memset(&inputs, 0, sizeof(BottomInputs));
        memset(&outputs, 0, sizeof(BottomOutputs));
        outputs.targetServoAngle = 90;
        outputs.ledBrightness = 50;
        needsTx = true;
        needsUiUpdate = true;
        Mode = (AppMode)outputs.currentMode;
        stateMutex = xSemaphoreCreateMutex();
    }

    AppMode getMode() {
        AppMode copy;
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            copy = Mode;
            xSemaphoreGive(stateMutex);
        }
        return copy;
    }

    // -----------------------------------------------------
    // VSTUPY (Čtení a zápis ze senzorů do stavu)
    // -----------------------------------------------------
    void updateJoystick(int16_t x, int16_t y, bool btn) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            if (abs(inputs.joyX - x) > 15 || abs(inputs.joyY - y) > 15 || inputs.joyBtn != btn) {
                inputs.joyX = x;
                inputs.joyY = y;
                inputs.joyBtn = btn;
                needsTx = true;
            }
            xSemaphoreGive(stateMutex);
        }
    }

    void updateButtons(const bool btns[5]) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            bool changed = false;
            for (int i = 0; i < 5; i++) {
                if (inputs.btnDown[i] != btns[i]) {
                    inputs.btnDown[i] = btns[i];
                    changed = true;
                }
            }
            if (changed) needsTx = true;
            xSemaphoreGive(stateMutex);
        }
    }

    void updateButton(uint8_t index, bool pressed) {
        if (index >= 5) return;
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            if (inputs.btnDown[index] != pressed) {
                inputs.btnDown[index] = pressed;
                needsTx = true;
            }
            xSemaphoreGive(stateMutex);
        }
    }

    void updateEncoder(int32_t pos, int16_t delta, bool btn) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            if (inputs.encoderPos != pos || inputs.encoderBtn != btn || delta != 0) {
                inputs.encoderPos = pos;
                inputs.encoderDelta = delta;
                inputs.encoderBtn = btn;
                needsTx = true;
            }
            xSemaphoreGive(stateMutex);
        }
    }

    void updatePotentiometer(uint16_t val) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            if (abs((int)inputs.potentiometer - (int)val) > 15) {
                inputs.potentiometer = val;
                needsTx = true;
            }
            xSemaphoreGive(stateMutex);
        }
    }

    void updateSwitches(bool s1, bool s2) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            if (inputs.switch1 != s1 || inputs.switch2 != s2) {
                inputs.switch1 = s1;
                inputs.switch2 = s2;
                needsTx = true;
            }
            xSemaphoreGive(stateMutex);
        }
    }

    void updateActuatorFeedback(int16_t smartAngle, uint8_t servoAngle, int8_t contSpeed, int16_t motorSpd, uint32_t ledStrip[8] = nullptr, uint8_t ledBrightness = 0) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            inputs.currentSmartServoAngle = smartAngle;
            inputs.currentServoAngle = servoAngle;
            inputs.currentContinuousServo = contSpeed;
            inputs.currentMotorSpeed = motorSpd;
            needsTx = true;

            xSemaphoreGive(stateMutex);
        }
    }

    BottomInputs getInputs() {
        BottomInputs copy;
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            copy = inputs;
            xSemaphoreGive(stateMutex);
        }
        return copy;
    }

    bool popNeedsTx() {
        bool res = false;
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            res = needsTx;
            needsTx = false;
            xSemaphoreGive(stateMutex);
        }
        return res;
    }

    bool popNeedsUiUpdate() {
        bool res = false;
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            res = needsUiUpdate;
            needsUiUpdate = false;
            xSemaphoreGive(stateMutex);
        }
        return res;
    }

    // -----------------------------------------------------
    // VÝSTUPY (Příkazy z UARTu od horního panelu)
    // -----------------------------------------------------
    void applyTopPacket(const TopToBottomPacket& packet) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            if (outputs.currentMode != packet.currentMode) {
                Mode = (AppMode)packet.currentMode;
                needsUiUpdate = true;
            }
            outputs.currentMode = packet.currentMode;
            outputs.overrideAutonomy = packet.overrideAutonomy;
            outputs.targetSmartServoAngle = packet.targetSmartServoAngle;
            outputs.targetServoAngle = packet.targetServoAngle;
            outputs.targetContinuousServo = packet.targetContinuousServo;
            outputs.targetMotorSpeed = packet.targetMotorSpeed;

            for (int i = 0; i < 8; i++) {
                outputs.ledStrip[i] = packet.ledStrip[i];
            }
            outputs.ledBrightness = packet.ledBrightness;

            strncpy(outputs.oledLine1, packet.oledLine1, 16);
            outputs.oledLine1[16] = '\0';
            strncpy(outputs.oledLine2, packet.oledLine2, 16);
            outputs.oledLine2[16] = '\0';

            xSemaphoreGive(stateMutex);
        }
    }

    BottomOutputs getOutputs() {
        BottomOutputs copy;
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            copy = outputs;
            xSemaphoreGive(stateMutex);
        }
        return copy;
    }
};

extern SystemState globalState;

#endif // SYSTEM_STATE_H
