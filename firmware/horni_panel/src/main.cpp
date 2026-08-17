#include <Arduino.h>
#include "SystemState.h"
#include "HardwareSetup.h"
#include "GraphicsManager.h"
#include "SensorManager.h"
#include "UartProtocol.h"
#include "hry/snake.h"
#include "hry/flappy_bird.h"
#include "hry/game_2048.h"

// ---------------------------------------------------------
// Fyzická instance Globálního Stavu (paměť)
// ---------------------------------------------------------
SystemState globalState;
SnakeGame snake;
FlappyGame flappy;
Game2048 g2048;

// ---------------------------------------------------------
// 1. Task: Wi-Fi a WebServer (Poběží na Core 0)
// ---------------------------------------------------------
void Task_WiFi_Web(void *pvParameters) {
    Serial.print("Task_WiFi_Web bezi na uvazku (Core): ");
    Serial.println(xPortGetCoreID());

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
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
        if (timeInMode >= 6000) {
            timeInMode = 0;
            int nextMode = (int)current + 1;
            if (nextMode > MODE_BAREVNY) {
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
// 3. Task: Senzory (Poběží na Core 1)
// ---------------------------------------------------------
void Task_Sensors(void *pvParameters) {
    Serial.print("Task_Sensors bezi na uvazku (Core): ");
    Serial.println(xPortGetCoreID());

    for (;;) {
        // Čtení všech aktivních senzorů a zápis do globalState s vlastním časováním
        sensorManager.updateAll();

        vTaskDelay(pdMS_TO_TICKS(10)); // Perioda 10 ms (rychlé vstupy 50 Hz, pomalé se filtrují)
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

        // 3. Vykreslujeme podle módu
        switch (currentMode) {
            
            // =======================================================
            case MODE_MAIN_MENU:

                delay = 50; // Defaultní zpoždění mezi překresleními (20 fps)
                if (needsFullRedraw) {
                    // TADY TVOŘÍŠ DESIGN HLAVNÍHO MENU
                    gfx.clearScreen(ST77XX_BLACK);
                    gfx.drawTextPartial(50, 20, "HLAVNI MENU", ST77XX_WHITE, ST77XX_BLACK, 2);
                    
                    gfx.fillRoundRect(20, 60, 200, 40, 5, ST77XX_BLUE);
                    gfx.drawTextPartial(40, 70, "1. Senzory", ST77XX_WHITE, ST77XX_BLUE, 2);
                    
                    gfx.fillRoundRect(20, 110, 200, 40, 5, ST77XX_DARKGREY);
                    gfx.drawTextPartial(40, 120, "2. Hra Had", ST77XX_WHITE, ST77XX_DARKGREY, 2);
                }
                // Dál se nic neděje, procesor může odpočívat
                break;

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
            case MODE_2048:
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

            case MODE_VZDALENOST:
                delay = 50;
                if (needsFullRedraw) {
                    gfx.clearScreen(ST77XX_BLACK);
                    gfx.drawTextPartial(70, 100, "VZDALENOST", ST77XX_WHITE, ST77XX_BLACK, 3);
                }
                break;

            case MODE_WIFI_SPOJENI:
                delay = 50;
                if (needsFullRedraw) {
                    gfx.clearScreen(ST77XX_BLACK);
                    gfx.drawTextPartial(52, 100, "WIFI SPOJENI", ST77XX_WHITE, ST77XX_BLACK, 3);
                }
                break;

            case MODE_SERVA:
                delay = 50;
                if (needsFullRedraw) {
                    gfx.clearScreen(ST77XX_BLACK);
                    gfx.drawTextPartial(115, 100, "SERVA", ST77XX_WHITE, ST77XX_BLACK, 3);
                }
                break;

            case MODE_MOTOR:
                delay = 50;
                if (needsFullRedraw) {
                    gfx.clearScreen(ST77XX_BLACK);
                    gfx.drawTextPartial(115, 100, "MOTOR", ST77XX_WHITE, ST77XX_BLACK, 3);
                }
                break;

            case MODE_BAREVNY:
                delay = 50;
                if (needsFullRedraw) {
                    gfx.clearScreen(ST77XX_BLACK);
                    gfx.drawTextPartial(34, 100, "BAREVNY SENZOR", ST77XX_WHITE, ST77XX_BLACK, 3);
                }
                break;
            
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

    // Zde se zavolá obrovský setup všech modulů
    if (!initializeAllHardware()) {
        Serial.println("SYSTEM ZASTAVEN KVULI CHYBE HARDWARU!");
        while (true) {
            delay(1000); // Zablokujeme start FreeRTOS, pokud HW selhal
        }
    }

    snake.reset();
    flappy.reset();

    // Vytváření úloh (Tasks) pro FreeRTOS.
    // Argumenty: Funkce, Název pro debug, Velikost paměti (Stack), Parametry, Priorita, Zvláštní Handle, ID Jádra

    xTaskCreatePinnedToCore(Task_WiFi_Web, "WiFi_Web", 4096, NULL, 1, NULL, 0); // Core 0
    xTaskCreatePinnedToCore(Task_UART_Simulator, "UART_Mock", 2048, NULL, 2, NULL, 0); // Core 0 (priorita 2 = vyšší než web)
    
    xTaskCreatePinnedToCore(Task_Display_UI, "Display_UI", 4096, NULL, 1, NULL, 1); // Core 1
    xTaskCreatePinnedToCore(Task_Sensors, "Sensors", 2048, NULL, 1, NULL, 1); // Core 1

    Serial.println("Vsechna vlakna spustena, mazu hlavni smycku (loop)");
}

// ---------------------------------------------------------
// Smyčku loop() už nepotřebujeme, vše řídí FreeRTOS Tasks
// ---------------------------------------------------------
void loop() {
    vTaskDelete(NULL); // Smaže tento původní Arduino loop task a uvolní paměť
}
