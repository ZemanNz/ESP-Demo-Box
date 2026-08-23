#include <Arduino.h>
#include "SystemState.h"
#include "HardwareSetup.h"
#include "GraphicsManager.h"
#include "SensorManager.h"
#include "UartProtocol.h"
#include "esp_sleep.h"
#include "hry/snake.h"
#include "hry/flappy_bird.h"
#include "hry/game_2048.h"
#include "wifi/WebManager.h"

// ---------------------------------------------------------
// Fyzická instance Globálního Stavu (paměť)
// ---------------------------------------------------------
SystemState globalState;
SnakeGame snake;
FlappyGame flappy;
Game2048 g2048;
WebManager webManager;

// ---------------------------------------------------------
// 1. Task: Wi-Fi a WebServer (Poběží na Core 0)
// ---------------------------------------------------------
void Task_WiFi_Web(void *pvParameters) {
    Serial.print("Task_WiFi_Web bezi na uvazku (Core): ");
    Serial.println(xPortGetCoreID());

    // Spuštění SoftAP, Captive Portalu, WebServeru a WebSocketu
    webManager.begin(&globalState);

    for (;;) {
        webManager.update();
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

// ---------------------------------------------------------
// 2A. Task: Demo Autopilot (Simulátor pro testování displeje)
// ---------------------------------------------------------
void Task_UART_Simulator(void *pvParameters) {
    Serial.print("Demo Autopilot bezi na uvazku (Core): ");
    Serial.println(xPortGetCoreID());

    int timeInMode = 0;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100)); 
        timeInMode += 100;
        
        AppMode current = globalState.getMode();
        
        // --- AUTOPILOT PRO HRY ---
        if (current == MODE_GAME_FLAPPY) {
            if (timeInMode % 500 == 0) flappy.jump();
        } 
        else if (current == MODE_GAME_SNAKE) {
            if (timeInMode % 800 == 0) {
                int r = random(0, 4);
                if (r == 0) snake.goUp();
                else if (r == 1) snake.goDown();
                else if (r == 2) snake.goLeft();
                else snake.goRight();
            }
        }
        else if (current == MODE_2048) {
            if (timeInMode % 1000 == 0) {
                int r = random(0, 4);
                if (r == 0) g2048.moveUp();
                else if (r == 1) g2048.moveDown();
                else if (r == 2) g2048.moveLeft();
                else g2048.moveRight();
            }
        }

        // --- PŘEPÍNÁNÍ MÓDŮ ---
        if (current == MODE_SLEEP) {
            timeInMode = 0; // Ve spánku neposouváme čas, čekáme na probuzení
        }
        else if (timeInMode >= 6000) {
            timeInMode = 0;
            int nextMode = (int)current + 1;
            if (nextMode > MODE_SLEEP) {
                nextMode = MODE_MAIN_MENU;
            }
            Serial.printf("[DEMO] Prepinam na mod: %d\n", nextMode);
            globalState.setMode((AppMode)nextMode);
        }
    }
}


