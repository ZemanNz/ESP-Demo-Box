#include <Arduino.h>
#include "config.h"
#include "SystemState.h"
#include "HardwareSetup.h"
#include "SensorManager.h"
#include "UartProtocol.h"

// ---------------------------------------------------------
// Fyzická instance Globálního Stavu spodního panelu
// ---------------------------------------------------------
SystemState globalState;

// ---------------------------------------------------------
// 1. Task: UART Komunikace s Horním panelem (Core 0)
// ---------------------------------------------------------
void Task_UART(void *pvParameters) {
#ifdef ENABLE_UART_TOP
    Serial.print("[TASK] Task_UART bezi na jadre: ");
    Serial.println(xPortGetCoreID());

    unsigned long lastSendTime = 0;
    TopToBottomPacket inPacket;
    BottomToTopPacket outPacket;

    for (;;) {
        unsigned long now = millis();

        // ---------------------------------------------------------
        // 1. PŘÍJEM PŘÍKAZŮ Z HORNÍHO PANELU (TOP -> BOTTOM)
        // ---------------------------------------------------------
        while (SerialTop.available() >= sizeof(TopToBottomPacket)) {
            // Kontrola startovního bajtu 0xAA
            if (SerialTop.peek() != UART_FRAME_START_TOP_TO_BOTTOM) {
                SerialTop.read(); // Zahodíme poškozený bajt a hledáme začátek
                continue;
            }

            // Přečtení celého paketu
            SerialTop.readBytes((uint8_t*)&inPacket, sizeof(TopToBottomPacket));

            // Výpočet a ověření kontrolního součtu
            uint8_t calculatedCRC = calculateChecksum(
                (const uint8_t*)&inPacket,
                sizeof(TopToBottomPacket) - 2
            );

            if (inPacket.endByte == UART_FRAME_END && inPacket.checksum == calculatedCRC) {
                // Uložíme příkazy do SystemState pro aplikaci na hardware
                globalState.applyTopPacket(inPacket);
            } else {
                Serial.println("[UART] Chyba kontrolniho souctu prikazu! Paket zahozen.");
            }
        }

        // ---------------------------------------------------------
        // 2. ODESÍLÁNÍ TELEMETRIE DO HORNÍHO PANELU (BOTTOM -> TOP)
        // ---------------------------------------------------------
        bool hasChanged = globalState.popNeedsTx();
        bool heartbeatTimeout = (now - lastSendTime >= 50); // Heartbeat / stream perioda 50 ms

        if (hasChanged || heartbeatTimeout) {
            lastSendTime = now;

            BottomInputs in = globalState.getInputs();

            outPacket.startByte = UART_FRAME_START_BOTTOM_TO_TOP; // 0x55
            outPacket.joyX = in.joyX;
            outPacket.joyY = in.joyY;
            outPacket.joyBtn = in.joyBtn;

            for (int i = 0; i < 5; i++) {
                outPacket.btnDown[i] = in.btnDown[i];
            }

            outPacket.encoderPos = in.encoderPos;
            outPacket.encoderDelta = in.encoderDelta;
            outPacket.encoderBtn = in.encoderBtn;

            outPacket.potentiometer = in.potentiometer;
            
            BottomOutputs out = globalState.getOutputs();
            outPacket.currentSmartServoAngle = out.targetSmartServoAngle;
            outPacket.currentServoAngle = out.targetServoAngle;
            outPacket.currentContinuousServo = out.targetContinuousServo;
            outPacket.currentMotorSpeed = out.targetMotorSpeed;

            for (int i = 0; i < 8; i++) {
                outPacket.ledStrip[i] = out.ledStrip[i];
            }
            outPacket.ledBrightness = out.ledBrightness;

            outPacket.switch1 = in.switch1;
            outPacket.switch2 = in.switch2;
            outPacket.endByte = UART_FRAME_END; // 0xFE

            outPacket.checksum = calculateChecksum(
                (const uint8_t*)&outPacket,
                sizeof(BottomToTopPacket) - 2
            );

            SerialTop.write((const uint8_t*)&outPacket, sizeof(BottomToTopPacket));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
#else
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif
}

// ---------------------------------------------------------
// 2. Task: Senzory, Vstupy a Výstupy (Core 1)
// ---------------------------------------------------------
void Task_Sensors(void *pvParameters) {
    Serial.print("[TASK] Task_Sensors bezi na jadre: ");
    Serial.println(xPortGetCoreID());

    unsigned long lastPrintTime = 0;
    uint8_t ledHue = 0;

    for (;;) {
        // 1. Přečteme veškeré vstupy (Joystick, Pot, Tlačítka, Switche, Enkodér, Dorazy)
        sensorManager.updateAllInputs();

        BottomInputs in = globalState.getInputs();
        int16_t smartAngle = sensorManager.getSmartServoAngle();

        unsigned long now = millis();

        // 2. Periodický přehledný výpis do Serial Monitoru (každých 250 ms)
        if (now - lastPrintTime >= 250) {
            lastPrintTime = now;

            Serial.printf("[TEST] POT: %4d | JOY: X=%4d Y=%4d SW=%d | BTN: [%d %d %d %d %d] | SW: [%d %d] | LIMIT: [MIN=%d MAX=%d] | ENC: %4d | SMART: %3d deg\n",
                          in.potentiometer,
                          in.joyX, in.joyY, in.joyBtn ? 1 : 0,
                          in.btnDown[0] ? 1 : 0, in.btnDown[1] ? 1 : 0, in.btnDown[2] ? 1 : 0, in.btnDown[3] ? 1 : 0, in.btnDown[4] ? 1 : 0,
                          in.switch1 ? 1 : 0, in.switch2 ? 1 : 0,
                          in.limitMin ? 1 : 0, in.limitMax ? 1 : 0,
                          in.encoderPos,
                          smartAngle);

            // 3. Živé zobrazení na OLED displeji
            char line1[17];
            char line2[17];
            snprintf(line1, sizeof(line1), "P:%4d J:%d,%d", in.potentiometer, in.joyX / 100, in.joyY / 100);
            snprintf(line2, sizeof(line2), "B:%d%d%d%d%d L:%d,%d",
                     in.btnDown[0], in.btnDown[1], in.btnDown[2], in.btnDown[3], in.btnDown[4],
                     in.limitMin, in.limitMax);
            sensorManager.updateOled(line1, line2);

            // 4. Efekt plynulé změny barvy na LED pásku
            uint32_t colors[8];
            for (int i = 0; i < 8; i++) {
                uint8_t hue = (ledHue + (i * 32)) & 255;
                // Převod HSV/Hue na 24-bit RGB barvu
                uint8_t region = hue / 43;
                uint8_t remainder = (hue - (region * 43)) * 6;
                uint8_t q = 255 - remainder;
                uint8_t t = remainder;
                uint8_t r = 0, g = 0, b = 0;
                switch (region) {
                    case 0: r = 255; g = t; b = 0; break;
                    case 1: r = q; g = 255; b = 0; break;
                    case 2: r = 0; g = 255; b = t; break;
                    case 3: r = 0; g = q; b = 255; break;
                    case 4: r = t; g = 0; b = 255; break;
                    default: r = 255; g = 0; b = q; break;
                }
                colors[i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            }
            sensorManager.setLedStrip(colors, 20);
            ledHue += 5;
        }

        vTaskDelay(pdMS_TO_TICKS(20)); // Perioda čtení vstupů 20 ms (50 Hz)
    }
}

// ---------------------------------------------------------
// Hlavní SETUP
// ---------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- ESP-Demo-Box: Start Spodniho Panelu ---");

    // 1. Inicializace hardwaru
    bool hwOk = initializeAllHardware();

    // 2. Synchronizační Boot Handshake s Horním panelem
    waitForHandshakeWithTop(hwOk, lastHardwareError);

    // 3. Spuštění FreeRTOS úloh
#ifdef ENABLE_UART_TOP
    xTaskCreatePinnedToCore(Task_UART, "UART_Bottom", 4096, NULL, 2, NULL, 0); // Core 0
#endif
    xTaskCreatePinnedToCore(Task_Sensors, "Sensors_Bottom", 4096, NULL, 1, NULL, 1); // Core 1

    Serial.println("[SYSTEM] Vsechna vlakna spodniho panelu uspesne spustena!");
}

// ---------------------------------------------------------
// Hlavní smyčka loop
// ---------------------------------------------------------
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}