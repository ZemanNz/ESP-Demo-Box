#include <Arduino.h>
#include "SystemState.h"
#include "HardwareSetup.h"
#include "GraphicsManager.h"
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

    // Zde bys dal WiFi.begin(), AsyncWebServer.begin() atd.

    for (;;) {
        // Smyčka vlákna. Jelikož AsyncWebServer jede na pozadí, 
        // tady můžeme řešit např. Websockety nebo udržování spojení
        vTaskDelay(pdMS_TO_TICKS(1000)); // Čekej 1 vteřinu bez blokování CPU
    }
}

// ---------------------------------------------------------
// 2. Task: Demo Autopilot (Simuluje mačkání tlačítek)
// ---------------------------------------------------------
void Task_UART_Simulator(void *pvParameters) {
    Serial.print("Demo Autopilot bezi na uvazku (Core): ");
    Serial.println(xPortGetCoreID());

    int timeInMode = 0;

    for (;;) {
        // Zkontrolujeme situaci každých 100 ms
        vTaskDelay(pdMS_TO_TICKS(100)); 
        timeInMode += 100;
        
        AppMode current = globalState.getMode();
        
        // --- AUTOPILOT PRO HRY ---
        if (current == MODE_GAME_FLAPPY) {
            // Každých 500 ms ptáček poskočí, aby nespadl
            if (timeInMode % 500 == 0) flappy.jump();
        } 
        else if (current == MODE_GAME_SNAKE) {
            // Každých 800 ms zkusí had náhodně zatočit
            if (timeInMode % 800 == 0) {
                int r = random(0, 4);
                if (r == 0) snake.goUp();
                else if (r == 1) snake.goDown();
                else if (r == 2) snake.goLeft();
                else snake.goRight();
            }
        }
        else if (current == MODE_2048) {
            // Každých 1000 ms zkusí 2048 udělat tah, pokud není konec hry
            if (timeInMode % 1000 == 0) {
                int r = random(0, 4);
                if (r == 0) g2048.moveUp();
                else if (r == 1) g2048.moveDown();
                else if (r == 2) g2048.moveLeft();
                else g2048.moveRight();
            }
        }

        // --- PŘEPÍNÁNÍ MÓDŮ ---
        // Každých 6 sekund přepneme na další obrazovku
        if (timeInMode >= 6000) {
            timeInMode = 0;
            
            int nextMode = (int)current + 1;
            if (nextMode > MODE_BAREVNY) {
                nextMode = MODE_MAIN_MENU; // Návrat na začátek
            }
            
         ;
            
            Serial.printf("[DEMO] Prepinam na mod: %d\n", nextMode);
            globalState.setMode((AppMode)nextMode);
        }
    }
}

// ---------------------------------------------------------
// 3. Task: Senzory (Poběží na Core 1)
// ---------------------------------------------------------
void Task_Sensors(void *pvParameters) {
    Serial.print("Task_Sensors bezi na uvazku (Core): ");
    Serial.println(xPortGetCoreID());

    // Zde bys udělal dht.begin(), Wire.begin() atd.

    for (;;) {
        // Fiktivní čtení teploměru
        float simulatedTemp = 24.5f + (random(-10, 10) / 10.0f);
        globalState.updateTemperature(simulatedTemp, 45.0f);

        vTaskDelay(pdMS_TO_TICKS(2000)); // Pomalé senzory čteme např. co 2 vteřiny
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
