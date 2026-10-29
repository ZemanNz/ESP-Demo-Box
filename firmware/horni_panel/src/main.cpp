#include <Arduino.h>
#include <Preferences.h>
#include "SystemState.h"
#include "HardwareSetup.h"
#include "GraphicsManager.h"
#include "SensorManager.h"
#include "UartProtocol.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "hry/snake.h"
#include "hry/flappy_bird.h"
#include "hry/game_2048.h"
#include "hry/tetris.h"
#include "wifi/WebManager.h"

// ---------------------------------------------------------
// Fyzická instance Globálního Stavu (paměť)
// ---------------------------------------------------------
SystemState globalState;
SnakeGame snake;
FlappyGame flappy;
Game2048 g2048;
TetrisGame tetris;
WebManager webManager;

// Trvalé ukládání rekordů her (NVS / Flash paměť)
Preferences gamePrefs;
int bestScoreSnake = 0;
int bestScoreFlappy = 0;
int bestScore2048 = 0;
int bestScoreTetris = 0;

void loadHighScores() {
    gamePrefs.begin("scores", true); // read-only
    bestScoreSnake = gamePrefs.getInt("snake", 0);
    bestScoreFlappy = gamePrefs.getInt("flappy", 0);
    bestScore2048 = gamePrefs.getInt("2048", 0);
    bestScoreTetris = gamePrefs.getInt("tetris", 0);
    gamePrefs.end();
}

void checkAndUpdateHighScore(const char* key, int currentScore, int &bestScore) {
    if (currentScore > bestScore) {
        bestScore = currentScore;
        gamePrefs.begin("scores", false); // read-write
        gamePrefs.putInt(key, bestScore);
        gamePrefs.end();
        Serial.printf("[HIGHSCORE] Novy rekord pro %s: %d!\n", key, bestScore);
    }
}

void resetHighScore(const char* key, int &bestScore) {
    bestScore = 0;
    gamePrefs.begin("scores", false);
    gamePrefs.putInt(key, 0);
    gamePrefs.end();
    Serial.printf("[HIGHSCORE] Rekord pro %s byl resetovan na 0!\n", key);
}

// ---------------------------------------------------------
// 1. Task: Wi-Fi a WebServer (Poběží na Core 0)
// ---------------------------------------------------------
void Task_WiFi_Web(void *pvParameters) {
    Serial.print("Task_WiFi_Web bezi na uvazku (Core): ");
    Serial.println(xPortGetCoreID());

    // Spuštění SoftAP, Captive Portalu, WebServeru a WebSocketu
    webManager.begin(&globalState);

    for (;;) {
        AppMode currentMode = globalState.getMode();
        if (currentMode == MODE_SLEEP) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        webManager.update();
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

// ---------------------------------------------------------
// 2A. Task: Demo Autopilot (Automatické střídání všech módů včetně her)
// ---------------------------------------------------------
void Task_Demo_Autopilot(void *pvParameters) {
    Serial.print("Demo Autopilot bezi na uvazku (Core): ");
    Serial.println(xPortGetCoreID());

    int timeInMode = 0;
    AppMode lastMode = globalState.getMode();

    int lastPot = 0;
    int32_t lastEnc = 0;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(50)); 
        
        AppMode current = globalState.getMode();
        if (current == MODE_SLEEP) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        SensorData data = globalState.getSensorData();

        // 1. Reset časovače při manuální změně módu
        if (current != lastMode) {
            timeInMode = 0;
            lastMode = current;
        }

        // 2. Detekce uživatelské aktivity (tlačítka spodního i horního panelu, joystick, potenciometr, enkodér)
        bool anyBtn = data.btnDown[BTN_DOWN_LEFT] || data.btnDown[BTN_DOWN_CENTER] || 
                      data.btnDown[BTN_DOWN_RIGHT] || data.btnDown[BTN_DOWN_UP] || 
                      data.btnDown[BTN_DOWN_DOWN] || data.joyBtn || data.btnTop;
        bool joyActive = (abs((int)data.joyX - 2048) > 800) || (abs((int)data.joyY - 2048) > 800);
        bool potMoved = abs((int)data.potentiometer - lastPot) > 60;
        bool encMoved = (data.encoderPos != lastEnc);

        if (anyBtn || joyActive || potMoved || encMoved) {
            // Uživatel aktivně kufr ovládá -> resetujeme časovač, aby mu autopilot nepřepnul mód pod rukama!
            timeInMode = 0;
            lastPot = data.potentiometer;
            lastEnc = data.encoderPos;
        } else {
            timeInMode += 50;
        }

        // 3. Stanovení doby setrvání v daném módu
        int requiredDuration = 15000; // Výchozí doba 15 s
        if (current == MODE_WIFI_SPOJENI || current == MODE_WIFI_DETECTION) {
            requiredDuration = 35000; // 35 s pro Wi-Fi obrazovky (QR kód, CSI detekce)
        } else if (current == MODE_GAME_SNAKE || current == MODE_GAME_FLAPPY || current == MODE_2048 || current == MODE_GAME_TETRIS) {
            requiredDuration = 25000; // 25 s pro hry
        }

        // 4. Automatické cyklické přepnutí na další mód (všech 12 módů včetně her)
        if (timeInMode >= requiredDuration) {
            timeInMode = 0;
            AppMode nextMode = MODE_MAIN_MENU;
            
            switch (current) {
                case MODE_MAIN_MENU:      nextMode = MODE_GYRO; break;
                case MODE_GYRO:           nextMode = MODE_GAME_SNAKE; break;
                case MODE_GAME_SNAKE:     nextMode = MODE_GAME_FLAPPY; break;
                case MODE_GAME_FLAPPY:    nextMode = MODE_2048; break;
                case MODE_2048:           nextMode = MODE_GAME_TETRIS; break;
                case MODE_GAME_TETRIS:    nextMode = MODE_VZDALENOST; break;
                case MODE_VZDALENOST:     nextMode = MODE_WIFI_SPOJENI; break;
                case MODE_WIFI_SPOJENI:   nextMode = MODE_WIFI_DETECTION; break;
                case MODE_WIFI_DETECTION: nextMode = MODE_SERVA; break;
                case MODE_SERVA:          nextMode = MODE_MOTOR; break;
                case MODE_MOTOR:          nextMode = MODE_BAREVNY; break;
                case MODE_BAREVNY:        nextMode = MODE_MAIN_MENU; break;
                default:                  nextMode = MODE_MAIN_MENU; break;
            }

            Serial.printf("[DEMO AUTOPILOT] Prepinam na mod: %d (doba: %d s)\n", (int)nextMode, requiredDuration / 1000);
            globalState.setMode(nextMode);
            lastMode = nextMode;
        }
    }
}


