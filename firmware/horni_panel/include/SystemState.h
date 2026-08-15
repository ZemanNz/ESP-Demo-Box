#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h> // Knihovna pro Mutexy a Semafory

// ---------------------------------------------------------
// 1. Definice všech možných stavů aplikace (Stavový automat)
// ---------------------------------------------------------
enum AppMode {
    MODE_MAIN_MENU,
    MODE_GAME_SNAKE,
    MODE_MOTOR_TEST,
    MODE_SENSORS
};

// ---------------------------------------------------------
// 2. Struktura držící aktuální data senzorů
// ---------------------------------------------------------
struct SensorData {
    float temperature;
    float humidity;
    uint16_t laserDistanceMm;
    uint8_t colorR, colorG, colorB;
    // Sem postupně přidáš další senzory...
};

// ---------------------------------------------------------
// 3. Hlavní sdílená třída (Globální Stav)
// ---------------------------------------------------------
class SystemState {
private:
    // Tyto proměnné jsou soukromé, aby je nikdo nemohl přepsat bez použití Mutexu
    AppMode currentMode;
    int menuCursorIndex;
    SensorData sensors;
    
    // Vlajka pro displej (true = obrazovka potřebuje překreslit)
    bool uiNeedsUpdate;

    // Mutex (zámek), který zabraňuje kolizi jader
    SemaphoreHandle_t stateMutex;

public:
    // Konstruktor - inicializuje výchozí hodnoty a vytvoří Mutex
    SystemState() {
        currentMode = MODE_MAIN_MENU;
        menuCursorIndex = 0;
        uiNeedsUpdate = true; // Hned po startu chceme vykreslit menu
        
        // Vynulujeme senzory
        sensors = {0.0f, 0.0f, 0, 0, 0, 0};

        // Vytvoření FreeRTOS Mutexu
        stateMutex = xSemaphoreCreateMutex();
    }

    // -----------------------------------------------------
    // Gettery (Čtení dat z jiných vláken)
    // -----------------------------------------------------
    
    // Příklad čtení aktuálního módu (bezpečně přes Mutex)
    AppMode getMode() {
        AppMode mode = MODE_MAIN_MENU; // Default fallback
        // Zkusíme zamknout Mutex (čekáme max 10 ticků)
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            mode = currentMode;
            xSemaphoreGive(stateMutex); // Odemkneme
        }
        return mode;
    }

    bool popUiNeedsUpdate() {
        bool needsUpdate = false;
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            needsUpdate = uiNeedsUpdate;
            uiNeedsUpdate = false; // "Sníme" vlajku (displej bude vědět, že to překreslil)
            xSemaphoreGive(stateMutex);
        }
        return needsUpdate;
    }

    SensorData getSensorData() {
        SensorData dataCopy = {0};
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            dataCopy = sensors; // Bezpečně zkopírujeme strukturu
            xSemaphoreGive(stateMutex);
        }
        return dataCopy;
    }

    // -----------------------------------------------------
    // Settery (Zápis dat z jiných vláken - např. z UARTu)
    // -----------------------------------------------------

    // Změna módu aplikace (např. když zmáčkneš červené tlačítko ESC)
    void setMode(AppMode newMode) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            currentMode = newMode;
            uiNeedsUpdate = true; // Změnil se mód -> displej se musí překreslit
            xSemaphoreGive(stateMutex);
        }
    }

    // Zápis naměřené teploty (volá Sensor Task na Core 1)
    void updateTemperature(float temp, float hum) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            sensors.temperature = temp;
            sensors.humidity = hum;
            // Zde nenastavujeme uiNeedsUpdate = true hned (překreslovalo by se to neustále)
            // Displej si data stáhne sám periodicky, pokud je zrovna v MODE_SENSORS
            xSemaphoreGive(stateMutex);
        }
    }

    // Simulace otočení enkodéru
    void moveMenuCursor(int step) {
        if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
            menuCursorIndex += step;
            // Zde bys přidal logiku např. `if (menuCursorIndex > MAX) menuCursorIndex = 0;`
            uiNeedsUpdate = true; // Kurzory vyžadují překreslení UI
            xSemaphoreGive(stateMutex);
        }
    }
};

// Zde deklarujeme, že globalState existuje (fyzicky ho vytvoříme v main.cpp)
extern SystemState globalState;

#endif // SYSTEM_STATE_H
