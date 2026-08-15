#include <Arduino.h>
#include "SystemState.h"
#include "HardwareSetup.h"

// ---------------------------------------------------------
// Fyzická instance Globálního Stavu (paměť)
// ---------------------------------------------------------
SystemState globalState;

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
// 2. Task: Falešný UART Simulátor (Poběží na Core 0)
// ---------------------------------------------------------
void Task_UART_Simulator(void *pvParameters) {
    Serial.print("Task_UART_Simulator bezi na uvazku (Core): ");
    Serial.println(xPortGetCoreID());

    for (;;) {
        // Simulujeme, že každých 5 sekund přijde ze spodního panelu povel
        // k otočení enkodéru.
        vTaskDelay(pdMS_TO_TICKS(5000)); 

        Serial.println("[UART MOCK] Prisel povel: ENCODER_RIGHT");
        
        // Zápis do sdíleného stavu (Mutex nás ochrání)
        globalState.moveMenuCursor(1);
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

    // Zde bys udělal tft.init(), tft.fillScreen() atd.

    for (;;) {
        // Zkontrolujeme, jestli si někdo nevyžádal překreslení UI
        if (globalState.popUiNeedsUpdate()) {
            
            AppMode currentMode = globalState.getMode();
            
            Serial.println("================================");
            Serial.println("PREKRESLUJI DISPLEJ!");
            
            switch (currentMode) {
                case MODE_MAIN_MENU:
                    Serial.println("Kreslim grafiku: HLAVNI MENU");
                    // Zde zavoláš funkci drawMainMenu()
                    break;
                case MODE_SENSORS:
                    Serial.println("Kreslim grafiku: SENZORY");
                    // Zde zavoláš funkci drawSensorsDashboard()
                    break;
                //... atd
            }
            Serial.println("================================");
        }

        // Tady můžeme dát kratičký delay, např. 50 ms (kreslíme max 20 fps)
        vTaskDelay(pdMS_TO_TICKS(50));
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
