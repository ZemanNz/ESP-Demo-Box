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

        // ZÁKAZ VYSÍLÁNÍ VE SPÁNKU:
        if (globalState.getOutputs().currentMode == MODE_SLEEP) {
            // V režimu spánku linka drží klidový stav (HIGH) a nevysílá žádné pakety!
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

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
    BottomInputs start = globalState.getInputs();
    int last_stupit_angle = map(constrain((int)start.potentiometer, 0, 4095), 0, 4095, 172, 0);

    for (;;) {
        // 1. Přečteme veškeré vstupy (Joystick, Pot, Tlačítka, Switche, Enkodér, Dorazy)
        sensorManager.updateAllInputs();

        BottomInputs in = globalState.getInputs();
        AppMode currentMode = globalState.getMode();

        unsigned long now = millis();


        switch(currentMode) {
            case MODE_SLEEP: {
                vTaskDelay(pdMS_TO_TICKS(100));
                Serial.println("[SPODNI] Prijat MODE_SLEEP z horniho panelu. Jdu spat...");

                // A) Zastavit motory a kontinuální servo
                sensorManager.setServoCont(0);

                // B) Zhasnout 8x WS2812B LED pásek
                uint32_t offLeds[8] = {0};
                sensorManager.setLedStrip(offLeds, 0);

                // C) Zhasnout OLED displej přes SensorManager (úplné vypnutí panelu)
                sensorManager.setOledPower(false);

                // D) Vyčkáme na uvolnění tlačítek spodního panelu
                while (digitalRead(PIN_BTN_1) == LOW || digitalRead(PIN_BTN_2) == LOW ||
                       digitalRead(PIN_BTN_3) == LOW || digitalRead(PIN_BTN_4) == LOW ||
                       digitalRead(PIN_BTN_5) == LOW || digitalRead(PIN_JOY_SW) == LOW) {
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
                vTaskDelay(pdMS_TO_TICKS(10)); // Debounce

                // Diagnostický výpis reálných úrovní na pinech před spánkem:
                Serial.printf("[DEBUG PINY] B1:%d B2:%d B3:%d B4:%d B5:%d | JOY_SW:%d | ENC_CLK:%d\n",
                    digitalRead(PIN_BTN_1), digitalRead(PIN_BTN_2), digitalRead(PIN_BTN_3),
                    digitalRead(PIN_BTN_4), digitalRead(PIN_BTN_5), digitalRead(PIN_JOY_SW),
                    digitalRead(PIN_ENC_CLK));

                // 1. Tlačítka 1 až 5 (D-Pad) - povolíme pouze pokud jsou v klidu HIGH (1)
                if (digitalRead(PIN_BTN_1) == HIGH) gpio_wakeup_enable((gpio_num_t)PIN_BTN_1, GPIO_INTR_LOW_LEVEL);
                if (digitalRead(PIN_BTN_2) == HIGH) gpio_wakeup_enable((gpio_num_t)PIN_BTN_2, GPIO_INTR_LOW_LEVEL);
                if (digitalRead(PIN_BTN_3) == HIGH) gpio_wakeup_enable((gpio_num_t)PIN_BTN_3, GPIO_INTR_LOW_LEVEL);
                if (digitalRead(PIN_BTN_4) == HIGH) gpio_wakeup_enable((gpio_num_t)PIN_BTN_4, GPIO_INTR_LOW_LEVEL);
                if (digitalRead(PIN_BTN_5) == HIGH) gpio_wakeup_enable((gpio_num_t)PIN_BTN_5, GPIO_INTR_LOW_LEVEL);

                // 2. Tlačítko joysticku - povolíme pouze pokud je v klidu HIGH
                if (digitalRead(PIN_JOY_SW) == HIGH) {
                    gpio_wakeup_enable((gpio_num_t)PIN_JOY_SW, GPIO_INTR_LOW_LEVEL);
                } else {
                    Serial.println("[VAROVANI] PIN_JOY_SW (GPIO 2) je v LOW -> vynechavam z wakeup!");
                }

                if (digitalRead(PIN_SWITCH_1) == HIGH) {
                    gpio_wakeup_enable((gpio_num_t)PIN_SWITCH_1, GPIO_INTR_LOW_LEVEL);
                }
                else{
                    gpio_wakeup_enable((gpio_num_t)PIN_SWITCH_1, GPIO_INTR_HIGH_LEVEL);
                }

                if (digitalRead(PIN_SWITCH_2) == HIGH) {
                    gpio_wakeup_enable((gpio_num_t)PIN_SWITCH_2, GPIO_INTR_LOW_LEVEL);
                }
                else{
                    gpio_wakeup_enable((gpio_num_t)PIN_SWITCH_2, GPIO_INTR_HIGH_LEVEL);
                }


                // 3. Rotační enkodér - povolíme pouze pokud je v klidu HIGH
                if (digitalRead(PIN_ENC_CLK) == HIGH) {
                    gpio_wakeup_enable((gpio_num_t)PIN_ENC_CLK, GPIO_INTR_LOW_LEVEL);
                } else {
                    Serial.println("[VAROVANI] PIN_ENC_CLK (GPIO 32) je v LOW -> vynechavam z wakeup!");
                }

                // 4. UART linka RX (GPIO 16) - vyčištění a probuzení při příchodu dat z horního panelu
                while (SerialTop.available()) {
                    SerialTop.read();
                }
                // U ESP32 (WROOM-32) funguje uart_wakeup pouze na GPIO 9 (IO_MUX), nikoliv na GPIO 16!
                // Proto použijeme spolehlivý GPIO wakeup na úrovni LOW (UART v klidu drží HIGH, data ho stáhnou na LOW).
                if (digitalRead(PIN_UART_TOP_RX) == HIGH) {
                    gpio_wakeup_enable((gpio_num_t)PIN_UART_TOP_RX, GPIO_INTR_LOW_LEVEL);
                }

                // 5. Globální povolení GPIO probuzení
                esp_sleep_enable_gpio_wakeup();

                Serial.println("[SPODNI] Vstupuji do Light Sleep...");
                Serial.flush();
                SerialTop.flush();

                esp_err_t err = esp_light_sleep_start();

                // ========================================================
                // TADY KÓD POKRAČUJE OKAMŽITĚ PO PROBUZENÍ!
                // ========================================================
                esp_sleep_wakeup_cause_t duvod = esp_sleep_get_wakeup_cause();

                // Okamžitě odečteme stav tlačítek a UARTu před další prodlevou:
                bool btnDownPressed = (digitalRead(PIN_BTN_1) == LOW || digitalRead(PIN_BTN_2) == LOW ||
                                       digitalRead(PIN_BTN_3) == LOW || digitalRead(PIN_BTN_4) == LOW ||
                                       digitalRead(PIN_BTN_5) == LOW || digitalRead(PIN_JOY_SW) == LOW);
                bool uartActive = (digitalRead(PIN_UART_TOP_RX) == LOW) || (SerialTop.available() > 0);

                esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
                gpio_wakeup_disable((gpio_num_t)PIN_UART_TOP_RX);

                #ifdef ENABLE_UART_TOP
                // Okamžitě probudíme horní panel pulsem (pokud by ještě spal)
                SerialTop.write((uint8_t)0x00);
                SerialTop.flush();
                #endif

                // Znovu zapneme OLED displej přes SensorManager
                sensorManager.setOledPower(true);

                Serial.printf("\n[SYSTEM] Probudil jsem se z Light Sleep! err=0x%X (%s), duvod=%d -> ",
                              err, esp_err_to_name(err), (int)duvod);
                if (btnDownPressed) {
                    Serial.println("PROBUZENO DOLNIMI TLACITKY!");
                } else if (uartActive || !btnDownPressed) {
                    Serial.printf("PROBUZENO Z HORNIHO PANELU nebo Switchema (UART)! (dostupno bajtu: %d)\n", SerialTop.available());
                } else {
                    Serial.printf("PROBUZENO JINYM ZDROJEM: %d\n", (int)duvod);
                }

                // Nastavíme režim zpět na MODE_MAIN_MENU:
                // 1. Zabrání tomu, aby spodní panel v dalším cyklu znovu spadl do spánku.
                // 2. Odblokuje Task_UART a odešle paket nahoru do horního panelu (probudí ho!).
                globalState.setMode(MODE_MAIN_MENU);
                break;
            }
            case MODE_MAIN_MENU:
            case MODE_GYRO:
            case MODE_BAREVNY:
            case MODE_GAME_SNAKE:
            case MODE_GAME_FLAPPY:
            case MODE_2048:
            case MODE_VZDALENOST:
            case MODE_WIFI_SPOJENI:
            case MODE_WIFI_DETECTION:
            case MODE_SERVA:
            case MODE_MOTOR:

            // -----------------------------------------------------------------
            // Řízení klasického (hloupého) serva potenciometrem (0 až 180°):
            // -----------------------------------------------------------------
            int classicServoAngle = map(constrain((int)in.potentiometer, 0, 4095), 0, 4095, 172, 0);
            if(classicServoAngle > (last_stupit_angle + 2) || classicServoAngle < (last_stupit_angle - 2)){
                last_stupit_angle = classicServoAngle;
                sensorManager.setServoClassic(classicServoAngle);
            }

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

            break;
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