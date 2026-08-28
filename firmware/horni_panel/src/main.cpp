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
        vTaskDelay(pdMS_TO_TICKS(50)); 
        timeInMode += 50;
        
        AppMode current = globalState.getMode();
        float t = millis() / 1000.0f;

        // ---------------------------------------------------------
        // 1. GENERÁTOR NÁHODNÝCH DAT (Změna hodnot cca 3x za 10 sekund)
        // ---------------------------------------------------------
        if (timeInMode % 3300 < 50) {
            // A) Gyroskop: náhodné stabilní úhly v reálném rozsahu (např. -34.2°, +13.9°, +24.8°)
            float simRoll  = (float)random(-450, 451) / 10.0f;
            float simPitch = (float)random(-300, 301) / 10.0f;
            float simYaw   = (float)random(-600, 601) / 10.0f;

            float radR = simRoll * (PI / 180.0f);
            float radP = simPitch * (PI / 180.0f);
            float ax = -sin(radP);
            float ay = sin(radR);
            float az = cos(radR);
            globalState.updateIMU(ax, ay, az, simPitch, simRoll, simYaw);

            // B) Barevný senzor: náhodné čisté barvy
            uint8_t r = random(20, 256);
            uint8_t g = random(20, 256);
            uint8_t b = random(20, 256);
            globalState.updateColor(r, g, b, 200);

            // C) Hlavní menu: náhodné polohy ovladačů
            int simPot = random(200, 4000);
            globalState.updatePotentiometer(simPot);

            int simJoyX = random(200, 4000);
            int simJoyY = random(200, 4000);
            globalState.updateJoystick(simJoyX, simJoyY, false);

            int simPhoto1 = random(300, 3900);
            int simPhoto2 = random(300, 3900);
            globalState.updatePhotoresistors(simPhoto1, simPhoto2);

            bool b0 = (random(0, 2) == 1);
            bool b1 = (random(0, 2) == 1);
            bool b2 = (random(0, 2) == 1);
            bool b3 = (random(0, 2) == 1);
            bool b4 = (random(0, 2) == 1);
            globalState.updateDownButtons(b0, b1, b2, b3, b4);
        }

        // ---------------------------------------------------------
        // 2. PŘEPÍNÁNÍ POUZE MEZI 3 MÓDY PO 10 SEKUNDÁCH (10 000 ms)
        // ---------------------------------------------------------
        if (timeInMode >= 10000) {
            timeInMode = 0;
            AppMode nextMode = MODE_MAIN_MENU;
            
            if (current == MODE_MAIN_MENU) {
                nextMode = MODE_SENSORS; // GYROSKOP
            } else if (current == MODE_SENSORS) {
                nextMode = MODE_BAREVNY; // BAREVNÝ SENZOR
            } else {
                nextMode = MODE_MAIN_MENU; // Zpět do HLAVNÍHO MENU
            }

            Serial.printf("[DEMO 3-MODY] Prepinam na mod: %d\n", (int)nextMode);
            globalState.setMode(nextMode);
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
        globalState.checkModeChange();

        /* --- MĚŘENÍ SENZORŮ ZAKOMENTOVÁNO PRO REŽIM SIMULÁTORU ---
        switch (currentMode) {
            case MODE_SLEEP: {
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
            }
            
            case MODE_MAIN_MENU: {
                sensorManager.readDHT();
                sensorManager.readUltrasonic();
                sensorManager.readVL53L0X();
                sensorManager.readIRDistance();
                sensorManager.readPhotoresistors();
                sensorManager.readTopButton();
                sensorManager.readColorSensor();
                break;
            }

            case MODE_SENSORS:
            case MODE_GAME_SNAKE:
            case MODE_GAME_FLAPPY:
            case MODE_2048:
            case MODE_VZDALENOST:
            case MODE_WIFI_SPOJENI:
            case MODE_SERVA:
            case MODE_MOTOR:
            case MODE_BAREVNY: {
                sensorManager.readAllSensors();
                break;
            }
        }
        ------------------------------------------------------------- */

        vTaskDelay(pdMS_TO_TICKS(50)); // Prodleva v simulátoru
    }
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
                isInfo = true;

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

                    // Statické proměnné pro detekci změn (překreslujeme JEN při reálné změně)
                    static int lastPhotoL = -1;
                    static int lastPhotoR = -1;
                    static int lastPot = -1;
                    static int lastJoyX = -1;
                    static int lastJoyY = -1;
                    static uint16_t lastColor = 0xFFFF;
                    static int lastStations = -1;
                    static uint8_t lastButtons = 0xFF;

                    // -------------------------------------------------------------
                    // 1. STATICKÁ MASKA (Vykreslí se pouze jednou při vstupu)
                    // -------------------------------------------------------------
                    if (needsFullRedraw) {
                        gfx.clearScreen(ST77XX_WHITE); // Základní čisté bílé pozadí

                        // Reset stavu pro vynucení čistého prvního vykreslení
                        lastPhotoL = -1;
                        lastPhotoR = -1;
                        lastPot = -1;
                        lastJoyX = -1;
                        lastJoyY = -1;
                        lastColor = 0xFFFF;
                        lastStations = -1;
                        lastButtons = 0xFF;

                        // =========================================================
                        // A) HORNÍ ČÁST (Y = 0 až 140) - Čisté hranaté rámečky
                        // =========================================================

                        // 1. fot_l (Levý fotorezistor)
                        gfx.drawRect(0, 0, 36, 140, ST77XX_BLACK);

                        // 2. Široký středový kolotoč módů (Mode Carousel)
                        gfx.fillRect(36, 0, 178, 140, 0xDEFB); // Světle šedé pozadí
                        gfx.drawRect(36, 0, 178, 140, ST77XX_BLACK);
                        
                        // 3 úrovně velikosti textu (zarovnané na střed):
                        // Úroveň 1 (Nejmenší / světlá):
                        gfx.drawTextPartial(68, 14, "MODE_WIFI_DETECTION", 0x8C71, 0xDEFB, 1);
                        // Úroveň 2 (Střední / tmavá):
                        gfx.drawTextPartial(89, 36, "MODE_BAREVNY", 0x2945, 0xDEFB, 1);
                        
                        // Úroveň 3 (NEJVĚTŠÍ velikost 2 - bez vnitřního rámečku):
                        gfx.drawTextPartial(41, 62, "MODE_MAIN_MENU", ST77XX_BLACK, 0xDEFB, 2);
                        
                        // Úroveň 2 (Střední / tmavá):
                        gfx.drawTextPartial(89, 96, "MODE_SENSORS", 0x2945, 0xDEFB, 1);
                        // Úroveň 1 (Nejmenší / světlá):
                        gfx.drawTextPartial(80, 118, "MODE_GAME_SNAKE", 0x8C71, 0xDEFB, 1);

                        // 3. Wi-Fi sekce
                        gfx.fillRect(214, 0, 70, 140, ST77XX_WHITE);
                        gfx.drawRect(214, 0, 70, 140, ST77XX_BLACK);
                        gfx.drawTextPartial(220, 96, "zarizeni :", ST77XX_BLACK, ST77XX_WHITE, 1);

                        // 4. fot_r (Pravý fotorezistor)
                        gfx.drawRect(284, 0, 36, 140, ST77XX_BLACK);

                        // =========================================================
                        // B) SPODNÍ ČÁST (Y = 141 až 239) - 5 plně rozšířených sekcí
                        // =========================================================
                        gfx.drawLine(0, 140, 319, 140, ST77XX_BLACK);

                        // 1. Potenciometr box (X: 0 až 63)
                        gfx.fillRect(0, 141, 64, 98, ST77XX_WHITE);
                        gfx.drawRect(0, 141, 64, 98, ST77XX_BLACK);
                        gfx.drawTextPartial(23, 145, "pot", ST77XX_BLACK, ST77XX_WHITE, 1);

                        // 2. Tlačítka 5x (D-Pad box) (X: 64 až 159)
                        gfx.fillRect(64, 141, 96, 98, ST77XX_WHITE);
                        gfx.drawRect(64, 141, 96, 98, ST77XX_BLACK);

                        // 3. joy-x box (X: 160 až 213)
                        gfx.fillRect(160, 141, 54, 98, ST77XX_WHITE);
                        gfx.drawRect(160, 141, 54, 98, ST77XX_BLACK);
                        gfx.drawTextPartial(170, 145, "joy-x", ST77XX_BLACK, ST77XX_WHITE, 1);

                        // 4. joy-y box (X: 214 až 267)
                        gfx.fillRect(214, 141, 54, 98, ST77XX_WHITE);
                        gfx.drawRect(214, 141, 54, 98, ST77XX_BLACK);
                        gfx.drawTextPartial(224, 145, "joy-y", ST77XX_BLACK, ST77XX_WHITE, 1);

                        // 5. Barevný senzor box (X: 268 až 319)
                        gfx.fillRect(268, 141, 52, 98, 0x9E3F);
                        gfx.drawRect(268, 141, 52, 98, ST77XX_BLACK);
                        gfx.drawTextPartial(272, 145, "barevny", ST77XX_BLACK, 0x9E3F, 1);
                    }

                    // -------------------------------------------------------------
                    // 2. ŽIVÁ ČÍSLA A GRAFICKÉ PAC-MAN UKAZATELE (Pouze při změně!)
                    // -------------------------------------------------------------

                    // A) Wi-Fi stav a počet připojených zařízení
                    #ifdef ENABLE_WIFI_WEB
                        uint8_t stations = WiFi.softAPgetStationNum();
                    #else
                        uint8_t stations = 0;
                    #endif
                    if (needsFullRedraw || stations != lastStations) {
                        lastStations = stations;
                        uint16_t wifiColor = (stations > 0) ? ST77XX_GREEN : ST77XX_RED;
                        gfx.drawWifiIcon(249, 44, wifiColor);
                        gfx.drawTextPartial(242, 114, String(stations) + " ", wifiColor, ST77XX_WHITE, 2);
                    }

                    // B) Potenciometr plný Pac-man kruh (Žlutý)
                    int potVal = data.potentiometer;
                    if (needsFullRedraw || abs(potVal - lastPot) > 5) {
                        lastPot = potVal;
                        float potPercent = constrain((float)potVal / 4095.0f, 0.0f, 1.0f);
                        gfx.drawPacmanGauge(32, 180, 18, potPercent, ST77XX_YELLOW, 0xDEFB);
                        gfx.drawTextPartial(14, 216, String(potVal) + "   ", 0x4208, ST77XX_WHITE, 1);
                    }

                    // C) 5x Tlačítka (D-Pad visualizer)
                    uint8_t btnMask = (data.btnDown[0] ? 1 : 0) |
                                      (data.btnDown[1] ? 2 : 0) |
                                      (data.btnDown[2] ? 4 : 0) |
                                      (data.btnDown[3] ? 8 : 0) |
                                      (data.btnDown[4] ? 16 : 0);
                    if (needsFullRedraw || btnMask != lastButtons) {
                        lastButtons = btnMask;
                        // Horní tlačítko (btnDown[1] - zelené)
                        gfx.fillCircle(112, 160, 8, data.btnDown[1] ? ST77XX_GREEN : ST77XX_WHITE);
                        gfx.drawCircle(112, 160, 8, ST77XX_BLACK);

                        // Prostřední tlačítko (btnDown[0] - červené)
                        gfx.fillCircle(112, 188, 9, data.btnDown[0] ? ST77XX_RED : ST77XX_WHITE);
                        gfx.drawCircle(112, 188, 9, ST77XX_BLACK);

                        // Levé tlačítko (btnDown[2] - modré)
                        gfx.fillCircle(86, 188, 8, data.btnDown[2] ? ST77XX_BLUE : ST77XX_WHITE);
                        gfx.drawCircle(86, 188, 8, ST77XX_BLACK);

                        // Pravé tlačítko (btnDown[3] - žluté)
                        gfx.fillCircle(138, 188, 8, data.btnDown[3] ? ST77XX_YELLOW : ST77XX_WHITE);
                        gfx.drawCircle(138, 188, 8, ST77XX_BLACK);

                        // Dolní tlačítko (btnDown[4] - azurové)
                        gfx.fillCircle(112, 216, 8, data.btnDown[4] ? ST77XX_CYAN : ST77XX_WHITE);
                        gfx.drawCircle(112, 216, 8, ST77XX_BLACK);
                    }

                    // D) Joystick X plný Pac-man kruh (Azurový / Modrý)
                    int jx = data.joyX;
                    if (needsFullRedraw || abs(jx - lastJoyX) > 10) {
                        lastJoyX = jx;
                        float joyXPercent = constrain((float)jx / 4095.0f, 0.0f, 1.0f);
                        gfx.drawPacmanGauge(187, 180, 16, joyXPercent, ST77XX_CYAN, 0xDEFB);
                        gfx.drawTextPartial(168, 216, String(jx) + "   ", 0x4208, ST77XX_WHITE, 1);
                    }

                    // E) Joystick Y plný Pac-man kruh (Oranžový)
                    int jy = data.joyY;
                    if (needsFullRedraw || abs(jy - lastJoyY) > 10) {
                        lastJoyY = jy;
                        float joyYPercent = constrain((float)jy / 4095.0f, 0.0f, 1.0f);
                        gfx.drawPacmanGauge(241, 180, 16, joyYPercent, 0xFD20, 0xDEFB); // 0xFD20 = Oranžová
                        gfx.drawTextPartial(222, 216, String(jy) + "   ", 0x4208, ST77XX_WHITE, 1);
                    }

                    // F) Barevný senzor náhled
                    uint16_t liveColor = 0x9E3F;
                    if (data.colorR > 0 || data.colorG > 0 || data.colorB > 0) {
                        uint8_t r8 = data.colorR > 255 ? (data.colorR >> 8) : data.colorR;
                        uint8_t g8 = data.colorG > 255 ? (data.colorG >> 8) : data.colorG;
                        uint8_t b8 = data.colorB > 255 ? (data.colorB >> 8) : data.colorB;
                        liveColor = gfx.color565(r8, g8, b8);
                    }
                    if (needsFullRedraw || liveColor != lastColor) {
                        lastColor = liveColor;
                        gfx.fillRect(269, 142, 50, 96, liveColor);
                        gfx.drawRect(268, 141, 52, 98, ST77XX_BLACK);
                        gfx.drawTextPartial(272, 145, "barevny", ST77XX_BLACK, liveColor, 1);
                    }

                    // G) Fotorezistory fot_l a fot_r (Dynamický odstín šedá -> bílá se změnovou hysterezí)
                    // Levý fotorezistor (fot / l)
                    uint8_t grayL = map(constrain((int)data.photo1, 0, 4095), 0, 4095, 40, 255);
                    if (needsFullRedraw || abs((int)grayL - lastPhotoL) >= 3) {
                        lastPhotoL = grayL;
                        uint16_t colL = gfx.color565(grayL, grayL, grayL);
                        uint16_t textColL = (grayL > 130) ? ST77XX_BLACK : ST77XX_WHITE;
                        gfx.fillRect(1, 1, 34, 138, colL);
                        gfx.drawRect(0, 0, 36, 140, ST77XX_BLACK);
                        gfx.drawTextPartial(9, 60, "fot", textColL, colL, 1);
                        gfx.drawTextPartial(15, 72, "l", textColL, colL, 1);
                    }

                    // Pravý fotorezistor (fot / r)
                    uint8_t grayR = map(constrain((int)data.photo2, 0, 4095), 0, 4095, 40, 255);
                    if (needsFullRedraw || abs((int)grayR - lastPhotoR) >= 3) {
                        lastPhotoR = grayR;
                        uint16_t colR = gfx.color565(grayR, grayR, grayR);
                        uint16_t textColR = (grayR > 130) ? ST77XX_BLACK : ST77XX_WHITE;
                        gfx.fillRect(285, 1, 34, 138, colR);
                        gfx.drawRect(284, 0, 36, 140, ST77XX_BLACK);
                        gfx.drawTextPartial(293, 60, "fot", textColR, colR, 1);
                        gfx.drawTextPartial(299, 72, "r", textColR, colR, 1);
                    }
                }
                break;
            }

            // =======================================================
            // MÓD: GYROSKOP / IMU 3D OSY (Návrh 2)
            // =======================================================
            case MODE_SENSORS: {
                delay = 50; // 20 fps
                SensorData data = globalState.getSensorData();
                static float lastAngleX = -999.0f;
                static float lastAngleY = -999.0f;
                static float lastAngleZ = -999.0f;
                
                // Výpočet úhlů z akcelerometru a gyroskopu
                float pitch = atan2(-data.accelX, sqrt(data.accelY * data.accelY + data.accelZ * data.accelZ)) * 57.2957795f;
                float roll  = atan2(data.accelY, data.accelZ) * 57.2957795f;
                float yaw   = data.gyroZ; // Z rychlost otáčení

                // A) STATICKÁ MASKA
                if (needsFullRedraw) {
                    lastAngleX = -999.0f;
                    lastAngleY = -999.0f;
                    lastAngleZ = -999.0f;

                    // Levý panel (oranžovo-béžový 0xFED7, přesně 0 až 119 px)
                    gfx.fillRect(0, 0, 120, 240, 0xFED7);
                    
                    // VELKÝ NADPIS GYROSKOP (Velikost 2)
                    gfx.drawTextPartial(12, 18, "GYROSKOP", ST77XX_BLACK, 0xFED7, 2);
                    
                    // Popisky os
                    gfx.drawTextPartial(8, 75, "X:", ST77XX_BLACK, 0xFED7, 2);
                    gfx.drawTextPartial(8, 125, "Y:", ST77XX_BLACK, 0xFED7, 2);
                    gfx.drawTextPartial(8, 175, "Z:", ST77XX_BLACK, 0xFED7, 2);

                    // Pravý panel (čistě bílý, 121 až 319 px)
                    gfx.fillRect(121, 0, 199, 240, ST77XX_WHITE);
                    
                    // Černá předělová a ohraničující čára
                    gfx.drawRect(0, 0, 320, 240, ST77XX_BLACK);
                    gfx.drawLine(120, 0, 120, 239, ST77XX_BLACK);
                }

                // B) DYNAMICKÁ ČÁST (Pouze při změně)
                if (needsFullRedraw || abs(pitch - lastAngleX) > 0.5f || abs(roll - lastAngleY) > 0.5f || abs(yaw - lastAngleZ) > 0.5f) {
                    lastAngleX = pitch;
                    lastAngleY = roll;
                    lastAngleZ = yaw;

                    // 1. Čísla na levém panelu (vymezený obdélník, nepřeteče přes X=120)
                    gfx.fillRect(30, 72, 88, 22, 0xFED7);
                    gfx.drawTextPartial(32, 75, String(pitch, 1) + "o", ST77XX_BLACK, 0xFED7, 2);

                    gfx.fillRect(30, 122, 88, 22, 0xFED7);
                    gfx.drawTextPartial(32, 125, String(roll, 1) + "o", ST77XX_BLACK, 0xFED7, 2);

                    gfx.fillRect(30, 172, 88, 22, 0xFED7);
                    gfx.drawTextPartial(32, 175, String(yaw, 1) + "o", ST77XX_BLACK, 0xFED7, 2);

                    // Vždy čistě obnovíme předělovou čáru
                    gfx.drawLine(120, 0, 120, 239, ST77XX_BLACK);

                    // 2. Překreslení 3D šipek na pravém panelu (Střed CX = 220, CY = 135)
                    int cx = 220, cy = 135;
                    gfx.fillRect(121, 1, 198, 238, ST77XX_WHITE); // Smazání pravého plátna
                    
                    // Rotace šipek podle úhlů náklonu
                    float radRoll = roll * 0.0174532925f;
                    
                    // A) Modrá šipka (Osa Z - ve výchozím stavu NEJVĚTŠÍ a směřuje přímo NAHORU)
                    int zLen = 75;
                    int zx = cx - (int)(zLen * sin(radRoll));
                    int zy = cy - (int)(zLen * cos(radRoll));
                    gfx.drawArrow(cx, cy, zx, zy, 15, 0x039F); // Modrá
                    
                    // B) Červená šipka (Osa X - ve výchozím stavu šikmo VLEVO DOLŮ 145 deg)
                    int xLen = 58;
                    float radX = (145.0f + roll) * 0.0174532925f;
                    int xx = cx + (int)(xLen * cos(radX));
                    int xy = cy + (int)(xLen * sin(radX));
                    gfx.drawArrow(cx, cy, xx, xy, 14, 0xE986); // Červená

                    // C) Zelená šipka (Osa Y - ve výchozím stavu šikmo VPRAVO DOLŮ 35 deg - ukazují od sebe)
                    int yLen = 58;
                    float radY = (35.0f + roll) * 0.0174532925f;
                    int yx = cx + (int)(yLen * cos(radY));
                    int yy = cy + (int)(yLen * sin(radY));
                    gfx.drawArrow(cx, cy, yx, yy, 14, 0x2444); // Zelená

                    // Středový černý pivot
                    gfx.fillCircle(cx, cy, 10, 0x2124);
                    gfx.drawCircle(cx, cy, 10, ST77XX_BLACK);
                }
                break;
            }

            // =======================================================
            // MÓD: BAREVNÝ SENZOR (Návrh 1)
            // =======================================================
            case MODE_BAREVNY: {
                delay = 50; // 20 fps
                SensorData data = globalState.getSensorData();
                static uint8_t lastR = 255, lastG = 255, lastB = 255;
                
                uint8_t r8 = data.colorR > 255 ? (data.colorR >> 8) : data.colorR;
                uint8_t g8 = data.colorG > 255 ? (data.colorG >> 8) : data.colorG;
                uint8_t b8 = data.colorB > 255 ? (data.colorB >> 8) : data.colorB;
                
                // A) STATICKÁ MASKA
                if (needsFullRedraw) {
                    lastR = 255; lastG = 255; lastB = 255;

                    // Levý panel (oranžovo-béžový 0xFED7, přesně 0 až 119 px)
                    gfx.fillRect(0, 0, 120, 240, 0xFED7);
                    
                    // VELKÝ DVOUŘÁDKOVÝ NADPIS (Velikost 2)
                    gfx.drawTextPartial(14, 14, "BAREVNY", ST77XX_BLACK, 0xFED7, 2);
                    gfx.drawTextPartial(20, 34, "SENZOR", ST77XX_BLACK, 0xFED7, 2);
                    
                    // Popisky složek R, G, B
                    gfx.drawTextPartial(10, 85, "R:", ST77XX_BLACK, 0xFED7, 2);
                    gfx.drawTextPartial(10, 135, "G:", ST77XX_BLACK, 0xFED7, 2);
                    gfx.drawTextPartial(10, 185, "B:", ST77XX_BLACK, 0xFED7, 2);

                    // Černá předělová a ohraničující čára
                    gfx.drawRect(0, 0, 320, 240, ST77XX_BLACK);
                    gfx.drawLine(120, 0, 120, 239, ST77XX_BLACK);
                }

                // B) DYNAMICKÁ ČÁST
                if (needsFullRedraw || abs((int)r8 - lastR) > 3 || abs((int)g8 - lastG) > 3 || abs((int)b8 - lastB) > 3) {
                    lastR = r8;
                    lastG = g8;
                    lastB = b8;

                    // 1. Čísla na levém panelu (vymezený obdélník, nepřeteče přes X=120)
                    gfx.fillRect(36, 82, 82, 22, 0xFED7);
                    gfx.drawTextPartial(38, 85, String(r8), ST77XX_BLACK, 0xFED7, 2);

                    gfx.fillRect(36, 132, 82, 22, 0xFED7);
                    gfx.drawTextPartial(38, 135, String(g8), ST77XX_BLACK, 0xFED7, 2);

                    gfx.fillRect(36, 182, 82, 22, 0xFED7);
                    gfx.drawTextPartial(38, 185, String(b8), ST77XX_BLACK, 0xFED7, 2);

                    // 2. Barva pozadí pravého panelu (121 až 319 px)
                    uint16_t bgColor = gfx.color565(r8, g8, b8);
                    if (r8 == 0 && g8 == 0 && b8 == 0) bgColor = 0xC53F; // Defaultní pastelová fialová
                    
                    gfx.fillRect(121, 1, 198, 238, bgColor);
                    gfx.drawLine(120, 0, 120, 239, ST77XX_BLACK); // Čistá předělová čára

                    // 3. Barevný kruh (Color Wheel) uprostřed pravé části (CX = 220, CY = 120)
                    int cx = 220, cy = 120, radius = 50;
                    gfx.drawColorWheel(cx, cy, radius);

                    // 4. Výpočet Hue a šipka ukazatele
                    float rf = r8 / 255.0f, gf = g8 / 255.0f, bf = b8 / 255.0f;
                    float maxV = max(rf, max(gf, bf));
                    float minV = min(rf, min(gf, bf));
                    float delta = maxV - minV;
                    float hue = 0.0f;
                    if (delta > 0.01f) {
                        if (maxV == rf) hue = 60.0f * fmod(((gf - bf) / delta), 6.0f);
                        else if (maxV == gf) hue = 60.0f * (((bf - rf) / delta) + 2.0f);
                        else hue = 60.0f * (((rf - gf) / delta) + 4.0f);
                        if (hue < 0.0f) hue += 360.0f;
                        
                        // Ručička ukazatele směrem zvenku k dané barvě
                        float hueRad = hue * 0.0174532925f;
                        int tx = cx + (int)(radius * cos(hueRad));
                        int ty = cy + (int)(radius * sin(hueRad));
                        
                        int sx = cx + (int)((radius + 20) * cos(hueRad));
                        int sy = cy + (int)((radius + 20) * sin(hueRad));
                        gfx.drawArrow(sx, sy, tx, ty, 8, ST77XX_BLACK);
                    }
                }
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
    xTaskCreatePinnedToCore(Task_UART_Simulator, "UART_Mock", 4096, NULL, 1, NULL, 0); // Core 0 (Simulátor dat a přepínání módů)
    
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
