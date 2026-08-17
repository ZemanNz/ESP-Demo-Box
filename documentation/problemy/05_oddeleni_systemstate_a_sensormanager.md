# Problém: Architektonické oddělení datové a hardwarové vrstvy (SystemState vs. SensorManager)

## Popis problému
Při vývoji komplexního vestavěného systému s více než 15 senzory a akčními členy hrozí vznik tzv. **„Božské třídy“ (God Object Antipattern)**. 

Původní návrh směřoval k tomu, že třída `SystemState` měla kromě správy sdílené paměti a FreeRTOS mutexů provádět i samotné čtení hardwarových pinů, inicializaci I2C sběrnic a generování tónů bzučáku.

### Rizika spojení dat a hardware do jedné třídy:
1. **Porušení principu jedné odpovědnosti (Single Responsibility Principle):** Třída má více důvodů ke změně – změna komunikačního protokolu i výměna fyzického pinu by vyžadovala zásah do stejného souboru.
2. **Nebezpečné zamykání mutexů:** Pokud by funkce čtoucí pomalý senzor (např. DHT11 nebo ultrazvuk `pulseIn()`, který trvá až 30 ms) držela zámek mutexu, došlo by k okamžitému zablokování vykreslování displeje i webového serveru.
3. **Obtížná testovatelnost a simulace:** Bez oddělení hardware nelze snadno spustit demo simulátor her bez fyzicky připojených čidel.

---

## Řešení: Dvojvrstvá architektura (State Layer vs. Hardware Layer)

Systém byl striktně rozdělen do dvou nezávislých vrstev:

```text
+-------------------------------------------------------------+
|                      APLIKAČNÍ VRSTVA                       |
|          (Hry, Grafické UI, Stavový automat, Web)           |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                 1. DATOVÁ VRSTVA (SystemState)              |
|  - Čistý datový model (SensorData)                          |
|  - Thread-Safe přístup pomocí FreeRTOS Mutexu               |
|  - ŽÁDNÝ přímý přístup k fyzickým pinům a registrům         |
|  - Bleskové atomické operace (trvání < 0,05 µs)             |
+-------------------------------------------------------------+
                              ^
                              | (aktualizuje data)
+-------------------------------------------------------------+
|              2. HARDWAROVÁ VRSTVA (SensorManager)           |
|  - Přímé ovládání GPIO, I2C sběrnic, časovačů a SPI         |
|  - Fyzické čtení: DHT, VL53L0X, HC-SR04, TCS34725, LSM6DS3  |
|  - Fyzické výstupy: LED diody, bzučák, 74HC595, WS2812B     |
|  - Automatická aktualizace stavu v SystemState po akci      |
+-------------------------------------------------------------+
```

---

## Rozdělení rolí v kódu

### 1. `SystemState` (Správce dat a synchronizace)
Spravuje pouze paměťovou strukturu `SensorData` a zajišťuje, že čtení a zápis mezi jádry Core 0 a Core 1 je bezpečný:
```cpp
void updateTemperature(float temp, float hum) {
    if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
        sensors.temperature = temp;
        sensors.humidity = hum;
        xSemaphoreGive(stateMutex);
    }
}
```

### 2. `SensorManager` (Fyzický ovladač)
Zajišťuje komunikaci s fyzickým hardwarem a okamžitě zapisuje výsledek do `globalState`:
```cpp
void SensorManager::readDHT() {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
        globalState.updateTemperature(t, h); // Zápis do datové vrstvy
    }
}

void SensorManager::setLed1(bool on) {
    digitalWrite(PIN_LED1, on ? HIGH : LOW); // Fyzická akce
    globalState.updateLeds(on, current.led2, current.led3); // Uložení stavu
}
```

---

## Výsledný přínos pro projekt
* **Jediný zdroj pravdy (Single Source of Truth):** Displej a hry se nikdy neptají přímo pinů čidel, ale čtou jednotnou strukturu `SensorData`.
* **Minimální doba držení mutexu:** Mutex je zamčen pouze na desítky nanosekund při zápisu/čtení z RAM.
* **Čistý a modulární kód:** Výměna hardwarového pinu nebo typu čidla ovlivní pouze `SensorManager`, zatímco grafika a herní logika zůstává netknutá.