void Task_UART(void *pvParameters) {
    unsigned long lastSendTime = 0;
    
    // Vyrovnávací paměti pro pakety
    TopToBottomPacket outPacket;
    BottomToTopPacket inPacket;

    for (;;) {
        unsigned long now = millis();

        // -------------------------------------------------------------
        // 1. ODESÍLÁNÍ (HORNÍ -> DOLNÍ PANEL)
        // -------------------------------------------------------------
        bool hasChanged = globalState.popBottomNeedsTx();
        bool heartbeatTimeout = (now - lastSendTime >= 200);

        // Pošli pokud: Nastala změna NEBO vypršelo 200 ms od posledního odeslání
        if (hasChanged || heartbeatTimeout) {
            lastSendTime = now;

            // A) Zkopírujeme si aktuální stav ze SystemState
            SensorData data = globalState.getSensorData();

            // B) Naplníme odchozí paket
            outPacket.startByte = UART_FRAME_START_TOP_TO_BOTTOM; // 0xAA
            outPacket.currentMode = (uint8_t)globalState.getMode();
            outPacket.overrideAutonomy = false; // Nebo true podle potřeby
            
            outPacket.targetSmartServoAngle = data.smartServoAngle;
            outPacket.targetServoAngle = data.servoAngle;
            outPacket.targetContinuousServo = data.continuousServoSpeed;
            outPacket.targetMotorSpeed = data.motorSpeed;

            for (int i = 0; i < 8; i++) {
                outPacket.ledStrip[i] = data.ledStripBottom[i];
            }
            outPacket.ledBrightness = data.ledStripBottomBrightness;

            strncpy(outPacket.oledLine1, data.bottomOledLine1, 16);
            outPacket.oledLine1[16] = '\0';
            strncpy(outPacket.oledLine2, data.bottomOledLine2, 16);
            outPacket.oledLine2[16] = '\0';

            outPacket.endByte = UART_FRAME_END; // 0xFE

            // C) Spočítáme kontrolní součet ze všech datových bajtů před checksumem
            outPacket.checksum = calculateChecksum(
                (const uint8_t*)&outPacket, 
                sizeof(TopToBottomPacket) - 2 // Odečteme checksum a endByte
            );

            // D) Odešleme celý binární blok najednou
            SerialESP.write((const uint8_t*)&outPacket, sizeof(TopToBottomPacket));
        }

        // -------------------------------------------------------------
        // 2. PŘÍJEM (DOLNÍ -> HORNÍ PANEL)
        // -------------------------------------------------------------
        
        while (SerialESP.available() >= sizeof(BottomToTopPacket)) {
            
            // 2. KONTROLA ZAČÁTKU (Synchronizace):
            // Funkce peek() se jen "podívá" na první bajt v bufferu, ale nesmaže ho
            if (SerialESP.peek() != UART_FRAME_START_BOTTOM_TO_TOP) { // Není to 0x55?
                SerialESP.read(); // Zahodíme 1 vadný bajt a zkusíme to v dalším kole znova
                continue;
            }
            // 3. PŘEČTENÍ CELÉHO PAKETU NARÁZ
            // readBytes bleskově nasype všechny bajty přímo do naší struktury v paměti!
            SerialESP.readBytes((uint8_t*)&inPacket, sizeof(BottomToTopPacket));
            // 4. KONTROLA KONCE A KONTROLNÍHO SOUČTU
            uint8_t calculatedCRC = calculateChecksum(
                (const uint8_t*)&inPacket, 
                sizeof(BottomToTopPacket) - 2
            );
            // Ověříme, že sedí koncový bajt (0xFE) i náš XOR checksum
            if (inPacket.endByte == UART_FRAME_END && inPacket.checksum == calculatedCRC) {
                
                // 5. DATA JSOU 100% V POŘÁDKU -> ZAPÍŠEME JE DO SYSTEMSTATE
                globalState.updateJoystick(inPacket.joyX, inPacket.joyY, inPacket.joyBtn);
                globalState.updateDownButtons(
                    inPacket.btnDown[0], inPacket.btnDown[1], 
                    inPacket.btnDown[2], inPacket.btnDown[3], inPacket.btnDown[4]
                );
                globalState.updateEncoder(inPacket.encoderPos, inPacket.encoderDelta, inPacket.encoderBtn);
                globalState.updatePotentiometer(inPacket.potentiometer);
                globalState.updateSwitches(inPacket.switch1, inPacket.switch2);
                
                // Zpětná vazba z akčních členů
                globalState.updateSmartServo(inPacket.currentSmartServoAngle);
                globalState.updateServo(inPacket.currentServoAngle);
                globalState.updateMotor(inPacket.currentMotorSpeed);
                globalState.updateContinuousServo(inPacket.currentContinuousServo);
                
            } else {
                // Pokud nesedí checksum (rušení na drátě), paket jednoduše ignorujeme
                Serial.println("[UART] Chyba kontrolniho souctu! Paket zahozen.");
            }
        }

        // -------------------------------------------------------------
        // Krátká neblokující prodleva (smyčka běží každých 10 ms)
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ---------------------------------------------------------
// 3. Task: Senzory a vstupy (Poběží na Core 1)
// ---------------------------------------------------------
void Task_Sensors(void *pvParameters) {
    Serial.print("Task_Sensors bezi na uvazku (Core): ");
    Serial.println(xPortGetCoreID());

    for (;;) {
        AppMode currentMode = globalState.getMode();

        // Kontrola otočení rotačního enkodéru pro změnu módu
        sensorManager.readAllSensors();
        globalState.checkModeChange();

        switch (currentMode) {
            case MODE_SLEEP: {
                // Ve spánku nečteme senzory, čekáme na probuzení
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
            }
            
            case MODE_MAIN_MENU: {
            

                break;
            }

            case MODE_SENSORS: {
                sensorManager.readAllSensors();
                break;
            }

            case MODE_GAME_SNAKE: {
                sensorManager.readAllSensors();
                break;
            }

            case MODE_GAME_FLAPPY: {
                sensorManager.readAllSensors();
                break;
            }

            case MODE_2048: {
                sensorManager.readAllSensors();
                break;
            }

            case MODE_VZDALENOST: {
                sensorManager.readAllSensors();
                break;
            }

            case MODE_WIFI_SPOJENI: {
                sensorManager.readAllSensors();
                break;
            }

            case MODE_SERVA: {
                sensorManager.readAllSensors();
                break;
            }

            case MODE_MOTOR: {
                sensorManager.readAllSensors();
                break;
            }

            case MODE_BAREVNY: {
                sensorManager.readAllSensors();
                break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Perioda čtení senzorů 10 ms
    }
}

// Pomocná funkce pro vykreslení kruhového / Pac-man gauge ukazatele (0.0f až 1.0f)
void drawRadialGauge(int cx, int cy, int rIn, int rOut, float percent, uint16_t activeColor, uint16_t bgColor) {
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 1.0f) percent = 1.0f;
    float maxAngle = percent * 360.0f;
    for (int deg = 0; deg < 360; deg += 6) {
        float rad = (deg - 90) * 0.0174532925f; // Převod na radiány (-90 = nahoře)
        float cosA = cos(rad);
        float sinA = sin(rad);
        uint16_t c = (deg <= maxAngle) ? activeColor : bgColor;
        
        int x1 = cx + (int)(rIn * cosA);
        int y1 = cy + (int)(rIn * sinA);
        int x2 = cx + (int)(rOut * cosA);
        int y2 = cy + (int)(rOut * sinA);
        gfx.drawLine(x1, y1, x2, y2, c);
    }
}

// Pomocná funkce pro vykreslení plného Pac-man kruhového výseku (0.0f až 1.0f)
void drawPacmanGauge(int cx, int cy, int radius, float percent, uint16_t activeColor, uint16_t bgColor) {
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 1.0f) percent = 1.0f;

    // 1. Podkladový kruh
    gfx.fillCircle(cx, cy, radius, bgColor);

    // 2. Zaplnění koláčového výseku (Pac-man tělo)
    int maxAngle = (int)(percent * 360.0f);
    if (maxAngle > 0) {
        for (int deg = 0; deg <= maxAngle; deg += 3) {
            float rad = (deg - 90) * 0.0174532925f; // Začátek nahoře na 12 hodinách (-90 deg)
            int x = cx + (int)(radius * cos(rad));
            int y = cy + (int)(radius * sin(rad));
            gfx.drawLine(cx, cy, x, y, activeColor);
        }
    }

    // 3. Černý obrys
    gfx.drawCircle(cx, cy, radius, ST77XX_BLACK);
}

// ---------------------------------------------------------
// 4. Task: Displej a State Machine (Poběží na Core 1)
// ---------------------------------------------------------
void Task_Display_UI(void *pvParameters) {
    Serial.print("Task_Display_UI bezi na uvazku (Core): ");
    Serial.println(xPortGetCoreID());
    unsigned long casPoslednihoKroku = 0;
    byte delay = 50; // Defaultní zpoždění mezi překresleními (20 fps)

    // Inicializace gfx je hotová v HardwareSetup. Nyní můžeme kreslit.
    for (;;) {
        // 1. Zjistíme, v jakém stavu se kufr zrovna nachází
        AppMode currentMode = globalState.getMode();
        
        // 2. Potřebujeme kompletně překreslit obrazovku? 
        // (Vlajka je true jen těsně po přepnutí stavu nebo stisku tlačítka)
        bool needsFullRedraw = globalState.popUiNeedsUpdate();


        //vykreslujeme info?
        bool infoActive = globalState.isInfoActive();

        // 3. Vykreslujeme podle módu
        switch (currentMode) {
            
            // =======================================================
            case MODE_SLEEP: {
                Serial.println("[SYSTEM] Prechazim do rezimu Light Sleep...");


                // 1. Zhasneme displej a vypneme jeho podsvícení
                gfx.clearScreen(ST77XX_BLACK);
                #ifdef PIN_TFT_LED
                    digitalWrite(PIN_TFT_LED, LOW);
                #endif

                // 2. Vypneme 3x LED diody na horním panelu
                #ifdef ENABLE_LEDS
                    sensorManager.setLeds(false, false, false);
                #endif

                // 3. Zhasneme 8-LED WS2812B pásek
                #ifdef ENABLE_WS2812B
                    uint32_t offLeds[8] = {0};
                    sensorManager.setLedStrip(offLeds, 0);
                #endif

                // 4. Vypneme bzučák
                #ifdef ENABLE_BUZZER
                    sensorManager.setBuzzer(false, 0);
                #endif

                // 5. Zhasneme 7-segmentový displej
                #ifdef ENABLE_74HC595
                    sensorManager.set7Segment(-1);
                #endif

                // 6. Zhasneme I2C LCD 1602 displej a jeho podsvícení
                #ifdef ENABLE_LCD1602
                    sensorManager.setLCDBacklight(false);
                #endif

                // 7. Vyprázdníme sériové buffery před uspáním
                Serial.flush();
                #ifdef ENABLE_UART_ESP
                    SerialESP.flush();
                #endif

                // 8. Nastavení všech 3 hardwarových zdrojů probuzení:
                // A) Horní tlačítko 1 (stisk spojí na GND = LOW)
                #ifdef PIN_BTN1
                    gpio_wakeup_enable((gpio_num_t)PIN_BTN1, GPIO_INTR_LOW_LEVEL);
                #endif

                // B) Akcelerometr LSM6DS3 (INT1 na GPIO 45 – impuls HIGH při otřesu/klepnutí)
                #ifdef PIN_IMU_INT
                    gpio_wakeup_enable((gpio_num_t)PIN_IMU_INT, GPIO_INTR_HIGH_LEVEL);
                #endif

                // Povolení GPIO probuzení (tlačítko + akcelerometr)
                esp_sleep_enable_gpio_wakeup();

                // C) UART z dolního panelu (UART1 – jakákoliv aktivita zdola)
                #ifdef ENABLE_UART_ESP
                    esp_sleep_enable_uart_wakeup(1);
                #endif

                // 9. VSTUP DO SKUTEČNÉHO LEHKÉHO SPÁNKU
                // (Procesor zastaví hodiny, minimální odběr proudu, RAM zůstává plně zachována)
                esp_light_sleep_start();

                // ===================================================
                // 10. PROBUZENÍ! (Kód pokračuje okamžitě zde)
                // ===================================================
                #ifdef PIN_TFT_LED
                    digitalWrite(PIN_TFT_LED, HIGH); // Znovu rozsvítíme podsvícení displeje
                #endif

                #ifdef ENABLE_LCD1602
                    sensorManager.setLCDBacklight(true); // Znovu rozsvítíme LCD 1602
                #endif

                esp_sleep_wakeup_cause_t duvod = esp_sleep_get_wakeup_cause();
                Serial.printf("[SYSTEM] Probudil jsem se z Light Sleep! (Duvod kod: %d) -> ", (int)duvod);
                if (duvod == ESP_SLEEP_WAKEUP_GPIO) {
                    Serial.println("PROBUZENO TLACITKEM NEBO OTRESEM (GPIO)");
                } else if (duvod == ESP_SLEEP_WAKEUP_UART) {
                    Serial.println("PROBUZENO Z DOLNIHO PANELU (UART)");
                } else {
                    Serial.printf("PROBUZENO JINYM ZDROJEM: %d\n", (int)duvod);
                }

                // Krátká prodleva na stabilizaci po probuzení
                vTaskDelay(pdMS_TO_TICKS(100));

                // Vrátíme se do předchozího módu (pokud byl předchozí mód na konci demo cyklu, začneme od Menu)
                AppMode returnMode = globalState.getLastMode();
                if (returnMode == MODE_SLEEP || returnMode == MODE_BAREVNY) {
                    returnMode = MODE_MAIN_MENU;
                }
                globalState.setMode(returnMode);
                break;
            }

            // =======================================================
            case MODE_MAIN_MENU: {
                delay = 50; // Defaultní zpoždění mezi překresleními (20 fps)
                bool isInfo = globalState.isInfoActive();

                if (isInfo) {
                    if (needsFullRedraw) {
                        // INFO OVERLAY pro Hlavní menu
                        gfx.clearScreen(ST77XX_BLACK);
                        gfx.drawRoundRect(10, 10, 300, 220, 8, ST77XX_YELLOW);
                        gfx.drawTextPartial(30, 25, "INFO: HLAVNI MENU", ST77XX_YELLOW, ST77XX_BLACK, 2);
                        
                        gfx.drawTextPartial(25, 60, "- Enkoder: Rotace meni mody", ST77XX_WHITE, ST77XX_BLACK, 1);
                        gfx.drawTextPartial(25, 80, "- Joy stisk: Rychly vyber modu", ST77XX_WHITE, ST77XX_BLACK, 1);
                        gfx.drawTextPartial(25, 100, "- Potenciometr: Uhel serva", ST77XX_WHITE, ST77XX_BLACK, 1);
                        gfx.drawTextPartial(25, 120, "- Joystick Y: Kontinualni servo", ST77XX_WHITE, ST77XX_BLACK, 1);
                        gfx.drawTextPartial(25, 140, "- Dolni tlacitka: Volba pro 7-seg", ST77XX_WHITE, ST77XX_BLACK, 1);
                        gfx.drawTextPartial(25, 160, "- Horni tlacitko (1s): Rezim spanku", ST77XX_WHITE, ST77XX_BLACK, 1);
                        
                        gfx.drawTextPartial(40, 195, "[Stiskni horni tlacitko pro zpet]", ST77XX_GREEN, ST77XX_BLACK, 1);
                    }
                }
                else {
                    SensorData data = globalState.getSensorData();

                    // -------------------------------------------------------------
                    // 1. STATICKÁ MASKA (Vykreslí se pouze jednou při vstupu)
                    // -------------------------------------------------------------
                    if (needsFullRedraw) {
                        gfx.clearScreen(ST77XX_WHITE); // Základní bílé pozadí

                        // A) HORNÍ ČÁST (Y = 0 až 140)
                        // fot_l (Levý fotorezistor)
                        gfx.drawRect(0, 0, 35, 140, ST77XX_BLACK);

                        // Široký středový kolotoč módů (Mode Carousel)
                        gfx.fillRect(36, 0, 178, 140, 0xDEFB); // Světle šedé pozadí
                        gfx.drawRect(36, 0, 178, 140, ST77XX_BLACK);
                        
                        // 2 nad: Nejmenší, nejsvětlejší
                        gfx.drawTextPartial(60, 14, "MODE_WIFI_DETECTION", 0x7BEF, 0xDEFB, 1);
                        // 1 nad: Střední
                        gfx.drawTextPartial(75, 36, "MODE_BAREVNY", 0x4208, 0xDEFB, 1);
                        
                        // AKTIVNÍ MÓD UPŘOSTŘED (Největší text velikosti 2 s bílou kartou)
                        gfx.fillRoundRect(40, 54, 170, 34, 4, ST77XX_WHITE);
                        gfx.drawRoundRect(40, 54, 170, 34, 4, ST77XX_BLACK);
                        gfx.drawTextPartial(44, 63, "MODE_MAIN_MENU", ST77XX_BLACK, ST77XX_WHITE, 2);
                        
                        // 1 pod: Střední
                        gfx.drawTextPartial(75, 96, "MODE_SENSORS", 0x4208, 0xDEFB, 1);
                        // 2 pod: Nejmenší, nejsvětlejší
                        gfx.drawTextPartial(70, 118, "MODE_GAME_SNAKE", 0x7BEF, 0xDEFB, 1);

                        // Wi-Fi sekce (Plná výška Y = 0 až 140)
                        gfx.fillRect(214, 0, 70, 140, ST77XX_WHITE);
                        gfx.drawRect(214, 0, 70, 140, ST77XX_BLACK);
                        
                        // Zelená Wi-Fi ikona
                        gfx.fillCircle(249, 44, 3, ST77XX_GREEN);
                        gfx.drawCircle(249, 44, 10, ST77XX_GREEN);
                        gfx.drawCircle(249, 44, 11, ST77XX_GREEN);
                        gfx.drawCircle(249, 44, 18, ST77XX_GREEN);
                        gfx.drawCircle(249, 44, 19, ST77XX_GREEN);
                        gfx.fillRect(225, 45, 48, 18, ST77XX_WHITE); // Odříznutí spodku vln
                        gfx.drawTextPartial(220, 96, "zarizeni :", ST77XX_BLACK, ST77XX_WHITE, 1);

                        // fot_r (Pravý fotorezistor)
                        gfx.drawRect(284, 0, 36, 140, ST77XX_BLACK);

                        // B) SPODNÍ ČÁST (Y = 141 až 239)
                        // Horizontální dělící linka
                        gfx.drawLine(0, 140, 319, 140, ST77XX_BLACK);

                        // Potenciometr box
                        gfx.fillRect(48, 141, 58, 98, ST77XX_WHITE);
                        gfx.drawRect(48, 141, 58, 98, ST77XX_BLACK);
                        gfx.drawTextPartial(66, 145, "pot", ST77XX_BLACK, ST77XX_WHITE, 1);

                        // Tlačítka 5x (D-Pad box)
                        gfx.fillRect(106, 141, 70, 98, ST77XX_WHITE);
                        gfx.drawRect(106, 141, 70, 98, ST77XX_BLACK);

                        // joy-x box
                        gfx.fillRect(176, 141, 48, 98, ST77XX_WHITE);
                        gfx.drawRect(176, 141, 48, 98, ST77XX_BLACK);
                        gfx.drawTextPartial(182, 145, "joy-x", ST77XX_BLACK, ST77XX_WHITE, 1);

                        // joy-y box
                        gfx.fillRect(224, 141, 48, 98, ST77XX_WHITE);
                        gfx.drawRect(224, 141, 48, 98, ST77XX_BLACK);
                        gfx.drawTextPartial(230, 145, "joy-y", ST77XX_BLACK, ST77XX_WHITE, 1);

                        // Barevný senzor box
                        gfx.fillRect(272, 141, 48, 98, 0x9E3F);
                        gfx.drawRect(272, 141, 48, 98, ST77XX_BLACK);
                        gfx.drawTextPartial(274, 145, "barevny", ST77XX_BLACK, 0x9E3F, 1);
                    }

                    // -------------------------------------------------------------
                    // 2. ŽIVÁ ČÍSLA A GRAFICKÉ PAC-MAN UKAZATELE
                    // -------------------------------------------------------------

                    // A) Wi-Fi počet připojených zařízení
                    #ifdef ENABLE_WIFI_WEB
                        gfx.drawTextPartial(242, 114, String(WiFi.softAPgetStationNum()) + " ", ST77XX_GREEN, ST77XX_WHITE, 2);
                    #else
                        gfx.drawTextPartial(242, 114, "0 ", ST77XX_BLACK, ST77XX_WHITE, 2);
                    #endif

                    // B) Switche s1 a s2
                    uint16_t s1Color = data.switch1 ? 0x07E0 : 0xF9C7;
                    gfx.fillRect(1, 142, 22, 96, s1Color);
                    gfx.drawRect(0, 141, 24, 98, ST77XX_BLACK);
                    gfx.drawTextPartial(6, 185, "s1", ST77XX_BLACK, s1Color, 1);

                    uint16_t s2Color = data.switch2 ? 0x07E0 : 0xF9C7;
                    gfx.fillRect(25, 142, 22, 96, s2Color);
                    gfx.drawRect(24, 141, 24, 98, ST77XX_BLACK);
                    gfx.drawTextPartial(30, 185, "s2", ST77XX_BLACK, s2Color, 1);

                    // C) Potenciometr plný Pac-man kruh (Žlutý)
                    float potPercent = constrain((float)data.potentiometer / 4095.0f, 0.0f, 1.0f);
                    drawPacmanGauge(77, 178, 17, potPercent, ST77XX_YELLOW, 0xDEFB);
                    gfx.drawTextPartial(56, 214, String(data.potentiometer) + "   ", 0x4208, ST77XX_WHITE, 1);

                    // D) 5x Tlačítka (D-Pad visualizer)
                    // Horní tlačítko (btnDown[1] - zelené)
                    gfx.fillCircle(141, 158, 7, data.btnDown[1] ? ST77XX_GREEN : ST77XX_WHITE);
                    gfx.drawCircle(141, 158, 7, ST77XX_BLACK);

                    // Prostřední tlačítko (btnDown[0] - červené)
                    gfx.fillCircle(141, 186, 8, data.btnDown[0] ? ST77XX_RED : ST77XX_WHITE);
                    gfx.drawCircle(141, 186, 8, ST77XX_BLACK);

                    // Levé tlačítko (btnDown[2] - modré)
                    gfx.fillCircle(117, 186, 7, data.btnDown[2] ? ST77XX_BLUE : ST77XX_WHITE);
                    gfx.drawCircle(117, 186, 7, ST77XX_BLACK);

                    // Pravé tlačítko (btnDown[3] - žluté)
                    gfx.fillCircle(165, 186, 7, data.btnDown[3] ? ST77XX_YELLOW : ST77XX_WHITE);
                    gfx.drawCircle(165, 186, 7, ST77XX_BLACK);

                    // Dolní tlačítko (btnDown[4] - azurové)
                    gfx.fillCircle(141, 214, 7, data.btnDown[4] ? ST77XX_CYAN : ST77XX_WHITE);
                    gfx.drawCircle(141, 214, 7, ST77XX_BLACK);

                    // E) Joystick X plný Pac-man kruh (Azurový / Modrý)
                    float joyXPercent = constrain((float)data.joyX / 4095.0f, 0.0f, 1.0f);
                    drawPacmanGauge(200, 178, 15, joyXPercent, ST77XX_CYAN, 0xDEFB);
                    gfx.drawTextPartial(180, 214, String(data.joyX) + "   ", 0x4208, ST77XX_WHITE, 1);

                    // F) Joystick Y plný Pac-man kruh (Oranžový)
                    float joyYPercent = constrain((float)data.joyY / 4095.0f, 0.0f, 1.0f);
                    drawPacmanGauge(248, 178, 15, joyYPercent, 0xFD20, 0xDEFB); // 0xFD20 = Oranžová
                    gfx.drawTextPartial(228, 214, String(data.joyY) + "   ", 0x4208, ST77XX_WHITE, 1);

                    // G) Barevný senzor náhled
                    uint16_t liveColor = 0x9E3F;
                    if (data.colorR > 0 || data.colorG > 0 || data.colorB > 0) {
                        uint8_t r8 = data.colorR > 255 ? (data.colorR >> 8) : data.colorR;
                        uint8_t g8 = data.colorG > 255 ? (data.colorG >> 8) : data.colorG;
                        uint8_t b8 = data.colorB > 255 ? (data.colorB >> 8) : data.colorB;
                        liveColor = gfx.color565(r8, g8, b8);
                    }
                    gfx.fillRect(273, 142, 46, 96, liveColor);
                    gfx.drawRect(272, 141, 48, 98, ST77XX_BLACK);
                    gfx.drawTextPartial(274, 145, "barevny", ST77XX_BLACK, liveColor, 1);

                    // H) Fotorezistory fot_l a fot_r (Dynamický odstín šedá -> bílá)
                    // Levý fotorezistor fot_l
                    uint8_t grayL = map(constrain((int)data.photo1, 0, 4095), 0, 4095, 40, 255);
                    uint16_t colL = gfx.color565(grayL, grayL, grayL);
                    uint16_t textColL = (grayL > 130) ? ST77XX_BLACK : ST77XX_WHITE;
                    gfx.fillRect(1, 1, 33, 138, colL);
                    gfx.drawRect(0, 0, 35, 140, ST77XX_BLACK);
                    gfx.drawTextPartial(5, 65, "fot_l", textColL, colL, 1);

                    // Pravý fotorezistor fot_r
                    uint8_t grayR = map(constrain((int)data.photo2, 0, 4095), 0, 4095, 40, 255);
                    uint16_t colR = gfx.color565(grayR, grayR, grayR);
                    uint16_t textColR = (grayR > 130) ? ST77XX_BLACK : ST77XX_WHITE;
                    gfx.fillRect(285, 1, 33, 138, colR);
                    gfx.drawRect(284, 0, 36, 140, ST77XX_BLACK);
                    gfx.drawTextPartial(288, 65, "fot_r", textColR, colR, 1);
                }
                break;
            }

            // =======================================================
            case MODE_SENSORS: {
                delay = 50; // Defaultní zpoždění mezi překresleními (20 fps)
                // A) KOMPLETNÍ PŘEKRESLENÍ (Kreslíme statickou kostru)
                if (needsFullRedraw) {
                    gfx.clearScreen(ST77XX_BLACK);
                    gfx.drawTextPartial(10, 10, "Teplota:", ST77XX_YELLOW, ST77XX_BLACK, 2);
                    gfx.drawTextPartial(10, 40, "Vlhkost:", ST77XX_YELLOW, ST77XX_BLACK, 2);
                    gfx.drawLine(0, 70, 320, 70, ST77XX_WHITE);
                }
                
                // B) ČÁSTEČNÉ PŘEKRESLENÍ (Mimo podmínku needsFullRedraw = běží pořád!)
                // Zkopírujeme si aktuální data ze senzorů
                SensorData data = globalState.getSensorData();
                
                // Přepíšeme jen čísla (barva pozadí automaticky smaže ta stará)
                gfx.drawTextPartial(120, 10, String(data.temperature, 1) + " C ", ST77XX_WHITE, ST77XX_BLACK, 2);
                gfx.drawTextPartial(120, 40, String(data.humidity, 1) + " % ", ST77XX_WHITE, ST77XX_BLACK, 2);
                break;
            }

            // =======================================================
            case MODE_2048: {
                delay = 50;
                if (needsFullRedraw) {
                    g2048.reset();
                    g2048.pohyb = true; // Vynutíme první vykreslení po startu hry
                }
                
                // Překreslí se POUZE když si hra vyžádá překreslení (tzn. změnil se stav)
                if (g2048.pohyb) {
                    g2048.draw();
                    g2048.pohyb = false; // Sníme flag, abychom nekreslili pořád
                }
                break;
            }

            // =======================================================
            case MODE_VZDALENOST: {
                delay = 50;
                if (needsFullRedraw) {
                    gfx.clearScreen(ST77XX_BLACK);
                    gfx.drawTextPartial(70, 100, "VZDALENOST", ST77XX_WHITE, ST77XX_BLACK, 3);
                }
                break;
            }

            // =======================================================
            case MODE_WIFI_SPOJENI: {
                delay = 50;
                if (needsFullRedraw) {
                    gfx.clearScreen(ST77XX_BLACK);
                    gfx.drawTextPartial(52, 100, "WIFI SPOJENI", ST77XX_WHITE, ST77XX_BLACK, 3);
                }
                break;
            }

            // =======================================================
            case MODE_SERVA: {
                delay = 50;
                if (needsFullRedraw) {
                    gfx.clearScreen(ST77XX_BLACK);
                    gfx.drawTextPartial(115, 100, "SERVA", ST77XX_WHITE, ST77XX_BLACK, 3);
                }
                break;
            }

            // =======================================================
            case MODE_MOTOR: {
                delay = 50;
                if (needsFullRedraw) {
                    gfx.clearScreen(ST77XX_BLACK);
                    gfx.drawTextPartial(115, 100, "MOTOR", ST77XX_WHITE, ST77XX_BLACK, 3);
                }
                break;
            }

            // =======================================================
            case MODE_BAREVNY: {
                delay = 50;
                if (needsFullRedraw) {
                    gfx.clearScreen(ST77XX_BLACK);
                    gfx.drawTextPartial(34, 100, "BAREVNY SENZOR", ST77XX_WHITE, ST77XX_BLACK, 3);
                }
                break;
            }
            
            // =======================================================
            case MODE_GAME_SNAKE: {
                if (needsFullRedraw) {
                    snake.reset();
                }
                unsigned long aktualniCas = millis();
                delay = 10;

                if(snake.isGameOver()){
                    // sem dat ze kdyz stisknuto nejaky tlacitko tak nova hra---if()
                    // snake.reset();
                }

                if(aktualniCas - casPoslednihoKroku >= 150){
                    casPoslednihoKroku = aktualniCas;
                    snake.update();
                    snake.draw();
                }
                break;
            }
            
            // =======================================================
            case MODE_GAME_FLAPPY: {
                if (needsFullRedraw) {
                    flappy.reset();
                }
                unsigned long aktualniCasFlappy = millis();
                delay = 10;
                if(flappy.getGameOver()){
                    // sem dat ze kdyz stisknuto nejaky tlacitko tak nova hra---if()
                    // flappy.reset();
                }

                if(aktualniCasFlappy - casPoslednihoKroku >= 40){
                    casPoslednihoKroku = aktualniCasFlappy;
                    flappy.update();
                    flappy.draw();
                }
                break;
            }
            // =======================================================
        }

        // Čekáme 50 ms (kreslíme max 20 fps)
        vTaskDelay(pdMS_TO_TICKS(delay));
    }
}


// ---------------------------------------------------------
// Hlavní SETUP (pouze pro vytvoření vláken)
// ---------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- ESP-Demo-Box: Boot systemu ---");

    // Zde se zavolá setup všech modulů
    if (!initializeAllHardware()) {
        Serial.println("[VAROVANI] Nektery modul selhal, presto pokracuji ve startu systemu...");
    }

    snake.reset();
    flappy.reset();

    // Vytváření úloh (Tasks) pro FreeRTOS.
    // Argumenty: Funkce, Název pro debug, Velikost paměti (Stack), Parametry, Priorita, Zvláštní Handle, ID Jádra

    xTaskCreatePinnedToCore(Task_WiFi_Web, "WiFi_Web", 8192, NULL, 1, NULL, 0); // Core 0
    //xTaskCreatePinnedToCore(Task_UART_Simulator, "UART_Mock", 4096, NULL, 1, NULL, 0); // Core 0
    
    xTaskCreatePinnedToCore(Task_Display_UI, "Display_UI", 8192, NULL, 1, NULL, 1); // Core 1
    xTaskCreatePinnedToCore(Task_Sensors, "Sensors", 4096, NULL, 1, NULL, 1); // Core 1

    Serial.println("Vsechna vlakna spustena!");
}

// ---------------------------------------------------------
// Smyčka loop()
// ---------------------------------------------------------
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