void Task_UART(void *pvParameters) {
    unsigned long lastSendTime = 0;
    
    // Vyrovnávací paměti pro pakety
    TopToBottomPacket outPacket;
    BottomToTopPacket inPacket;

    for (;;) {
        AppMode currentMode = globalState.getMode();
        if (currentMode == MODE_SLEEP) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

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
        while (SerialESP.available() > 0) {
            // Kontrola případné textové zprávy Handshake (BOOT:READY)
            if (SerialESP.peek() == 'B') {
                String line = SerialESP.readStringUntil('\n');
                line.trim();
                if (line == "BOOT:READY") {
                    SerialESP.println("BOOT:START");
                    Serial.println("[UART] Spodni panel poslal BOOT:READY -> odesilam BOOT:START!");
                }
                continue;
            }

            // Pokud nemáme ještě celý paket v bufferu, počkáme na další data
            if (SerialESP.available() < sizeof(BottomToTopPacket)) {
                break;
            }

            // Kontrola synchronizačního startovního bajtu 0x55
            if (SerialESP.peek() != UART_FRAME_START_BOTTOM_TO_TOP) {
                SerialESP.read(); // Zahodíme poškozený bajt a hledáme začátek
                continue;
            }

            // Přečtení celého paketu naráz
            SerialESP.readBytes((uint8_t*)&inPacket, sizeof(BottomToTopPacket));

            // Kontrola kontrolního součtu XOR
            uint8_t calculatedCRC = calculateChecksum(
                (const uint8_t*)&inPacket, 
                sizeof(BottomToTopPacket) - 2
            );

            if (inPacket.endByte == UART_FRAME_END && inPacket.checksum == calculatedCRC) {
                // DATA JSOU V POŘÁDKU -> Uložíme do SystemState
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

    unsigned long lastSerialPrint = 0;
    unsigned long lastOtherSensors = 0;
    unsigned long lastUltrasonic = 0;
    unsigned long lastLcdUpdate = 0;
    unsigned long lastDhtUpdate = 0;

    // Prvotní kalibrace gyroskopu po zapnutí
    sensorManager.calibrateIMU();

    for (;;) {
        unsigned long now = millis();
        AppMode currentMode = globalState.getMode();

        // Kontrola otočení rotačního enkodéru pro okamžitou manuální změnu módu
        globalState.checkModeChange();

        // -------------------------------------------------------------
        // VÝKONNÁ LOGIKA PODLE AKTUÁLNÍHO MÓDU (State Machine)
        // -------------------------------------------------------------
        switch (currentMode) {

            // =======================================================
            case MODE_SLEEP: {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            // =======================================================
            case MODE_GYRO: {
                // 1. Integrace gyroskopu LSM6DS3 (každých 10 ms)
                sensorManager.readIMU();

                // 2. Rychlé senzory (každých 40 ms)
                if (now - lastOtherSensors >= 40) {
                    lastOtherSensors = now;
                    sensorManager.readVL53L0X();
                    sensorManager.readIRDistance();
                    sensorManager.readIRObstacle();
                    sensorManager.readPhotoresistors();
                    sensorManager.readTopButton();
                    sensorManager.readColorSensor();

                    SensorData data = globalState.getSensorData();
                    int segVal = -1;
                    if (data.laserDistanceMm > 0 && data.laserDistanceMm < 2000) {
                        segVal = data.laserDistanceMm / 10;
                    } else if (data.irDistanceCm > 0) {
                        segVal = (int)data.irDistanceCm;
                    } else if (data.ultrasonicDistanceCm > 0) {
                        segVal = (int)data.ultrasonicDistanceCm;
                    }
                    sensorManager.set7Segment(segVal);

                    uint32_t stripColor = ((uint32_t)data.colorR << 16) | ((uint32_t)data.colorG << 8) | (uint32_t)data.colorB;
                    uint8_t stripBright = (stripColor == 0) ? 5 : 15;
                    sensorManager.setLedStripColor(stripColor, stripBright);

                    bool led_ir    = data.irObstacle;
                    bool led_ult   = (data.ultrasonicDistanceCm > 0 && data.ultrasonicDistanceCm < 40.0f);
                    bool led_laser = (data.laserDistanceMm > 0 && data.laserDistanceMm < 300);
                    sensorManager.setLeds(led_ir, led_ult, led_laser);
                }

                // 3. Ultrazvuk (každých 100 ms)
                if (now - lastUltrasonic >= 100) {
                    lastUltrasonic = now;
                    sensorManager.readUltrasonic();
                }

                // 4. Znakový LCD 1602 (každých 250 ms)
                if (now - lastLcdUpdate >= 250) {
                    lastLcdUpdate = now;
                    SensorData data = globalState.getSensorData();
                    char l1[17], l2[17];
                    snprintf(l1, sizeof(l1), "X:%+5.1f Y:%+5.1f", data.gyroX, data.gyroY);
                    if (data.imuShock) {
                        snprintf(l2, sizeof(l2), "!DRBNUTI!  %3.1fg", data.imuShockG);
                    } else {
                        snprintf(l2, sizeof(l2), "Z:%+5.1f  [KLID]", data.gyroZ);
                    }
                    sensorManager.writeLCD1602(String(l1), String(l2));
                }

                break;
            }

            // =======================================================
            case MODE_GAME_SNAKE: {
                sensorManager.readIMU();

                static int step_led_counter = 3;

                if (now - lastLcdUpdate >= 250) {
                    lastLcdUpdate = now;
                    char l2[17];
                    snprintf(l2, sizeof(l2), "S:%-3d  BEST:%-3d", snake.getScore(), bestScoreSnake);
                    sensorManager.writeLCD1602("HRA: HAD (SNAKE)", String(l2));
                }
                if(now - lastOtherSensors >= 400) {
                    lastOtherSensors = now;
                    uint32_t leds[8] = {0};
                    int appleIndex = 7; // Jablko na konci pásku (červená)
                    leds[appleIndex] = 0xFF0000;

                    step_led_counter++;
                    if(step_led_counter > 6) step_led_counter = 0;

                    // Hlava a tělo hada lezou od 0 k 6
                    int head = step_led_counter;
                    int body1 = head -1;
                    int body2 = head -2;

                    
                    if(body1 >= 0) leds[body1] = 0x008800; // Tělo tmavší zelená
                    if(body2 >= 0) leds[body2] = 0x003300;

                    leds[head]  = 0x00FF00; // Hlava jasně zelená

                    // Zápis na horní pásek (jemná intenzita):
                    sensorManager.setLedStrip(leds, 8);

                }

                 // Vestavěná RGB LED na desce:
                if (snake.isGameOver()) {
                    sensorManager.setBuiltinRgb(15, 0, 0);  // Jemná červená při Game Over
                } else {
                    sensorManager.setBuiltinRgb(0, 5, 0);   // Jemná zelená při hře
                }

#if defined(ENABLE_7SEG_GAME_TEXT) && ENABLE_7SEG_GAME_TEXT
                sensorManager.set7SegmentText(snake.isGameOver() ? " gg" : "LOL");
#endif
                    
                break;
            }

            // =======================================================
            case MODE_GAME_FLAPPY: {
                sensorManager.readIMU();

                // 1. Znakový LCD 1602 displej (každých 250 ms)
                if (now - lastLcdUpdate >= 250) {
                    lastLcdUpdate = now;
                    char l2[17];
                    snprintf(l2, sizeof(l2), "S:%-3d  BEST:%-3d", flappy.getScore(), bestScoreFlappy);
                    sensorManager.writeLCD1602("HRA: FLAPPY BIRD", String(l2));
                }

                // 2. Horní LED pásek: žlutý bod (ptáček) se posouvá, proti němu jdou 2 zelené body (trubky), pak se vymění a jdou zpět
                if (now - lastOtherSensors >= 140) {
                    lastOtherSensors = now;
                    static int birdPos = 0;
                    static int birdDir = 1;
                    static int pipePos = 6;
                    static int pipeDir = -1;

                    uint32_t stripLeds[8] = {0};

                    // Překážka: 2 zelené body
                    if (pipePos >= 0 && pipePos < 8) stripLeds[pipePos] = 0x00FF00;
                    if (pipePos + 1 >= 0 && pipePos + 1 < 8) stripLeds[pipePos + 1] = 0x00FF00;

                    // Ptáček: 1 žlutý bod (má přednost)
                    if (birdPos >= 0 && birdPos < 8) stripLeds[birdPos] = 0xFFFF00;

                    sensorManager.setLedStrip(stripLeds, 8); // Jemná intenzita 8

                    birdPos += birdDir;
                    pipePos += pipeDir;

                    if (birdPos >= 7 && pipePos <= -1) {
                        birdDir = -1;
                        pipeDir = 1;
                    } else if (birdPos <= 0 && pipePos >= 6) {
                        birdDir = 1;
                        pipeDir = -1;
                    }
                }

                // 3. Vestavěná RGB LED na desce: zelená při hře, červená při Game Over
                if (flappy.getGameOver()) {
                    sensorManager.setBuiltinRgb(15, 0, 0);  // Jemná červená při Game Over
                } else {
                    sensorManager.setBuiltinRgb(0, 5, 0);   // Jemná zelená při hře
                }

#if defined(ENABLE_7SEG_GAME_TEXT) && ENABLE_7SEG_GAME_TEXT
                sensorManager.set7SegmentText(flappy.getGameOver() ? " gg" : "LOL");
#endif
                break;
            }

            // =======================================================
            case MODE_2048: {
                sensorManager.readIMU();

                // 1. Znakový LCD 1602 displej (každých 250 ms)
                if (now - lastLcdUpdate >= 250) {
                    lastLcdUpdate = now;
                    char l2[17];
                    snprintf(l2, sizeof(l2), "S:%-4d BEST:%-4d", g2048.getScore(), bestScore2048);
                    sensorManager.writeLCD1602("HRA: 2048", String(l2));
                }

                // 2. Horní LED pásek: svítí v barvě nejvyšší aktuálně získané hodnoty (dlaždice) na ploše
                if (now - lastOtherSensors >= 100) {
                    lastOtherSensors = now;
                    int maxTile = g2048.getMaxTile();
                    uint32_t tileColor = 0x00FFFF; // default pro 2
                    switch (maxTile) {
                        case 2:    tileColor = 0x00FFFF; break; // Azurová (Cyan)
                        case 4:    tileColor = 0x0000FF; break; // Sytá modrá
                        case 8:    tileColor = 0x00FF00; break; // Jasná zelená
                        case 16:   tileColor = 0xFFFF00; break; // Žlutá
                        case 32:   tileColor = 0xFFA500; break; // Oranžová
                        case 64:   tileColor = 0xFF0000; break; // Červená
                        case 128:  tileColor = 0xFF00FF; break; // Purpurová / Magenta
                        case 256:  tileColor = 0xFF1493; break; // Sytě růžová (Deep Pink)
                        case 512:  tileColor = 0x00FF80; break; // Jarní mořská zelená (Spring Green)
                        case 1024: tileColor = 0x80FF00; break; // Limetková (Chartreuse)
                        case 2048: tileColor = 0xFFD700; break; // Zlatá (Gold)
                        default:   tileColor = 0xFFFFFF; break; // Bílá
                    }
                    sensorManager.setLedStripColor(tileColor, 8); // Jemná intenzita 8
                }

                // 3. Vestavěná RGB LED na desce: zelená při hře, červená při Game Over
                if (g2048.getState() == G2048_GAME_OVER) {
                    sensorManager.setBuiltinRgb(15, 0, 0);  // Jemná červená při Game Over
                } else {
                    sensorManager.setBuiltinRgb(0, 5, 0);   // Jemná zelená při hře
                }

#if defined(ENABLE_7SEG_GAME_TEXT) && ENABLE_7SEG_GAME_TEXT
                sensorManager.set7SegmentText((g2048.getState() == G2048_GAME_OVER) ? " gg" : "LOL");
#endif
                break;
            }

            // =======================================================
            case MODE_GAME_TETRIS: {
                sensorManager.readIMU();

                // 1. Znakový LCD 1602 displej (každých 250 ms)
                if (now - lastLcdUpdate >= 250) {
                    lastLcdUpdate = now;
                    char l2[17];
                    snprintf(l2, sizeof(l2), "S:%-4d BEST:%-4d", tetris.getScore(), bestScoreTetris);
                    sensorManager.writeLCD1602("HRA: TETRIS", String(l2));
                }

                // 2. Horní LED pásek: nízkou intenzitou rozsvícený barvou aktuálního padajícího dílku
                if (now - lastOtherSensors >= 100) {
                    lastOtherSensors = now;

                    // 0: I (Azurová), 1: J (Modrá), 2: L (Oranžová), 3: O (Žlutá), 4: S (Zelená), 5: T (Fialová), 6: Z (Červená)
                    static const uint32_t BARVY_STRIP[7] = {
                        0x00FFFF, // 0: I - Azurová
                        0x0000FF, // 1: J - Modrá
                        0xFFA500, // 2: L - Oranžová
                        0xFFFF00, // 3: O - Žlutá
                        0x00FF00, // 4: S - Zelená
                        0x800080, // 5: T - Fialová
                        0xFF0000  // 6: Z - Červená
                    };

                    int curType = tetris.getCurrentType();
                    uint32_t barvaDilku = (curType >= 0 && curType < 7) ? BARVY_STRIP[curType] : 0x00FFFF;

                    // Velmi jemná nízká intenzita (brightness = 5 z 255 pro neoslňující svit)
                    sensorManager.setLedStripColor(barvaDilku, 5);
                }

                // 3. Vestavěná RGB LED na desce (Zelená jemně svítící při hře, Červená při Game Over)
                if (tetris.isGameOver()) {
                    sensorManager.setBuiltinRgb(15, 0, 0);  // Jemná červená při Game Over
                } else {
                    sensorManager.setBuiltinRgb(0, 5, 0);   // Jemná zelená, minimální intenzita při hře
                }

#if defined(ENABLE_7SEG_GAME_TEXT) && ENABLE_7SEG_GAME_TEXT
                sensorManager.set7SegmentText(tetris.isGameOver() ? " gg" : "LOL");
#endif
                break;
            }

            // =======================================================
            case MODE_MAIN_MENU:
            case MODE_VZDALENOST:
            case MODE_WIFI_SPOJENI:
            case MODE_WIFI_DETECTION:
            case MODE_SERVA:
            case MODE_MOTOR:
            case MODE_BAREVNY:
            default: {
                // Původní společná logika senzorů a displeje pro ostatní režimy:
                sensorManager.readIMU();

                if (now - lastOtherSensors >= 40) {
                    lastOtherSensors = now;
                    sensorManager.readVL53L0X();
                    sensorManager.readIRDistance();
                    sensorManager.readIRObstacle();
                    sensorManager.readPhotoresistors();
                    sensorManager.readTopButton();
                    sensorManager.readColorSensor();

                    SensorData data = globalState.getSensorData();
                    int segVal = -1;
                    if (data.laserDistanceMm > 0 && data.laserDistanceMm < 2000) {
                        segVal = data.laserDistanceMm / 10;
                    } else if (data.irDistanceCm > 0) {
                        segVal = (int)data.irDistanceCm;
                    } else if (data.ultrasonicDistanceCm > 0) {
                        segVal = (int)data.ultrasonicDistanceCm;
                    }
                    sensorManager.set7Segment(segVal);

                    sensorManager.setBuiltinRgb(0, 0, 0);

                    uint32_t stripColor = ((uint32_t)data.colorR << 16) | ((uint32_t)data.colorG << 8) | (uint32_t)data.colorB;
                    uint8_t stripBright = (stripColor == 0) ? 5 : 15;
                    sensorManager.setLedStripColor(stripColor, stripBright);

                    bool led_ir    = data.irObstacle;
                    bool led_ult   = (data.ultrasonicDistanceCm > 0 && data.ultrasonicDistanceCm < 40.0f);
                    bool led_laser = (data.laserDistanceMm > 0 && data.laserDistanceMm < 300);
                    sensorManager.setLeds(led_ir, led_ult, led_laser);
                }

                if (now - lastUltrasonic >= 100) {
                    lastUltrasonic = now;
                    sensorManager.readUltrasonic();
                }

                if (now - lastLcdUpdate >= 250) {
                    lastLcdUpdate = now;
                    SensorData data = globalState.getSensorData();
                    String lcdLine1 = "T:" + String(data.temperature, 1) + "C H:" + String(data.humidity, 0) + "%";
                    String lcdLine2;
                    if (data.laserDistanceMm > 0 && data.laserDistanceMm < 2000) {
                        lcdLine2 = "Laser: " + String(data.laserDistanceMm) + " mm";
                    } else if (data.irDistanceCm > 0) {
                        lcdLine2 = "IR: " + String(data.irDistanceCm, 1) + " cm";
                    } else if (data.ultrasonicDistanceCm > 0) {
                        lcdLine2 = "Ultra: " + String(data.ultrasonicDistanceCm, 1) + " cm";
                    } else {
                        lcdLine2 = "IR: " + String(data.irObstacle ? "OBSTACLE" : "CLEAR");
                    }
                    sensorManager.writeLCD1602(lcdLine1, lcdLine2);
                }

                if (now - lastDhtUpdate >= 1500) {
                    lastDhtUpdate = now;
                    sensorManager.readDHT();
                }
                break;
            }
        }

        // -------------------------------------------------------------
        // 6. PERIODICKÝ VÝPIS TELEMETRIE NA SÉRIOVÝ MONITOR (každých 1000 ms)
        // -------------------------------------------------------------
        if (now - lastSerialPrint >= 1000) {
            lastSerialPrint = now;
            SensorData data = globalState.getSensorData();
            bool led_ir    = data.irObstacle;
            bool led_ult   = (data.ultrasonicDistanceCm > 0 && data.ultrasonicDistanceCm < 40.0f);
            bool led_laser = (data.laserDistanceMm > 0 && data.laserDistanceMm < 300);

            Serial.println("\n========== [TELEMETRIE HORNI PANEL] ==========");
            Serial.printf("  DHT11:        Teplota: %.1f °C | Vlhkost: %.1f %%\n", data.temperature, data.humidity);
            Serial.printf("  Laser VL53:   %d mm\n", data.laserDistanceMm);
            Serial.printf("  Ultrazvuk:    %.1f cm\n", data.ultrasonicDistanceCm);
            Serial.printf("  IR1 (vzdal.): %.1f cm (ADC2_CH9 GPIO 20 = %d)\n", data.irDistanceCm, analogRead(PIN_IR1));
            Serial.printf("  IR2 (prekazka): %s (GPIO 38)\n", data.irObstacle ? "PREKAZKA!" : "CISTO");
            Serial.printf("  LEDky nad senz: IR(D1)=%d | ULT(D4)=%d | LASER(D5)=%d\n", led_ir, led_ult, led_laser);
            Serial.printf("  Fotorezistory: LDR1 (pin 1)=%d | LDR2 (pin 2)=%d\n", data.photo1, data.photo2);
            Serial.printf("  Barevny TCS:  R=%d, G=%d, B=%d, Clear=%d\n", data.colorR, data.colorG, data.colorB, data.colorClear);
            Serial.printf("  Uhy IMU:      Roll(X)=%.1f° | Pitch(Y)=%.1f° | Yaw(Z)=%.1f°\n",
                          data.gyroX, data.gyroY, data.gyroZ);
            Serial.printf("  Zrychleni:    Ax=%.2f g, Ay=%.2f g, Az=%.2f g\n",
                          data.accelX, data.accelY, data.accelZ);
            Serial.printf("  Otres / Tap:  %s (%.2f g)\n",
                          data.imuShock ? ">>> DRBNUTI DETEKOVANO! <<<" : "Klid", data.imuShockG);
            Serial.printf("  Tlacitko TOP: %s\n", data.btnTop ? "STISKNUTO" : "VOLNO");
            Serial.printf("  Dolni panel (UART): Pot: %4d | Joy: X=%4d, Y=%4d, Btn=%d | Tlacitka: [L:%d C:%d R:%d U:%d D:%d] | Enc: %d\n",
                          data.potentiometer, data.joyX, data.joyY, data.joyBtn,
                          data.btnDown[BTN_DOWN_LEFT], data.btnDown[BTN_DOWN_CENTER], data.btnDown[BTN_DOWN_RIGHT],
                          data.btnDown[BTN_DOWN_UP], data.btnDown[BTN_DOWN_DOWN],
                          data.encoderPos);
            Serial.println("================================================");
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // 10 ms takt pro rychlou integraci gyroskopu
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
    static bool lastBtnDown[5] = {false, false, false, false, false};
    static bool lastBtnTop = false;
    static int32_t lastEncoderPos = 0;

    for (;;) {
        // 1. Zjistíme, v jakém stavu se kufr zrovna nachází
        AppMode currentMode = globalState.getMode();

        // ---------------------------------------------------------
        // OBSLUHA FYZICKÝCH / VIRTUÁLNÍCH TLAČÍTEK A ENKODÉRU PRO PŘEPÍNÁNÍ MÓDŮ
        // ---------------------------------------------------------
        SensorData currentSensors = globalState.getSensorData();

        // Horní fyzické tlačítko: Návrat do Menu nebo info
        if (currentSensors.btnTop && !lastBtnTop) {
            if (currentMode != MODE_MAIN_MENU) {
                globalState.setMode(MODE_MAIN_MENU);
                currentMode = MODE_MAIN_MENU;
            } else {
                globalState.toggleInfo();
            }
        }
        lastBtnTop = currentSensors.btnTop;
        
        // 2. Potřebujeme kompletně překreslit obrazovku? 
        // (Vlajka je true jen těsně po přepnutí stavu nebo stisku tlačítka)
        bool needsFullRedraw = globalState.popUiNeedsUpdate();

        //vykreslujeme info?
        bool infoActive = globalState.isInfoActive();

        // 3. Vykreslujeme podle módu
        switch (currentMode) {
            
            // =======================================================
            case MODE_SLEEP: {
                Serial.println("\n[SYSTEM] Pozadavek na rezim Light Sleep...");

                // 1. Zhasneme TFT displej a vypneme jeho podsvícení
                gfx.clearScreen(ST77XX_BLACK);
                #if defined(PIN_TFT_LED) && (PIN_TFT_LED >= 0)
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

                // 7. Vyčkáme na uvolnění horního tlačítka!
                // (Uživatel ho před chvílí 1s držel pro uspání, takže pin 47 je v této chvíli stále LOW.
                // Kdybychom hned povolili wake-up na LOW úroveň, procesor by se v tu stejnou mikrosekundu okamžitě probudil!)
                Serial.println("[SYSTEM] Cekam na uvolneni tlacitka TOP...");
                unsigned long waitBtn = millis();
                #ifdef PIN_BTN1
                while (digitalRead(PIN_BTN1) == LOW && (millis() - waitBtn < 3000)) {
                    vTaskDelay(pdMS_TO_TICKS(40));
                }
                #endif
                vTaskDelay(pdMS_TO_TICKS(200)); // Debounce prodleva pro naprostý klid kontaktů

                // 8. Uvedeme přerušení gyroskopu LSM6DS3 do klidu:
                // Při mačkání tlačítka nebo držení kufru došlo k pohybu, takže pin INT1 (GPIO 45)
                // mohl sepnout do HIGH. Přečtením registru 0x1B (WAKE_UP_SRC) vyčistíme příznak a shodíme pin do LOW.
                #ifdef ENABLE_LSM6DS3
                Wire.beginTransmission(0x6B);
                Wire.write(0x1B);
                Wire.endTransmission(false);
                Wire.requestFrom((uint8_t)0x6B, (uint8_t)1);
                if (Wire.available()) Wire.read();

                Wire.beginTransmission(0x6A);
                Wire.write(0x1B);
                Wire.endTransmission(false);
                Wire.requestFrom((uint8_t)0x6A, (uint8_t)1);
                if (Wire.available()) Wire.read();

                #ifdef PIN_IMU_INT
                int gyroClearTries = 10;
                while (digitalRead(PIN_IMU_INT) == HIGH && gyroClearTries-- > 0) {
                    Wire.beginTransmission(0x6B);
                    Wire.write(0x1B);
                    Wire.endTransmission(false);
                    Wire.requestFrom((uint8_t)0x6B, (uint8_t)1);
                    if (Wire.available()) Wire.read();
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                g_imuHwInterruptFired = false;
                #endif
                #endif

                // 9. Vypneme Wi-Fi rádio před spánkem (běžící Wi-Fi AP by periodicky budil light sleep)
                #ifdef ENABLE_WIFI_WEB
                esp_wifi_stop();
                vTaskDelay(pdMS_TO_TICKS(50));
                #endif

                // 10. Vyprázdníme sériové buffery
                Serial.println("[SYSTEM] Vstupuji do Light Sleep (probuzeni tuknutim prstem do kufru nebo stiskem tlacitka)...");
                Serial.flush();
                #ifdef ENABLE_UART_ESP
                    SerialESP.flush();
                #endif


                // Odeslání informace spodnímu panelu, že přecházíme do spánku
                TopToBottomPacket sleepPkt;
                memset(&sleepPkt, 0, sizeof(sleepPkt));
                sleepPkt.startByte = UART_FRAME_START_TOP_TO_BOTTOM; // 0xAA
                sleepPkt.currentMode = (uint8_t)MODE_SLEEP;
                sleepPkt.endByte = UART_FRAME_END; // 0xFE
                sleepPkt.checksum = calculateChecksum((const uint8_t*)&sleepPkt, sizeof(TopToBottomPacket) - 2);

                SerialESP.write((const uint8_t*)&sleepPkt, sizeof(TopToBottomPacket));
                SerialESP.flush(); // Počkáme, až všechny bajty fyzicky odejdou z TX pinu do drátu
                vTaskDelay(pdMS_TO_TICKS(150)); // Krátká pauza, aby to spodní panel stihl zpracovat

                while (SerialESP.available()) {
                    SerialESP.read();
                }

                // 11. Nastavení hardwarových zdrojů probuzení:
                // A) Horní tlačítko (stisk spojí na GND = LOW úroveň)
                #ifdef PIN_BTN1
                    gpio_wakeup_enable((gpio_num_t)PIN_BTN1, GPIO_INTR_LOW_LEVEL);
                #endif

                // B) Gyroskop LSM6DS3 (INT1 – puls HIGH při otřesu/ťuknutí prstem)
                #ifdef PIN_IMU_INT
                    gpio_wakeup_enable((gpio_num_t)PIN_IMU_INT, GPIO_INTR_HIGH_LEVEL);
                #endif

                // C) UART linka (RX) - probuzení od spodního panelu při stisku tlačítka dole
                #if defined(ENABLE_UART_ESP) && defined(UART_ESP_RX)
                if (digitalRead(UART_ESP_RX) == HIGH) {
                    gpio_wakeup_enable((gpio_num_t)UART_ESP_RX, GPIO_INTR_LOW_LEVEL);
                }
                #endif

                esp_sleep_enable_gpio_wakeup();

                // 12. VSTUP DO SKUTEČNÉHO LEHKÉHO SPÁNKU
                esp_light_sleep_start();

                // ===================================================
                // 13. PROBUZENÍ! (Kód pokračuje okamžitě zde)
                // ===================================================
                esp_sleep_wakeup_cause_t duvod = esp_sleep_get_wakeup_cause();

                // Okamžitě odečteme stav pinů a periferií PŘED jakýmikoliv prodlevami:
                #ifdef PIN_BTN1
                bool btn1Pressed = (digitalRead(PIN_BTN1) == LOW);
                #else
                bool btn1Pressed = false;
                #endif

                #ifdef PIN_IMU_INT
                bool imuPinActive = (digitalRead(PIN_IMU_INT) == HIGH);
                bool imuIsrFired = g_imuHwInterruptFired;
                // Přečteme registr WAKE_UP_SRC (0x1B) z gyroskopu LSM6DS3
                uint8_t lsmWakeSrc = 0;
                Wire.beginTransmission(0x6B);
                Wire.write(0x1B);
                Wire.endTransmission(false);
                Wire.requestFrom((uint8_t)0x6B, (uint8_t)1);
                if (Wire.available()) lsmWakeSrc = Wire.read();
                if (lsmWakeSrc == 0) {
                    Wire.beginTransmission(0x6A);
                    Wire.write(0x1B);
                    Wire.endTransmission(false);
                    Wire.requestFrom((uint8_t)0x6A, (uint8_t)1);
                    if (Wire.available()) lsmWakeSrc = Wire.read();
                }
                bool gyroWoke = imuIsrFired || imuPinActive || ((lsmWakeSrc & 0x0F) != 0);
                #else
                bool gyroWoke = false;
                uint8_t lsmWakeSrc = 0;
                bool imuIsrFired = false;
                bool imuPinActive = false;
                #endif

                #if defined(ENABLE_UART_ESP) && defined(UART_ESP_RX)
                bool uartActive = (digitalRead(UART_ESP_RX) == LOW) || (SerialESP.available() > 0);
                #else
                bool uartActive = false;
                #endif

                // Okamžitě deaktivujeme wake-up, aby se neopakovalo
                esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
                #ifdef PIN_BTN1
                    gpio_wakeup_disable((gpio_num_t)PIN_BTN1);
                #endif
                #ifdef PIN_IMU_INT
                    gpio_wakeup_disable((gpio_num_t)PIN_IMU_INT);
                #endif
                #if defined(ENABLE_UART_ESP) && defined(UART_ESP_RX)
                    gpio_wakeup_disable((gpio_num_t)UART_ESP_RX);
                #endif

                #ifdef ENABLE_UART_ESP
                // Okamžitě pošleme probouzecí puls pro spodní panel (probudí ho na GPIO 16)
                SerialESP.write((uint8_t)0x00);
                SerialESP.flush();
                #endif

                // Pokud byl probuzen tlačítkem, počkáme na jeho uvolnění, aby neprokliklo v Menu
                #ifdef PIN_BTN1
                if (btn1Pressed) {
                    while (digitalRead(PIN_BTN1) == LOW) {
                        vTaskDelay(pdMS_TO_TICKS(20));
                    }
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
                #endif

                // Znovu zapneme Wi-Fi
                #ifdef ENABLE_WIFI_WEB
                esp_wifi_start();
                #endif

                #if defined(PIN_TFT_LED) && (PIN_TFT_LED >= 0)
                    digitalWrite(PIN_TFT_LED, HIGH);
                #endif

                #ifdef ENABLE_LCD1602
                    sensorManager.setLCDBacklight(true);
                #endif

                Serial.printf("\n[SYSTEM] Probudil jsem se z Light Sleep! (Duvod kod: %d) -> ", (int)duvod);
                if (gyroWoke) {
                    Serial.printf("PROBUZENO OTRESEM / TUKNUTIM DO GYROSKOPU! (src=0x%02X, isr=%d, pin=%d)\n",
                                  lsmWakeSrc, (int)imuIsrFired, (int)imuPinActive);
                } else if (uartActive || (!btn1Pressed && !gyroWoke)) {
                    Serial.printf("PROBUZENO ZE SPODNIHO PANELU (UART)! (dostupno bajtu: %d)\n",
                                  SerialESP.available());
                } else if (btn1Pressed) {
                    Serial.println("PROBUZENO STISKEM HORNIHO TLACITKA (GPIO 47)!");
                } else {
                    Serial.printf("PROBUZENO JINYM ZDROJEM: %d\n", (int)duvod);
                }

                // Krátká prodleva na stabilizaci po probuzení
                vTaskDelay(pdMS_TO_TICKS(100));

                // Vrátíme se do Hlavního Menu
                globalState.setMode(MODE_MAIN_MENU);
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
                        // Horní tlačítko (BTN_DOWN_UP = index 3 - zelené)
                        gfx.fillCircle(112, 160, 8, data.btnDown[BTN_DOWN_UP] ? ST77XX_GREEN : ST77XX_WHITE);
                        gfx.drawCircle(112, 160, 8, ST77XX_BLACK);

                        // Dolní tlačítko (BTN_DOWN_DOWN = index 4 - azurové)
                        gfx.fillCircle(112, 216, 8, data.btnDown[BTN_DOWN_DOWN] ? ST77XX_CYAN : ST77XX_WHITE);
                        gfx.drawCircle(112, 216, 8, ST77XX_BLACK);

                        // Levé tlačítko (BTN_DOWN_LEFT = index 0 - modré)
                        gfx.fillCircle(86, 188, 8, data.btnDown[BTN_DOWN_LEFT] ? ST77XX_BLUE : ST77XX_WHITE);
                        gfx.drawCircle(86, 188, 8, ST77XX_BLACK);

                        // Pravé tlačítko (BTN_DOWN_RIGHT = index 2 - žluté)
                        gfx.fillCircle(138, 188, 8, data.btnDown[BTN_DOWN_RIGHT] ? ST77XX_YELLOW : ST77XX_WHITE);
                        gfx.drawCircle(138, 188, 8, ST77XX_BLACK);

                        // Prostřední tlačítko (BTN_DOWN_CENTER = index 1 - červené)
                        gfx.fillCircle(112, 188, 9, data.btnDown[BTN_DOWN_CENTER] ? ST77XX_RED : ST77XX_WHITE);
                        gfx.drawCircle(112, 188, 9, ST77XX_BLACK);
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
            case MODE_GYRO: {
                delay = 40; // 25 fps
                SensorData data = globalState.getSensorData();
                static float lastAngleX = -999.0f;
                static float lastAngleY = -999.0f;
                static float lastAngleZ = -999.0f;
                static bool lastShock = false;
                
                // Relativní úhly natočení kufru v jednotlivých osách [stupně]
                float angX = data.gyroX; // Osa X (naklopení)
                float angY = data.gyroY; // Osa Y (otočení)
                float angZ = data.gyroZ; // Osa Z (náklon do stran)

                // A) STATICKÁ MASKA (překreslení rámu a popisků při přepnutí obrazovky)
                if (needsFullRedraw) {
                    lastAngleX = -999.0f;
                    lastAngleY = -999.0f;
                    lastAngleZ = -999.0f;
                    lastShock = !data.imuShock;

                    // Levý panel (oranžovo-béžový 0xFED7, přesně 0 až 119 px)
                    gfx.fillRect(0, 0, 120, 240, 0xFED7);
                    
                    // VELKÝ NADPIS GYROSKOP (Velikost 2)
                    gfx.drawTextPartial(12, 18, "GYROSKOP", ST77XX_BLACK, 0xFED7, 2);
                    
                    // Popisky os
                    gfx.drawTextPartial(8, 70, "X:", ST77XX_BLACK, 0xFED7, 2);
                    gfx.drawTextPartial(8, 115, "Y:", ST77XX_BLACK, 0xFED7, 2);
                    gfx.drawTextPartial(8, 160, "Z:", ST77XX_BLACK, 0xFED7, 2);

                    // Pravý panel (čistě bílý, 121 až 319 px)
                    gfx.fillRect(121, 0, 199, 240, ST77XX_WHITE);
                    
                    // Černá předělová a ohraničující čára
                    gfx.drawRect(0, 0, 320, 240, ST77XX_BLACK);
                    gfx.drawLine(120, 0, 120, 239, ST77XX_BLACK);
                }

                // B) DYNAMICKÁ ČÁST (Pouze při změně o více než 0.4 stupně)
                if (needsFullRedraw || abs(angX - lastAngleX) > 0.4f || abs(angY - lastAngleY) > 0.4f || abs(angZ - lastAngleZ) > 0.4f) {
                    lastAngleX = angX;
                    lastAngleY = angY;
                    lastAngleZ = angZ;

                    // 1. Čísla na levém panelu (vymezený obdélník, nepřeteče přes X=120)
                    gfx.fillRect(30, 67, 88, 22, 0xFED7);
                    gfx.drawTextPartial(32, 70, String(angX, 1) + "o", ST77XX_BLACK, 0xFED7, 2);

                    gfx.fillRect(30, 112, 88, 22, 0xFED7);
                    gfx.drawTextPartial(32, 115, String(angY, 1) + "o", ST77XX_BLACK, 0xFED7, 2);

                    gfx.fillRect(30, 157, 88, 22, 0xFED7);
                    gfx.drawTextPartial(32, 160, String(angZ, 1) + "o", ST77XX_BLACK, 0xFED7, 2);

                    // Vždy čistě obnovíme předělovou čáru
                    gfx.drawLine(120, 0, 120, 239, ST77XX_BLACK);

                    // 2. Překreslení 3D šipek na pravém panelu (Střed CX = 220, CY = 135)
                    int cx = 220, cy = 135;
                    gfx.fillRect(121, 1, 198, 238, ST77XX_WHITE); // Smazání pravého plátna
                    
                    // A) Modrá šipka (Osa Z - ve výchozím stavu míří NAHORU, rotuje s angZ)
                    int zLen = 75;
                    float radZ = (90.0f - angZ) * 0.0174532925f;
                    int zx = cx + (int)(zLen * cos(radZ));
                    int zy = cy - (int)(zLen * sin(radZ));
                    gfx.drawArrow(cx, cy, zx, zy, 15, 0x039F); // Modrá Z
                    
                    // B) Červená šipka (Osa X - ve výchozím stavu šikmo VLEVO DOLŮ 145 deg, rotuje s angX)
                    int xLen = 58;
                    float radX = (145.0f + angX) * 0.0174532925f;
                    int xx = cx + (int)(xLen * cos(radX));
                    int xy = cy + (int)(xLen * sin(radX));
                    gfx.drawArrow(cx, cy, xx, xy, 14, 0xE986); // Červená X

                    // C) Zelená šipka (Osa Y - ve výchozím stavu šikmo VPRAVO DOLŮ 35 deg, rotuje s angY)
                    int yLen = 58;
                    float radY = (35.0f - angY) * 0.0174532925f;
                    int yx = cx + (int)(yLen * cos(radY));
                    int yy = cy + (int)(yLen * sin(radY));
                    gfx.drawArrow(cx, cy, yx, yy, 14, 0x2444); // Zelená Y

                    // Středový černý pivot
                    gfx.fillCircle(cx, cy, 10, 0x2124);
                    gfx.drawCircle(cx, cy, 10, ST77XX_BLACK);
                }

                // C) DETEKCE DRBNUTÍ / OTŘESU (v levém panelu dole Y: 194 až 234)
                if (needsFullRedraw || data.imuShock != lastShock) {
                    lastShock = data.imuShock;
                    if (data.imuShock) {
                        gfx.fillRect(4, 194, 112, 40, ST77XX_RED);
                        gfx.drawRect(4, 194, 112, 40, ST77XX_WHITE);
                        gfx.drawTextPartial(8, 199, "! DRBNUTI !", ST77XX_WHITE, ST77XX_RED, 1);
                        char bufG[18];
                        snprintf(bufG, sizeof(bufG), "Sila: %.2fg", data.imuShockG);
                        gfx.drawTextPartial(12, 217, bufG, ST77XX_YELLOW, ST77XX_RED, 1);
                    } else {
                        gfx.fillRect(4, 194, 112, 40, 0x1AE4); // Tmavě tyrkysová
                        gfx.drawRect(4, 194, 112, 40, ST77XX_BLACK);
                        gfx.drawTextPartial(14, 199, "STAV: KLID", ST77XX_WHITE, 0x1AE4, 1);
                        gfx.drawTextPartial(8, 217, "Citl: 1 (125mg)", 0xCE59, 0x1AE4, 1);
                    }
                    gfx.drawLine(120, 194, 120, 239, ST77XX_BLACK);
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
                SensorData data = globalState.getSensorData();
                static bool g2048_lastBtn[5] = {false, false, false, false, false};
                static bool g2048_lastJoyBtn = false;
                static unsigned long lastG2048Action = 0;
                static unsigned long lastAuto2048 = 0;
                unsigned long now2048 = millis();

                if (needsFullRedraw) {
                    g2048.reset();
                    g2048.setBestScore(bestScore2048);
                    g2048.pohyb = true; // Vynutíme první vykreslení po startu hry
                    lastG2048Action = now2048;
                }

                bool userAction = false;
                // Reset rekordu 2048 při současném stisku CENTER a JOYSTICKU:
                if (data.btnDown[BTN_DOWN_CENTER] && data.joyBtn) {
                    resetHighScore("2048", bestScore2048);
                    g2048.setBestScore(bestScore2048);
                    g2048.pohyb = true;
                } else if (data.joyBtn && !g2048_lastJoyBtn) {
                    // Stisk samotného joysticku restartuje hru
                    g2048.reset();
                    g2048.pohyb = true;
                }
                g2048_lastJoyBtn = data.joyBtn;

                // AI Autopilot na přepínači Switch 1 (krok každých 180 ms)
                if (data.switch1) {
                    if (now2048 - lastAuto2048 >= 180) {
                        lastAuto2048 = now2048;
                        g2048.autoStep();
                        g2048.pohyb = true;
                    }
                } else {
                    // Ovládání tlačítky (D-Pad / 5 tlačítek): UP, DOWN, LEFT, RIGHT, a CENTER (Reset)
                    if (data.btnDown[BTN_DOWN_UP] && !g2048_lastBtn[BTN_DOWN_UP])          { g2048.moveUp();    userAction = true; }
                    if (data.btnDown[BTN_DOWN_DOWN] && !g2048_lastBtn[BTN_DOWN_DOWN])      { g2048.moveDown();  userAction = true; }
                    if (data.btnDown[BTN_DOWN_LEFT] && !g2048_lastBtn[BTN_DOWN_LEFT])      { g2048.moveLeft();  userAction = true; }
                    if (data.btnDown[BTN_DOWN_RIGHT] && !g2048_lastBtn[BTN_DOWN_RIGHT])    { g2048.moveRight(); userAction = true; }
                    if (data.btnDown[BTN_DOWN_CENTER] && !g2048_lastBtn[BTN_DOWN_CENTER] && !data.joyBtn) { g2048.reset(); userAction = true; }

                    // Ovládání analogovým joystickem: Y < 1200 = Nahoru, Y > 2800 = Dolů, X < 1200 = Vlevo, X > 2800 = Vpravo
                    static bool joyMoved = false;
                    if (data.joyY < 1200 && !joyMoved)      { g2048.moveUp();    joyMoved = true; userAction = true; }
                    else if (data.joyY > 2800 && !joyMoved) { g2048.moveDown();  joyMoved = true; userAction = true; }
                    else if (data.joyX < 1200 && !joyMoved) { g2048.moveLeft();  joyMoved = true; userAction = true; }
                    else if (data.joyX > 2800 && !joyMoved) { g2048.moveRight(); joyMoved = true; userAction = true; }
                    else if (data.joyX >= 1200 && data.joyX <= 2800 && data.joyY >= 1200 && data.joyY <= 2800) {
                        joyMoved = false;
                    }

                    if (userAction) {
                        lastG2048Action = now2048;
                    }

                    for (int i = 0; i < 5; i++) g2048_lastBtn[i] = data.btnDown[i];
                }
                
                // Překreslí se POUZE když si hra vyžádá překreslení (tzn. změnil se stav)
                if (g2048.pohyb) {
                    if (!data.switch1) {
                        checkAndUpdateHighScore("2048", g2048.getScore(), bestScore2048);
                    }
                    g2048.setBestScore(bestScore2048);
                    g2048.draw();
                    g2048.pohyb = false; // Sníme flag, abychom nekreslili pořád
                }
                break;
            }

            // =======================================================
            // MÓD: HRA TETRIS (Následuje po 2048)
            // =======================================================
            case MODE_GAME_TETRIS: {
                delay = 20; // 50 Hz smyčka pro plynulé čtení tlačítek
                SensorData data = globalState.getSensorData();
                static bool tetris_lastBtn[5] = {false, false, false, false, false};
                static bool tetris_lastJoyBtn = false;
                static unsigned long casPoslednihoKroku = 0;
                static unsigned long lastMoveXTime = 0;
                static int lastDirX = 0;
                static unsigned long lastDownTime = 0;
                static bool wasDownHeld = false;
                static bool lastUpState = false;
                unsigned long aktualniCas = millis();

                if (needsFullRedraw) {
                    tetris.reset();
                    casPoslednihoKroku = aktualniCas;
                    lastMoveXTime = 0;
                    lastDirX = 0;
                    lastDownTime = 0;
                    wasDownHeld = false;
                    lastUpState = false;
                    tetris.draw();
                }

                // Reset rekordu při současném stisku tlačítka CENTER a JOYSTICKU
                if (data.btnDown[BTN_DOWN_CENTER] && data.joyBtn) {
                    resetHighScore("tetris", bestScoreTetris);
                    tetris.draw();
                } else if (data.joyBtn && !tetris_lastJoyBtn) {
                    // Pouze stisk joysticku samotného restartuje hru
                    tetris.reset();
                    casPoslednihoKroku = aktualniCas;
                    tetris.draw();
                }
                tetris_lastJoyBtn = data.joyBtn;

                bool userMove = false;

                // 1. Horizontální pohyb (Doleva / Doprava) - plynulé opakování při držení (DAS)
                int currentDirX = 0;
                if (data.joyX < 1200 || data.btnDown[BTN_DOWN_LEFT]) {
                    currentDirX = -1;
                } else if (data.joyX > 2800 || data.btnDown[BTN_DOWN_RIGHT]) {
                    currentDirX = 1;
                }

                if (currentDirX != 0) {
                    if (currentDirX != lastDirX) {
                        // Okamžitý první krok při vychýlení
                        if (currentDirX == -1) tetris.moveLeft();
                        else tetris.moveRight();
                        userMove = true;
                        lastDirX = currentDirX;
                        lastMoveXTime = aktualniCas + 170; // Úvodní zpoždění před opakováním
                    } else if (aktualniCas >= lastMoveXTime) {
                        // Plynulý posun během držení každých 75 ms bez nutnosti vracet joystick do nuly
                        if (currentDirX == -1) tetris.moveLeft();
                        else tetris.moveRight();
                        userMove = true;
                        lastMoveXTime = aktualniCas + 75;
                    }
                } else {
                    lastDirX = 0;
                }

                // 2. Vertikální pohyb dolů (Zrychlený pád / Soft drop) - plynulé opakování při držení
                bool isDown = (data.joyY > 2800 || data.btnDown[BTN_DOWN_DOWN]);
                if (isDown) {
                    if (!wasDownHeld) {
                        tetris.update();
                        userMove = true;
                        wasDownHeld = true;
                        casPoslednihoKroku = aktualniCas;
                        lastDownTime = aktualniCas + 140; // Prodleva před sérií pádu
                    } else if (aktualniCas >= lastDownTime) {
                        tetris.update();
                        userMove = true;
                        casPoslednihoKroku = aktualniCas;
                        lastDownTime = aktualniCas + 55;  // Plynulý pád každých 55 ms při držení
                    }
                } else {
                    wasDownHeld = false;
                }

                // 3. Rotace (Nahoru) - 1 otočení na jedno vychýlení/stisk
                bool isUp = (data.joyY < 1200 || data.btnDown[BTN_DOWN_UP]);
                if (isUp && !lastUpState) {
                    tetris.rotate();
                    userMove = true;
                }
                lastUpState = isUp;

                // 4. Hard drop (okamžitý dopad na středové tlačítko D-Padu)
                if (data.btnDown[BTN_DOWN_CENTER] && !tetris_lastBtn[BTN_DOWN_CENTER] && !data.joyBtn) {
                    tetris.boost();
                    userMove = true;
                }

                // Pokud hráč pohnul kostkou, hned překreslíme a aktualizujeme skóre
                if (userMove) {
                    if (!data.switch1) {
                        checkAndUpdateHighScore("tetris", tetris.getScore(), bestScoreTetris);
                    }
                    tetris.draw();
                }

                // 5. Automatický gravitační krok (každých 400 ms), pokud hráč nedrží pád dolů
                int krokDelay = 400;
                if (!isDown) {
                    if (data.switch1) {
                        // AI Autopilot na přepínači Switch 1 (krok každých 150 ms)
                        if (aktualniCas - casPoslednihoKroku >= 150) {
                            casPoslednihoKroku = aktualniCas;
                            tetris.autoStep();
                            tetris.draw();
                        }
                    } else if (aktualniCas - casPoslednihoKroku >= (unsigned long)krokDelay) {
                        casPoslednihoKroku = aktualniCas;
                        tetris.update();
                        checkAndUpdateHighScore("tetris", tetris.getScore(), bestScoreTetris);
                        tetris.draw();
                    }
                }

                for (int i = 0; i < 5; i++) tetris_lastBtn[i] = data.btnDown[i];
                break;
            }

            // =======================================================
            // MÓD: VZDÁLENOSTNÍ SENZORY A GRAFY (ULT, Laser, IR)
            // =======================================================
            case MODE_VZDALENOST: {
                delay = 50; // 20 fps
                SensorData data = globalState.getSensorData();
                
                int ultMm   = (int)(data.ultrasonicDistanceCm * 10.0f);
                int laserMm = (int)data.laserDistanceMm;
                int irMm    = (int)(data.irDistanceCm * 10.0f);

                static int lastUltMm   = -999;
                static int lastLaserMm = -999;
                static int lastIrMm    = -999;

                // Obsluha tlačítek pro přepínání aktivního senzoru a LED nad senzory:
                // Tlačítko 1 Vlevo (BTN_DOWN_LEFT)   -> Ultrazvuk LED (D4)
                // Tlačítko 2 Střed (BTN_DOWN_CENTER) -> Laser LED (D5)
                // Tlačítko 3 Vpravo (BTN_DOWN_RIGHT) -> IR LED (D1)
                if (data.btnDown[BTN_DOWN_LEFT]) {
                    globalState.updateLeds(true, false, false);
                } else if (data.btnDown[BTN_DOWN_CENTER]) {
                    globalState.updateLeds(false, false, true);
                } else if (data.btnDown[BTN_DOWN_RIGHT]) {
                    globalState.updateLeds(false, true, false);
                }

                static bool last_led_ult   = false;
                static bool last_led_laser = false;
                static bool last_led_ir    = false;

                // Historie hodnot pro 5sekundové grafy (25 vzorků po 200 ms = 5.0 s)
                static int histUlt[25]   = {0};
                static int histLaser[25] = {0};
                static int histIr[25]    = {0};
                static unsigned long lastGraphSampleTime = 0;
                static bool graphInitialized = false;

                // A) STATICKÁ MASKA
                if (needsFullRedraw) {
                    lastUltMm   = -999;
                    lastLaserMm = -999;
                    lastIrMm    = -999;

                    if (!graphInitialized) {
                        for (int i = 0; i < 25; i++) {
                            histUlt[i]   = constrain(ultMm, 0, 2000);
                            histLaser[i] = constrain(laserMm, 0, 1300);
                            histIr[i]    = constrain(irMm, 0, 300);
                        }
                        graphInitialized = true;
                    }

                    // 1. Bílé pozadí celého displeje
                    gfx.fillRect(0, 0, 320, 240, ST77XX_WHITE);
                    
                    // 2. Vycentrované nadpisy sloupců (Velikost 2)
                    gfx.drawTextPartial(35, 14, "ULT", ST77XX_BLACK, ST77XX_WHITE, 2);
                    gfx.drawTextPartial(130, 14, "Laser", ST77XX_BLACK, ST77XX_WHITE, 2);
                    gfx.drawTextPartial(254, 14, "IR", ST77XX_BLACK, ST77XX_WHITE, 2);

                    // 3. Větší ovládací tlačítka (Šedá kolečka R=17) - vycentrované ve spodní části horního panelu
                    // Col 1: Left (CX=32, CY=102)
                    gfx.fillCircle(32, 102, 17, 0xCE79);
                    gfx.drawCircle(32, 102, 17, ST77XX_BLACK);
                    gfx.drawTextPartial(20, 98, "Left", ST77XX_BLACK, 0xCE79, 1);

                    // Col 2: On (CX=139, CY=102)
                    gfx.fillCircle(139, 102, 17, 0xCE79);
                    gfx.drawCircle(139, 102, 17, ST77XX_BLACK);
                    gfx.drawTextPartial(133, 98, "On", ST77XX_BLACK, 0xCE79, 1);

                    // Col 3: Right (CX=245, CY=102)
                    gfx.fillCircle(245, 102, 17, 0xCE79);
                    gfx.drawCircle(245, 102, 17, ST77XX_BLACK);
                    gfx.drawTextPartial(230, 98, "Right", ST77XX_BLACK, 0xCE79, 1);

                    // 4. Předělové linky (Vodorovná čára posunutá dolů na Y=140 pro větší horní prostor)
                    gfx.drawRect(0, 0, 320, 240, ST77XX_BLACK);
                    gfx.drawLine(106, 0, 106, 239, ST77XX_BLACK);
                    gfx.drawLine(213, 0, 213, 239, ST77XX_BLACK);
                    gfx.drawLine(0, 140, 319, 140, ST77XX_BLACK);
                }

                // B) DYNAMICKÁ ČÁST - ČÍSLA (Vycentrováno)
                if (needsFullRedraw || abs(ultMm - lastUltMm) >= 3 || abs(laserMm - lastLaserMm) >= 3 || abs(irMm - lastIrMm) >= 3) {
                    lastUltMm   = ultMm;
                    lastLaserMm = laserMm;
                    lastIrMm    = irMm;

                    // Sloupec 1 (ULT mm)
                    String sUlt = String(ultMm) + " mm";
                    int wUlt = sUlt.length() * 12;
                    gfx.fillRect(8, 45, 90, 22, ST77XX_WHITE);
                    gfx.drawTextPartial(53 - wUlt / 2, 48, sUlt, ST77XX_BLACK, ST77XX_WHITE, 2);

                    // Sloupec 2 (Laser mm)
                    String sLaser = String(laserMm) + " mm";
                    int wLaser = sLaser.length() * 12;
                    gfx.fillRect(115, 45, 90, 22, ST77XX_WHITE);
                    gfx.drawTextPartial(160 - wLaser / 2, 48, sLaser, ST77XX_BLACK, ST77XX_WHITE, 2);

                    // Sloupec 3 (IR mm)
                    String sIr = String(irMm) + " mm";
                    int wIr = sIr.length() * 12;
                    gfx.fillRect(222, 45, 90, 22, ST77XX_WHITE);
                    gfx.drawTextPartial(266 - wIr / 2, 48, sIr, ST77XX_BLACK, ST77XX_WHITE, 2);
                }

                // C) DYNAMICKÁ ČÁST - INDIKAČNÍ LED KOLEČKA (Zvětšená R=13, Zelená = aktivní / Červená = neaktivní)
                if (needsFullRedraw || last_led_ult != data.led_ult || last_led_laser != data.led_laser || last_led_ir != data.led_ir) {
                    last_led_ult   = data.led_ult;
                    last_led_laser = data.led_laser;
                    last_led_ir    = data.led_ir;

                    // Col 1: ULT LED (CX=76, CY=102)
                    gfx.fillCircle(76, 102, 13, data.led_ult ? 0x07E0 : 0xF800);
                    gfx.drawCircle(76, 102, 13, ST77XX_BLACK);

                    // Col 2: Laser LED (CX=183, CY=102)
                    gfx.fillCircle(183, 102, 13, data.led_laser ? 0x07E0 : 0xF800);
                    gfx.drawCircle(183, 102, 13, ST77XX_BLACK);

                    // Col 3: IR LED (CX=289, CY=102)
                    gfx.fillCircle(289, 102, 13, data.led_ir ? 0x07E0 : 0xF800);
                    gfx.drawCircle(289, 102, 13, ST77XX_BLACK);
                }

                // D) DYNAMICKÁ ČÁST - 5SEKUNDOVÉ KONTINUÁLNÍ GRAFY (100% BEZ PROBLIKÁVÁNÍ)
                unsigned long now = millis();
                if (needsFullRedraw || (now - lastGraphSampleTime >= 200)) {
                    lastGraphSampleTime = now;

                    // Posun vzorků v poli historie
                    for (int i = 0; i < 24; i++) {
                        histUlt[i]   = histUlt[i + 1];
                        histLaser[i] = histLaser[i + 1];
                        histIr[i]    = histIr[i + 1];
                    }
                    histUlt[24]   = constrain(ultMm, 0, 2000);   // Max 2.0 m (2000 mm)
                    histLaser[24] = constrain(laserMm, 0, 1300); // Max 1.3 m (1300 mm)
                    histIr[24]    = constrain(irMm, 0, 300);     // Max 30 cm (300 mm)

                    // Vykreslení grafů (sloupec po sloupci) přímým jednoprůchodovým zápisem sloupců (Zero-flicker)
                    for (int col = 0; col < 3; col++) {
                        int xBase = (col == 0) ? 2 : ((col == 1) ? 108 : 215);
                        int* hData = (col == 0) ? histUlt : ((col == 1) ? histLaser : histIr);
                        int maxRange = (col == 0) ? 2000 : ((col == 1) ? 1300 : 300); // Specifický rozsah pro každý senzor
                        uint16_t fillColor = (col == 0) ? 0x9E7F : ((col == 1) ? 0xFFB0 : 0xFD34); // Modrá / Žlutá / Červená výplň
                        uint16_t lineColor = (col == 0) ? 0x01DF : ((col == 1) ? 0xD2A0 : 0xC800); // Tmavší křivka

                        for (int i = 0; i < 24; i++) {
                            int x1 = xBase + (i * 102) / 24;
                            int x2 = xBase + ((i + 1) * 102) / 24;
                            int y1 = 236 - map(hData[i], 0, maxRange, 0, 88);
                            int y2 = 236 - map(hData[i + 1], 0, maxRange, 0, 88);
                            y1 = constrain(y1, 148, 236);
                            y2 = constrain(y2, 148, 236);

                            for (int px = x1; px <= x2; px++) {
                                int py = y1 + (y2 - y1) * (px - x1) / (x2 - x1);
                                py = constrain(py, 148, 236);

                                // 1. Bílé pozadí nad křivkou (okamžitý přepis beze zbytku a bez blikání)
                                if (py > 142) {
                                    gfx.drawLine(px, 142, px, py - 1, ST77XX_WHITE);
                                }
                                // 2. Tmavá křivka
                                gfx.drawLine(px, py, px, min(py + 1, 236), lineColor);
                                // 3. Barevná výplň pod křivkou
                                if (py + 2 <= 237) {
                                    gfx.drawLine(px, py + 2, px, 237, fillColor);
                                }
                            }
                        }

                        // Postranní orientační značky vzdálenosti uvnitř grafu odpovídající reálným rozsahům
                        if (col == 0) {
                            gfx.drawTextPartial(xBase + 3, 144, "2m", 0x7BEF, ST77XX_WHITE, 1);
                            gfx.drawTextPartial(xBase + 3, 188, "1m", 0x7BEF, ST77XX_WHITE, 1);
                        } else if (col == 1) {
                            gfx.drawTextPartial(xBase + 3, 144, "1.3m", 0x7BEF, ST77XX_WHITE, 1);
                            gfx.drawTextPartial(xBase + 3, 188, "0.6m", 0x7BEF, ST77XX_WHITE, 1);
                        } else {
                            gfx.drawTextPartial(xBase + 3, 144, "30cm", 0x7BEF, ST77XX_WHITE, 1);
                            gfx.drawTextPartial(xBase + 3, 188, "15cm", 0x7BEF, ST77XX_WHITE, 1);
                        }

                        // Časové značky "0s" a "5s" přímo v grafu (šetří místo a neblikají)
                        gfx.drawTextPartial(xBase + 4, 227, "0s", ST77XX_BLACK, fillColor, 1);
                        gfx.drawTextPartial(xBase + 88, 227, "5s", ST77XX_BLACK, fillColor, 1);
                    }

                    // Obnova oddělovacích a ohraničujících linek
                    gfx.drawRect(0, 0, 320, 240, ST77XX_BLACK);
                    gfx.drawLine(106, 0, 106, 239, ST77XX_BLACK);
                    gfx.drawLine(213, 0, 213, 239, ST77XX_BLACK);
                    gfx.drawLine(0, 140, 319, 140, ST77XX_BLACK);
                }
                break;
            }

            // =======================================================
            // MÓD: WI-FI PŘIPOJENÍ & QR KÓD (Návrh Wi-Fi 1)
            // =======================================================
            case MODE_WIFI_SPOJENI: {
                delay = 100; // 10 fps
                SensorData data = globalState.getSensorData();
                uint8_t stations = data.wifiStationsCount;
                float estDist = data.wifiDistanceM;
                static uint8_t lastStations = 255;
                static int lastDistInt = -999;

                // A) STATICKÁ MASKA (Vykreslí se pouze 1x při vstupu do módu)
                if (needsFullRedraw) {
                    lastStations = 255;
                    lastDistInt = -999;

                    // 1. Levý panel (béžovo-oranžový 0xFED7, 0..105 px)
                    gfx.fillRect(0, 0, 106, 240, 0xFED7);

                    // 2. Pravý panel (čistě bílý, 106..319 px)
                    gfx.fillRect(106, 0, 214, 240, ST77XX_WHITE);

                    // 3. Rámečky a svislá předělová linka
                    gfx.drawRect(0, 0, 320, 240, ST77XX_BLACK);
                    gfx.drawLine(106, 0, 106, 239, ST77XX_BLACK);

                    // 4. Titulek "WIFI"
                    gfx.drawTextPartial(29, 16, "WIFI", ST77XX_BLACK, 0xFED7, 2);

                    // 5. Statické popisky
                    gfx.drawTextPartial(26, 110, "zarizeni:", ST77XX_BLACK, 0xFED7, 1);
                    gfx.drawTextPartial(16, 168, "VZDALENOST:", ST77XX_BLACK, 0xFED7, 1);

                    // 6. QR kód vycentrovaný na pravém panelu (29x29 modulů, scale=6 -> 174x174 px)
                    gfx.drawQRCode(126, 33, 6);
                }

                // B) DYNAMICKÁ ČÁST (Reálná data ze SystemState)
                int distInt = (int)(estDist * 10.0f);
                if (needsFullRedraw || stations != lastStations || distInt != lastDistInt) {
                    lastStations = stations;
                    lastDistInt = distInt;

                    uint16_t wifiColor = (stations > 0) ? 0x07E0 : 0xF800; // Zelená při připojení, červená při 0

                    // 1. Překreslení Wi-Fi ikony vlevo nahoře
                    gfx.fillRect(20, 46, 66, 48, 0xFED7); // Vyčištění prostoru ikony
                    gfx.drawWifiIcon(53, 65, wifiColor, 0xFED7);

                    // 2. Počet připojených zařízení
                    String sCount = String(stations);
                    int wCount = sCount.length() * 12;
                    gfx.fillRect(20, 124, 66, 22, 0xFED7);
                    gfx.drawTextPartial(53 - wCount / 2, 126, sCount, wifiColor, 0xFED7, 2);

                    // 3. Reálná vzdálenost z RSSI
                    String sDist = (stations > 0 && estDist > 0.1f) ? (String(estDist, 1) + "m") : "---";
                    int wDist = sDist.length() * 12;
                    gfx.fillRect(6, 184, 94, 22, 0xFED7);
                    gfx.drawTextPartial(53 - wDist / 2, 186, sDist, ST77XX_BLACK, 0xFED7, 2);

                    // Obnova oddělovací linky
                    gfx.drawLine(106, 0, 106, 239, ST77XX_BLACK);
                }
                break;
            }

            // =======================================================
            // MÓD: WI-FI CSI RADAR & DETEKCE POHYBU (Návrh Wi-Fi 2)
            // =======================================================
            case MODE_WIFI_DETECTION: {
                delay = 50; // 20 fps
                SensorData data = globalState.getSensorData();

                static float histCsi[60]; // 60 vzorků (1 vzorek za sekundu = 1 minuta historie)
                static unsigned long lastCsiSampleTime = 0;
                static bool csiGraphInit = false;
                static int lastStateBox = -1; // 0 = kalibrace, 1 = klid, 2 = pohyb, 3 = odpojeno
                static bool lastHasClient = false;

                bool hasClient = (data.wifiStationsCount > 0);

                // A) STATICKÁ MASKA
                if (needsFullRedraw || hasClient != lastHasClient) {
                    lastHasClient = hasClient;
                    lastStateBox = -1;
                    if (!csiGraphInit) {
                        for (int i = 0; i < 60; i++) histCsi[i] = 1.0f;
                        csiGraphInit = true;
                    }

                    // 1. Bílé pozadí
                    gfx.fillRect(0, 0, 320, 240, ST77XX_WHITE);

                    // 2. Levá horní část: Titulek
                    gfx.drawTextPartial(16, 38, "WIFI -->", ST77XX_BLACK, ST77XX_WHITE, 2);
                    gfx.drawTextPartial(16, 62, "Detection", ST77XX_BLACK, ST77XX_WHITE, 2);

                    // 3. Rámečky a dělící linky
                    gfx.drawRect(0, 0, 320, 240, ST77XX_BLACK);
                    gfx.drawLine(140, 0, 140, 120, ST77XX_BLACK);
                    gfx.drawLine(0, 120, 319, 120, ST77XX_BLACK);

                    // Pokud NENÍ připojen žádný mobil, zobrazíme velkou informativní obrazovku
                    if (!hasClient) {
                        gfx.fillRect(1, 121, 318, 118, 0xF7BE); // Jemné šedobílé pozadí
                        gfx.drawTextPartial(22, 145, "CEKAM NA PRIPOJENI...", 0xF800, 0xF7BE, 2);
                        gfx.drawTextPartial(18, 178, "CSI radar potrebuje telefon pro odraz vln.", ST77XX_BLACK, 0xF7BE, 1);
                        gfx.drawTextPartial(18, 198, "1. Pripoj se k Wi-Fi: ESP-Demo-Box", 0x001F, 0xF7BE, 1);
                        gfx.drawTextPartial(18, 216, "2. Nebo naskenuj QR kod v modu Wi-Fi QR.", ST77XX_DARKGREY, 0xF7BE, 1);
                        gfx.drawRect(0, 0, 320, 240, ST77XX_BLACK);
                        gfx.drawLine(0, 120, 319, 120, ST77XX_BLACK);
                    }
                }

                // B) DYNAMICKÁ ČÁST 1 - HORNÍ VÝSTRAŽNÝ / STAVOVÝ BOX (Vpravo nahoře)
                int currentStateBox;
                if (!hasClient) {
                    currentStateBox = 3; // ODPOJENO
                } else if (data.wifiCsiCalibrating) {
                    currentStateBox = 0; // KALIBRACE
                } else if (data.wifiMotionDetected) {
                    currentStateBox = 2; // POHYB
                } else {
                    currentStateBox = 1; // KLID
                }

                if (needsFullRedraw || currentStateBox != lastStateBox) {
                    lastStateBox = currentStateBox;

                    if (currentStateBox == 3) { // ODPOJENO (Žádný mobil)
                        gfx.fillRect(141, 1, 178, 118, 0xFED7); // Světle oranžová
                        gfx.drawTextPartial(158, 38, "ZADNY MOBIL", 0xF800, 0xFED7, 2);
                        gfx.drawTextPartial(170, 68, "Odpojeno", ST77XX_BLACK, 0xFED7, 2);
                    } else if (currentStateBox == 0) { // KALIBRACE
                        gfx.fillRect(141, 1, 178, 118, 0xFED7); // Světle oranžová/žlutá
                        gfx.drawTextPartial(168, 38, "KALIBRACE...", ST77XX_BLACK, 0xFED7, 2);
                        String sLeft = "Zbyva: " + String(data.wifiCsiCalibSecLeft) + "s";
                        gfx.drawTextPartial(180, 68, sLeft, ST77XX_BLACK, 0xFED7, 2);
                    } else if (currentStateBox == 2) { // POHYB DETEKOVÁN! (Červený box + ! ! !)
                        gfx.fillRect(141, 1, 178, 118, 0xFCD3); // Lososová/růžovo-červená
                        gfx.drawTextPartial(175, 42, "!  !  !", 0xF800, 0xFCD3, 4); // Velké červené vykřičníky
                    } else { // KLID / OK
                        gfx.fillRect(141, 1, 178, 118, 0xCE79); // Světle zelená
                        gfx.drawTextPartial(180, 50, "[ KLID ]", 0x03E0, 0xCE79, 3);
                    }
                    gfx.drawLine(140, 0, 140, 120, ST77XX_BLACK);
                    gfx.drawLine(0, 120, 319, 120, ST77XX_BLACK);
                }

                // C) DYNAMICKÁ ČÁST 2 - 60SEKUNDOVÝ DVOJBAREVNÝ GRAF (Prahová detekce - pouze při připojeném telefonu)
                if (hasClient) {
                    unsigned long now = millis();
                    if (needsFullRedraw || (now - lastCsiSampleTime >= 1000)) { // 1 vzorek za sekundu = 60s graf
                        lastCsiSampleTime = now;

                        // Posun historie
                        for (int i = 0; i < 59; i++) {
                            histCsi[i] = histCsi[i + 1];
                        }
                        histCsi[59] = data.wifiMotionMetric;

                        float thresh = (data.wifiThreshold > 0.5f) ? data.wifiThreshold : 2.5f;
                        int yThresh = 236 - (int)(constrain(thresh, 0.0f, 8.0f) * 11.0f);
                        yThresh = constrain(yThresh, 128, 234);

                        int xBase = 2;
                        int totalW = 316;

                        for (int i = 0; i < 59; i++) {
                            int x1 = xBase + (i * totalW) / 59;
                            int x2 = xBase + ((i + 1) * totalW) / 59;

                            int y1 = 236 - (int)(constrain(histCsi[i], 0.0f, 8.0f) * 11.0f);
                            int y2 = 236 - (int)(constrain(histCsi[i + 1], 0.0f, 8.0f) * 11.0f);
                            y1 = constrain(y1, 126, 236);
                            y2 = constrain(y2, 126, 236);

                            for (int px = x1; px <= x2; px++) {
                                int py = y1 + (y2 - y1) * (px - x1) / (x2 - x1);
                                py = constrain(py, 126, 236);

                                // 1. Bílé pozadí nad křivkou
                                if (py > 122) {
                                    gfx.drawLine(px, 122, px, py - 1, ST77XX_WHITE);
                                }
                                // 2. Tmavá křivka
                                gfx.drawLine(px, py, px, min(py + 1, 236), ST77XX_BLACK);

                                // 3. Výplň pod křivkou:
                                // Nad prahem (py < yThresh) -> Červená výplň (POHYB)
                                // Pod prahem (py >= yThresh) -> Zelená výplň (KLID)
                                if (py + 2 <= yThresh) {
                                    gfx.drawLine(px, py + 2, px, yThresh, 0xFCD3); // Červená horní zóna poplachu
                                    if (yThresh + 1 <= 237) {
                                        gfx.drawLine(px, yThresh + 1, px, 237, 0xCE79); // Zelená spodní klidová zóna
                                    }
                                } else if (py + 2 <= 237) {
                                    gfx.drawLine(px, py + 2, px, 237, 0xCE79); // Zelená klidová zóna
                                }
                            }
                        }

                        // Prahová horizontální čára přes graf
                        for (int px = 4; px < 316; px += 4) {
                            gfx.drawLine(px, yThresh, px + 2, yThresh, 0x7BEF); // Šedá čárkovaná linie prahu
                        }

                        // Časové značky 60s, 30s, 0s
                        gfx.drawTextPartial(6, 227, "60s", ST77XX_BLACK, 0xCE79, 1);
                        gfx.drawTextPartial(150, 227, "30s", ST77XX_BLACK, 0xCE79, 1);
                        gfx.drawTextPartial(296, 227, "0s", ST77XX_BLACK, 0xCE79, 1);

                        // Obnova oddělovacích linek
                        gfx.drawRect(0, 0, 320, 240, ST77XX_BLACK);
                        gfx.drawLine(0, 120, 319, 120, ST77XX_BLACK);
                    }
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
                bool boost = false;
                SensorData data = globalState.getSensorData();
                static bool snake_lastBtn[5] = {false, false, false, false, false};
                static unsigned long lastSnakeAction = 0;
                static unsigned long inputHoldStartTime = 0;

                if (!data.switch1) {

                    bool snakeUserMove = false;
                    // Ovládání směru hada tlačítky (D-Pad / 5 tlačítek): UP, DOWN, LEFT, RIGHT
                    if (data.btnDown[BTN_DOWN_UP] && !snake_lastBtn[BTN_DOWN_UP])          { snake.goUp();    snakeUserMove = true; }
                    if (data.btnDown[BTN_DOWN_DOWN] && !snake_lastBtn[BTN_DOWN_DOWN])      { snake.goDown();  snakeUserMove = true; }
                    if (data.btnDown[BTN_DOWN_LEFT] && !snake_lastBtn[BTN_DOWN_LEFT])      { snake.goLeft();  snakeUserMove = true; }
                    if (data.btnDown[BTN_DOWN_RIGHT] && !snake_lastBtn[BTN_DOWN_RIGHT])    { snake.goRight(); snakeUserMove = true; }

                    // Ovládání joystickem: Y < 1200 = Nahoru, Y > 2800 = Dolů, X < 1200 = Vlevo, X > 2800 = Vpravo
                    if (data.joyY < 1200)      { snake.goUp();    snakeUserMove = true; }
                    else if (data.joyY > 2800) { snake.goDown();  snakeUserMove = true; }
                    if (data.joyX < 1200)      { snake.goLeft();  snakeUserMove = true; }
                    else if (data.joyX > 2800) { snake.goRight(); snakeUserMove = true; }

                    // Detekce aktivního držení směru pro BOOST (tlačítka i joystick):
                    bool anyInputHeld = (data.btnDown[BTN_DOWN_UP] || data.btnDown[BTN_DOWN_DOWN] ||
                                        data.btnDown[BTN_DOWN_LEFT] || data.btnDown[BTN_DOWN_RIGHT] ||
                                        data.joyX < 1200 || data.joyX > 2800 ||
                                        data.joyY < 1200 || data.joyY > 2800);

                    if (anyInputHeld) {
                        if (inputHoldStartTime == 0) {
                            inputHoldStartTime = aktualniCas; // Zaznamenáme čas začátku držení
                        } else if (aktualniCas - inputHoldStartTime >= 300) {
                            boost = true; // Držíme směr déle než 300 ms -> aktivujeme Boost!
                        }
                    } else {
                        inputHoldStartTime = 0; // Uvolněno -> reset časovače Boostu
                    }

                    if (snakeUserMove) {
                        lastSnakeAction = aktualniCas;
                    }
                }
                // Restart hry po Game Over stiskem středového tlačítka (CENTER / BTN2) nebo joysticku
                // Reset rekordu při současném stisku CENTER a JOYSTICKU:
                if (data.btnDown[BTN_DOWN_CENTER] && data.joyBtn) {
                    resetHighScore("snake", bestScoreSnake);
                } else if ((data.btnDown[BTN_DOWN_CENTER] && !snake_lastBtn[BTN_DOWN_CENTER]) || data.joyBtn) {
                    lastSnakeAction = aktualniCas;
                    if (snake.isGameOver()) {
                        snake.reset();
                    }
                }
                for (int i = 0; i < 5; i++) snake_lastBtn[i] = data.btnDown[i];


                if (aktualniCas - casPoslednihoKroku >= (boost ? 80 : 150)) {
                    if(data.switch1){
                        snake.autoStep(); // Autonomní mód
                    }
                    casPoslednihoKroku = aktualniCas;
                    snake.update();
                    if(!data.switch1){
                        checkAndUpdateHighScore("snake", snake.getScore(), bestScoreSnake);
                    }
                    snake.draw();
                }
                break;
            }
            
            // =======================================================
            case MODE_GAME_FLAPPY: {
                if (needsFullRedraw) {
                    flappy.reset();
                    flappy.setBestScore(bestScoreFlappy);
                    flappy.draw();
                }
                unsigned long aktualniCasFlappy = millis();
                delay = 10;
                SensorData data = globalState.getSensorData();
                static bool flappy_lastBtn = false;
                static unsigned long lastFlappyAction = 0;

                // Skok nebo restart hry při stisku kteréhokoliv tlačítka D-Padu, joysticku nebo horního tlačítka
                if (data.btnDown[BTN_DOWN_CENTER] && data.joyBtn) {
                    resetHighScore("flappy", bestScoreFlappy);
                    flappy.setBestScore(bestScoreFlappy);
                    flappy.draw();
                }
                bool jumpPress = (data.btnDown[BTN_DOWN_UP] || data.btnDown[BTN_DOWN_CENTER] ||
                                  data.btnDown[BTN_DOWN_LEFT] || data.btnDown[BTN_DOWN_RIGHT] ||
                                  data.btnDown[BTN_DOWN_DOWN] || data.joyBtn || data.btnTop);
                if (jumpPress && !flappy_lastBtn) {
                    lastFlappyAction = aktualniCasFlappy;
                    if (flappy.getGameOver()) {
                        flappy.reset();
                        flappy.setBestScore(bestScoreFlappy);
                    } else {
                        flappy.jump();
                    }
                }
                flappy_lastBtn = jumpPress;

                if (aktualniCasFlappy - casPoslednihoKroku >= 40) {
                    casPoslednihoKroku = aktualniCasFlappy;
                    if (data.switch1) {
                        flappy.autoStep(); // AI Autopilot na Switch 1
                    }
                    flappy.update();
                    if (!data.switch1) {
                        checkAndUpdateHighScore("flappy", flappy.getScore(), bestScoreFlappy);
                    }
                    flappy.setBestScore(bestScoreFlappy);
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

    loadHighScores();

    snake.reset();
    flappy.reset();

    // Vytváření úloh (Tasks) pro FreeRTOS.
    // Argumenty: Funkce, Název pro debug, Velikost paměti (Stack), Parametry, Priorita, Zvláštní Handle, ID Jádra

    xTaskCreatePinnedToCore(Task_WiFi_Web, "WiFi_Web", 8192, NULL, 1, NULL, 0); // Core 0
    // xTaskCreatePinnedToCore(Task_Demo_Autopilot, "Demo_Autopilot", 4096, NULL, 1, NULL, 0); // Vypnuto: pouze manuální přepínání
    
#ifdef ENABLE_UART_ESP
    xTaskCreatePinnedToCore(Task_UART, "UART_Top", 4096, NULL, 2, NULL, 0); // Core 0 (Reálná UART komunikace se spodním panelem)
#endif
    
    xTaskCreatePinnedToCore(Task_Display_UI, "Display_UI", 8192, NULL, 1, NULL, 1); // Core 1
    xTaskCreatePinnedToCore(Task_Sensors, "Sensors", 4096, NULL, 2, NULL, 1); // Core 1 (Priorita 2 pro presne casovani vzorkovani IMU)

    Serial.println("Vsechna vlakna spustena!");
}

// ---------------------------------------------------------
// Smyčka loop()
// ---------------------------------------------------------
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
