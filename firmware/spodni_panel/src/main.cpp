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

    // Sledování stavu přepínače 2 (pro pohyb chytrého serva: ON = zmáčkne/70°, OFF = roztáhne/0°)
    static int lastSwitch2 = -1;
    static int smartTargetDeg = SMART_SERVO_MIN_DEG;

    for (;;) {
        // 1. Přečteme veškeré vstupy (Joystick, Pot, Tlačítka, Switche, Enkodér, Dorazy)
        sensorManager.updateAllInputs();

        BottomInputs in = globalState.getInputs();

        unsigned long now = millis();

        // -----------------------------------------------------------------
        // Řízení klasického (hloupého) serva potenciometrem (0 až 180°):
        // -----------------------------------------------------------------
        int classicServoAngle = map(constrain((int)in.potentiometer, 0, 4095), 0, 4095, 172, 0);
        sensorManager.setServoClassic(classicServoAngle);

        // -----------------------------------------------------------------
        // Řízení chytrého serva LX-16A přepínačem Switch 2 (s LEDkou):
        // Zapnutý (ON / 1) -> zmáčkne se na SMART_SERVO_MAX_DEG (70°)
        // Vypnutý (OFF / 0) -> roztáhne se na SMART_SERVO_MIN_DEG (0°)
        // Příkaz se pošle pouze při změně stavu přepínače, aby AutoStop mohl pracovat!
        // -----------------------------------------------------------------
        int currentSwitch2 = in.switch2 ? 1 : 0;
        if (currentSwitch2 != lastSwitch2) {
            lastSwitch2 = currentSwitch2;
            smartTargetDeg = in.switch2 ? SMART_SERVO_MAX_DEG : SMART_SERVO_MIN_DEG;
            sensorManager.setSmartServo(smartTargetDeg);
            Serial.printf("[SMART_SERVO] Prepinac SW2: %s -> cil: %d deg\n",
                          in.switch2 ? "ON (Zmacknuto)" : "OFF (Roztazeno)", smartTargetDeg);
        }

        // -----------------------------------------------------------------
        // Řízení kontinuálního 360° serva joystickem (osa X):
        // Mrtvá zóna kolem středu (1800 až 2200) -> 0 % rychlost (klid)
        // Doleva (X: 1800 -> 0)    -> plynule rychlost 0 % až +100 % (směr k limitMin)
        // Doprava (X: 2200 -> 4095) -> plynule rychlost 0 % až -100 % (směr k limitMax)
        // -----------------------------------------------------------------
        int8_t contSpeed = 0;
        if (in.joyX < 1800) {
            contSpeed = map(constrain((int)in.joyX, 0, 1800), 1800, 0, 0, 100);
        } else if (in.joyX > 2200) {
            contSpeed = map(constrain((int)in.joyX, 2200, 4095), 2200, 4095, 0, -100);
        } else {
            contSpeed = 0;
        }

        // Pokud je sepnut levý doraz (limitMin / GPIO 25), nepovolíme jízdu doleva
        if (in.limitMin && contSpeed > 0) {
            contSpeed = 0;
        }
        // Pokud je sepnut pravý doraz (limitMax / GPIO 35), nepovolíme jízdu doprava
        if (in.limitMax && contSpeed < 0) {
            contSpeed = 0;
        }

        sensorManager.setServoCont(contSpeed);

        // 2. Periodický přehledný výpis do Serial Monitoru (každých 250 ms)
        if (now - lastPrintTime >= 250) {
            lastPrintTime = now;

            Serial.printf("[TEST] POT: %4d -> CLAS: %3d deg | SW2: %s -> SMART: %2d deg | JOY: X=%4d (CONT: %+4d%%) | BTN: [%d %d %d %d %d] | LIMIT: [MIN=%d MAX=%d]\n",
                          in.potentiometer,
                          classicServoAngle,
                          in.switch2 ? "ON" : "OFF",
                          smartTargetDeg,
                          in.joyX, contSpeed,
                          in.btnDown[0] ? 1 : 0, in.btnDown[1] ? 1 : 0, in.btnDown[2] ? 1 : 0, in.btnDown[3] ? 1 : 0, in.btnDown[4] ? 1 : 0,
                          in.limitMin ? 1 : 0, in.limitMax ? 1 : 0);

            // 3. Živé zobrazení na OLED displeji
            char line1[22];
            char line2[22];
            char line3[22];
            char line4[22];
            // Řádek 1: Klasické servo a potenciometr
            snprintf(line1, sizeof(line1), "Pot:%4d Srv:%3d\xF7", in.potentiometer, classicServoAngle);
            // Řádek 2: Chytré servo a přepínač SW2
            snprintf(line2, sizeof(line2), "SW2:%s Smart:%2d\xF7", in.switch2 ? "ON " : "OFF", smartTargetDeg);
            // Řádek 3: Joystick a kontinuální servo
            snprintf(line3, sizeof(line3), "JoyX:%4d Cont:%+3d%%", in.joyX, contSpeed);
            // Řádek 4: Koncové spínače a tlačítka
            snprintf(line4, sizeof(line4), "Lim:%d,%d Btn:%d%d%d%d%d",
                     in.limitMin, in.limitMax,
                     in.btnDown[0], in.btnDown[1], in.btnDown[2], in.btnDown[3], in.btnDown[4]);
            sensorManager.updateOled(line1, line2, line3, line4);
            sensorManager.updateOled(line1, line2, line3, line4);

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