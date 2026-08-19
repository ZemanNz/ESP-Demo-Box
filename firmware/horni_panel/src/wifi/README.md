# Detailní dokumentace Wi-Fi a Webového rozhraní (ESP-Demo-Box)

Tento dokument slouží jako kompletní technická a výuková příručka pro bezdrátový modul projektu **ESP-Demo-Box**. Popisuje architekturu, princip fungování, paměťové optimalizace a **detailní rozbor každého souboru a funkce řádek po řádku**.

---

## Obsah
1. [Celková architektura a princip fungování](#1-celková-architektura-a-princip-fungování)
2. [Rozbor `WebManager.h`](#2-rozbor-webmanagerh)
3. [Rozbor `WebManager.cpp`](#3-rozbor-webmanagercpp)
4. [Rozbor `WebPages.h`](#4-rozbor-webpagesh)
5. [Rozbor `index.html`](#5-rozbor-indexhtml)
6. [Rozbor `style.css`](#6-rozbor-stylecss)
7. [Rozbor `script.js`](#7-rozbor-scriptjs)
8. [Rozbor integrace v `main.cpp`](#8-rozbor-integrace-v-maincpp)
9. [Otázky a odpovědi k obhajobě projektu (FAQ)](#9-otázky-a-odpovědi-k-obhajobě-projektu-faq)

---

## 1. Celková architektura a princip fungování

ESP32-S3 je dvoujádrový mikroprocesor (240 MHz). Abychom zajistili maximální rychlost a plynulost, je systém rozdělen na dvě nezávislá jádra pomocí operačního systému reálného času **FreeRTOS**:

* **Core 1 (Aplikační jádro):** Vykresluje grafiku na TFT displej (2.8" ST7789), počítá herní smyčky (Snake, Flappy Bird, 2048) a čte fyzické senzory.
* **Core 0 (Síťové jádro):** Běží na něm úloha `Task_WiFi_Web`, která obsluhuje Wi-Fi Access Point, DNS server (Captive Portal), asynchronní WebServer a WebSocket spojení.
* **Sdílená paměť (`SystemState`):** Komunikace mezi oběma jádry probíhá přes vláknově bezpečnou třídu chráněnou **FreeRTOS Mutexem**. Tím je vyloučen souběh (tzv. *race condition*).

```
                      +---------------------------------------+
                      |               ESP32-S3                |
                      |                                       |
  [ Mobilní telefon ] |   +-------------------------------+   |
          |           |   |       Core 0 (Síť)            |   |
          | Wi-Fi AP  |   |  - SoftAP ("ESP-Demo-Box")    |   |
          |<=========>|   |  - Captive Portal (DNSServer) |   |
          |           |   |  - AsyncWebServer (Port 80)   |   |
          |           |   |  - WebSocket (/ws)            |   |
          |           |   +---------------+---------------+   |
          |           |                   |                   |
          |           |      [ FreeRTOS Mutex Lock ]          |
          |           |                   |                   |
          |           |   +---------------+---------------+   |
          |           |   |      SystemState (Paměť)      |   |
          |           |   +---------------+---------------+   |
          |           |                   |                   |
          |           |   +---------------+---------------+   |
          |           |   |      Core 1 (Aplikace)        |   |
          |           |   |  - Grafika TFT ST7789         |   |
          |           |   |  - Hry (Snake, Flappy Bird)   |   |
          |           |   |  - Fyzické senzory            |   |
          |           |   +-------------------------------+   |
          |           +---------------------------------------+
```

---

## 2. Rozbor `WebManager.h`

Tento hlavičkový soubor definuje veřejné i soukromé rozhraní síťového modulu.

```cpp
#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H
```
* **Řádky 1–2:** Tzv. *Header Guards* (ochranné direktivy preprocesoru). Zajišťují, že pokud je tento soubor vložen do projektu vícekrát, zkompiluje se pouze jednou a nedojde k chybě vícenásobné deklarace.

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include "SystemState.h"
```
* **Řádky 4–8:** Vložení potřebných knihoven:
  * `WiFi.h`: Nativní Wi-Fi ovladač pro ESP32.
  * `ESPAsyncWebServer.h`: Asynchronní HTTP a WebSocket server. Zpracovává síťové události na pozadí bez blokování procesoru čekacími smyčkami.
  * `DNSServer.h`: Lehký DNS server pro Captive Portal.
  * `"SystemState.h"`: Odkaz na globální stavový model našeho boxu.

```cpp
#define WIFI_AP_SSID        "ESP-Demo-Box"
#define WIFI_AP_PASS        ""             // Prázdné heslo = otevřená síť
#define WIFI_AP_CHANNEL     1
#define WIFI_AP_MAX_CONN    4
#define DNS_PORT            53
#define HTTP_PORT           80
#define TELEMETRY_INTERVAL_MS 100
```
* **Definice konstant:**
  * `WIFI_AP_SSID`: Název vysílané Wi-Fi sítě.
  * `WIFI_AP_PASS`: Heslo (prázdný řetězec = otevřená síť bez nutnosti zadávat heslo).
  * `WIFI_AP_CHANNEL`: Wi-Fi kanál 1 (standardní 2.4 GHz frekvence).
  * `WIFI_AP_MAX_CONN`: Maximální počet současně připojených telefonů (4 zařízení šetří RAM).
  * `DNS_PORT` (53) a `HTTP_PORT` (80): Standardní síťové porty.
  * `TELEMETRY_INTERVAL_MS` (100 ms): Telemetrická data posíláme 10× za sekundu, což je pro lidské oko plynulé a pro ESP32 minimální zátěž.

```cpp
class WebManager {
public:
    WebManager();
    ~WebManager();
    bool begin(SystemState* state);
    void update();
```
* **Třída `WebManager`:**
  * `begin(SystemState* state)`: Inicializační metoda. Nastaví Wi-Fi, DNS, HTTP routy a WebSocket. Přijímá ukazatel na instanci `SystemState`.
  * `update()`: Obslužná metoda, která se volá v každém průchodu nekonečné smyčky v `Task_WiFi_Web`.

```cpp
private:
    SystemState* pState;
    AsyncWebServer server;
    AsyncWebSocket ws;
    DNSServer dnsServer;
    unsigned long lastTelemetryMs;

    void setupRoutes();
    void setupWebSocket();
    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                            AwsEventType type, void *arg, uint8_t *data, size_t len);
    void handleClientMessage(AsyncWebSocketClient *client, uint8_t *data, size_t len);
    void broadcastTelemetry();
};
```
* **Privátní členy:**
  * `pState`: Ukazatel na sdílený stav pro čtení senzorů a zápis povelů.
  * `server(HTTP_PORT)`: Instance webového serveru.
  * `ws("/ws")`: Instance WebSocket endpointu na adrese `/ws`.
  * `dnsServer`: Instance DNS serveru pro přesměrování.
  * `lastTelemetryMs`: Časová značka (v milisekundách) pro neblokující časovač telemetrie.
  * Privátní metody pro konfiguraci, obsluhu událostí klienta a odesílání dat.

---

## 3. Rozbor `WebManager.cpp`

### 3.1 Konstruktor a `begin()`
```cpp
WebManager::WebManager() 
    : pState(nullptr), server(HTTP_PORT), ws("/ws"), lastTelemetryMs(0) {
}
```
* Inicializační seznam konstruktoru nastaví výchozí hodnoty členských proměnných.

```cpp
bool WebManager::begin(SystemState* state) {
    pState = state;
    WiFi.mode(WIFI_AP);
    
    IPAddress apIP(192, 168, 4, 1);
    IPAddress netMsk(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, netMsk);
```
* Přepne Wi-Fi do režimu **Access Point (AP)**.
* Nastaví pevnou IP adresu `192.168.4.1` a síťovou masku `255.255.255.0`.

```cpp
    if (strlen(WIFI_AP_PASS) >= 8) {
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONN);
    } else {
        WiFi.softAP(WIFI_AP_SSID, NULL, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONN);
    }
```
* Spustí vysílání sítě. Pokud je heslo prázdné (`NULL`), vytvoří se otevřená síť bez šifrování.

```cpp
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
```
* **Klíčový prvek Captive Portalu:** DNS server začne odposlouchávat na portu 53. Hvězdička `"*"` znamená, že na **jakýkoliv** doménový dotaz z mobilu (např. `google.com`, `apple.com`) odpoví naší vlastní IP adresou `192.168.4.1`. Díky tomu mobil ví, že je v lokální síti, a ihned zobrazí úvodní stránku.

```cpp
    setupWebSocket();
    setupRoutes();
    server.begin();
    return true;
}
```
* Zaregistruje WebSocket a HTTP routy a spustí naslouchání na portu 80.

---

### 3.2 Nastavení HTTP rout (`setupRoutes`)
```cpp
void WebManager::setupRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html; charset=utf-8", INDEX_HTML);
        response->addHeader("Cache-Control", "public, max-age=3600");
        request->send(response);
    });
```
* `server.on("/", HTTP_GET, ...)`: Při požadavku na kořenový adresář vrátí obsah `INDEX_HTML`.
* `beginResponse_P`: Písmeno `_P` znamená **PROGMEM** – data se čtou přímo z Flash paměti programu bez zbytečného kopírování do RAM.
* `Cache-Control: public, max-age=3600`: Říká prohlížeči v mobilu, aby si stránku uložil do své lokální paměti na 1 hodinu. Při dalším otevření ESP32 nemusí posílat data znovu.

```cpp
    server.on("/style.css", ...);
    server.on("/script.js", ...);
```
* Obdobně servíruje kaskádové styly a JavaScript.

```cpp
    auto captiveRedirect = [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    };

    server.on("/generate_204", HTTP_GET, captiveRedirect);        // Android
    server.on("/hotspot-detect.html", HTTP_GET, captiveRedirect); // Apple iOS
    server.on("/ncsi.txt", HTTP_GET, captiveRedirect);            // Windows
```
* Mobilní telefony při připojení k Wi-Fi testují specifické adresy (např. Android testuje `/generate_204`, Apple testuje `/hotspot-detect.html`). Tyto adresy přesměrujeme (HTTP 302 Redirect) na náš dashboard, což vyvolá automatické otevření panelu na displeji telefonu.

---

### 3.3 WebSocket a příjem zpráv (`handleClientMessage`)
```cpp
void WebManager::onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                                  AwsEventType type, void *arg, uint8_t *data, size_t len) {
```
* Callback funkce pro události WebSocketu:
  * `WS_EVT_CONNECT`: Nový telefon se připojil.
  * `WS_EVT_DISCONNECT`: Telefon se odpojil.
  * `WS_EVT_DATA`: Přišla textová data (příkaz z webu).

```cpp
void WebManager::handleClientMessage(AsyncWebSocketClient *client, uint8_t *data, size_t len) {
    if (!pState) return;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
```
* `JsonDocument doc`: Využívá **ArduinoJson verze 7**. Paměť pro JSON dokument je alokována přímo na **zásobníku (stack)** funkce – nedochází k fragmentaci paměti haldy (heap).
* `deserializeJson`: Rozparsuje příchozí bajty na JSON objekt.

```cpp
    const char* cmd = doc["cmd"] | "";

    if (strcmp(cmd, "setMode") == 0) {
        int mode = doc["mode"] | 0;
        pState->setMode((AppMode)mode);
    }
```
* Zkontroluje typ příkazu (`cmd`):
  * `"setMode"`: Přepne stavový automat (`pState->setMode()`).
  * `"setLed"`: Přepne stav stavových LED diod (`pState->updateLeds()`).
  * `"setServo"`: Nastaví úhel servomotoru (`pState->updateServo()`).
  * `"setMotor"`: Nastaví PWM rychlost motoru (`pState->updateMotor()`).
  * `"setLedStrip"`: Přepočítá RGB složky a nastaví pásek WS2812B (`pState->updateLedStripTop()`).
  * `"beep"`: Spustí tón bzučáku.

---

### 3.4 Odesílání telemetrie (`broadcastTelemetry`)
```cpp
void WebManager::broadcastTelemetry() {
    if (!pState || ws.count() == 0) return;
```
* **Kritická optimalizace:** Pokud není připojen žádný klient (`ws.count() == 0`), metoda okamžitě skončí. Žádný JSON se nevytváří a procesor šetří 100 % výkonu.

```cpp
    SensorData d = pState->getSensorData();
    AppMode mode = pState->getMode();

    JsonDocument doc;
    doc["t"] = d.temperature;
    doc["h"] = d.humidity;
    doc["laser"] = d.laserDistanceMm;
    ...
```
* Bezpečně načte kopii dat z `SystemState` přes mutex.
* Naplní JSON objekt zkrácenými klíči (`t` = teplota, `h` = vlhkost, `ax`/`ay`/`az` = akcelerometr atd.), aby byl datový paket co nejmenší (cca 250 bajtů).

```cpp
    char buffer[768];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    if (len > 0) {
        ws.textAll(buffer, len);
    }
}
```
* Serializuje JSON do pevného pole znaků `buffer[768]` na zásobníku a odešle jej metodou `ws.textAll()` všem připojeným telefonům.

---

### 3.5 Periodická obsluha (`update`)
```cpp
void WebManager::update() {
    dnsServer.processNextRequest(); // Zpracování Captive Portal požadavků
    ws.cleanupClients();            // Úklid neaktivních spojení

    if (ws.count() > 0) {
        unsigned long now = millis();
        if (now - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
            lastTelemetryMs = now;
            broadcastTelemetry();   // Odeslání telemetrie 10x za sekundu
        }
    }
}
```
* Volá se v cyklu `Task_WiFi_Web`. Zajišťuje neblokující běh DNS, úklid odpojených klientů z paměti RAM a časování telemetrie.

---

## 4. Rozbor `WebPages.h`

Soubor `WebPages.h` obsahuje zkompilované webové soubory (HTML, CSS, JS) v paměti Flash programu.

```cpp
const char INDEX_HTML[] PROGMEM = R"rawliteral(
... HTML KÓD ...
)rawliteral";
```
* **Klíčové slovo `PROGMEM`:** Říká kompilátoru GCC, aby proměnnou neumisťoval do vzácné paměti RAM, ale uložil ji do **Flash paměti** mikrokontroléru.
* **C++ Raw String Literal `R"rawliteral(...)rawliteral"`:** Umožňuje zapsat libovolně dlouhý víceřádkový text včetně uvozovek, lomítek a HTML tagů bez nutnosti cokoliv escapovat (`\"`, `\n`).
* **Výhoda tohoto řešení:**
  * Celý web je součástí binárního firmwaru.
  * Nahraje se do ESP32 jedním kliknutím tlačítka **Upload** (odpadá nutnost vytvářet a nahrávat LittleFS image zvlášť).
  * Načítání z paměti Flash je okamžité.

---

## 5. Rozbor `index.html`

Moderní HTML5 aplikace navržená jako tzv. **SPA (Single Page Application)**:

1. **Meta Viewport:**
   ```html
   <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
   ```
   * Zajišťuje, že se stránka na mobilu zobrazí v měřítku 1:1, bez nutnosti přibližování a s chováním nativní aplikace.

2. **Hlavička s indikátorem stavu:**
   ```html
   <div id="status-badge" class="status-badge connecting">
       <span class="dot"></span>
       <span id="status-text">Připojování...</span>
   </div>
   ```
   * Živě ukazuje stav WebSocket spojení: *Připojování...* (žlutá), *Online* (zelená s pulzující animací), *Odpojeno* (červená).

3. **Záložky (Tabs):**
   * **📊 Telemetrie:** Karty se živými údaji (Teplota, Vlhkost, Laser ToF, Ultrazvuk, IR senzor, Gyroskop/Akcelerometr LSM6DS3, Fotorezistory, Barvy, Stav tlačítek a Joysticku).
   * **🎛️ Ovládání:** Tlačítka pro změnu režimu displeje ESP32, přepínače LED, Color Picker pro RGB pásek, posuvníky pro serva a motor, tlačítka bzučáku.
   * **⚙️ Systém:** Uptime systému, volná paměť RAM (Heap) a počet klientů.

---

## 6. Rozbor `style.css`

Stylizace je postavena na moderním tmavém designu (Dark Theme) bez jakýchkoliv externích knihoven (žádný Bootstrap ani Tailwind z internetu – 100% funguje v uzavřené offline síti):

1. **CSS proměnné (`:root`):**
   * Centrální správa barevné palety (`--bg-color: #0f172a`, `--card-bg: #1e293b`, `--primary: #38bdf8` atd.).
2. **Responzivní CSS Grid a Flexbox:**
   * Karty a ovládací prvky se automaticky přizpůsobují šířce displeje (mobil na výšku, na šířku i tablet).
3. **Dotykové přepínače (`.switch`, `.slider`):**
   * Hardwarově akcelerované CSS animace pro plynulé přepínání stavových LED.
4. **Optimalizace pro dotykové obrazovky:**
   * Vypnuté modré zvýraznění při klepnutí (`-webkit-tap-highlight-color: transparent`).

---

## 7. Rozbor `script.js`

JavaScriptový soubor řídí veškerou logiku na straně prohlížeče v telefonu.

### 7.1 WebSocket klient a Auto-Reconnect
```javascript
function initWebSocket() {
    const wsUrl = `ws://${location.host}/ws`;
    ws = new WebSocket(wsUrl);

    ws.onopen = () => { ... status: Online ... };
    ws.onclose = () => { ... status: Odpojeno ... scheduleReconnect(); };
    ws.onmessage = (event) => {
        const data = JSON.parse(event.data);
        handleTelemetry(data);
    };
}
```
* Otevře obousměrné WebSocket spojení na adrese `ws://192.168.4.1/ws`.
* Při výpadku Wi-Fi nebo odpojení automaticky spustí časovač `scheduleReconnect()`, který se každé 2 sekundy pokouší spojení obnovit.

### 7.2 Throttling (Ochrana proti zahlcení sítě)
```javascript
function throttle(func, limit) {
    let inThrottle;
    return function() {
        if (!inThrottle) {
            func.apply(this, arguments);
            inThrottle = true;
            setTimeout(() => inThrottle = false, limit);
        }
    };
}
```
* **Klíčový prvek:** Když uživatel rychle hýbe posuvníkem (např. úhel serva nebo otáčky motoru), mobil generuje desítky událostí za sekundu. Funkce `throttle` omezí frekvenci odesílání na maximálně 20 zpráv za sekundu (interval 50 ms). Tím chrání Wi-Fi zásobník ESP32 před přetečením.

### 7.3 Aktualizace DOM elementů (`handleTelemetry`)
```javascript
function handleTelemetry(d) {
    if (d.t !== undefined) setText('val-temp', d.t.toFixed(1));
    if (d.laser !== undefined) setText('val-laser', d.laser);
    ...
}
```
* Přijme JSON od ESP32 a okamžitě přepíše hodnoty v HTML prvcích na obrazovce bez blikání či překreslování celé stránky.

---

## 8. Rozbor integrace v `main.cpp`

### 8.1 Vytvoření vlákna FreeRTOS
V `setup()` souboru `main.cpp`:
```cpp
xTaskCreatePinnedToCore(Task_WiFi_Web, "WiFi_Web", 8192, NULL, 1, NULL, 0); // Core 0
```
* **Parametry volání:**
  * `Task_WiFi_Web`: Ukazatel na funkci vlákna.
  * `"WiFi_Web"`: Název úlohy pro ladicí výpisy.
  * `8192`: Velikost zásobníku v bajtech (dostatek paměti pro WebSocket a JSON operace).
  * `NULL`: Parametry předávané tasku.
  * `1`: Priorita vlákna.
  * `NULL`: Task handle.
  * `0`: **ID Jádra (Core 0)** – síťové operace běží striktně odděleně od grafiky na Core 1.

### 8.2 Smyčka úlohy `Task_WiFi_Web`
```cpp
void Task_WiFi_Web(void *pvParameters) {
    Serial.print("Task_WiFi_Web bezi na uvazku (Core): ");
    Serial.println(xPortGetCoreID());

    webManager.begin(&globalState); // Spustí AP, DNS, Server a WS

    for (;;) {
        webManager.update();
        vTaskDelay(pdMS_TO_TICKS(15)); // 15 ms spánek pro uvolnění CPU
    }
}
```
* `vTaskDelay(pdMS_TO_TICKS(15))`: Uvolní procesor Core 0 pro interní Wi-Fi a TCP stack ESP-IDF. Zabraňuje spuštění Watchdog Timeru (WDT reset).

---

## 9. Otázky a odpovědi k obhajobě projektu (FAQ)

### Q1: Proč je pro přenos dat použit WebSocket a ne klasické HTTP REST API (AJAX/Fetch)?
> **Odpověď:** Klasické HTTP spojení vyžaduje pro každý požadavek sestavení TCP handshake a odeslání HTTP hlaviček (cca 500–800 bajtů režie na každou zprávu). Prohlížeč by se musel neustále dokola ptát serveru (*polling*).  
> **WebSocket** po úvodním handshake udržuje jedno trvalé obousměrné TCP spojení. Zprávy mají režii pouhé 2 bajty, data mohou proudit v reálném čase oběma směry a odezva je okamžitá (v řádu jednotek milisekund).

### Q2: Proč je Wi-Fi server umístěn na Core 0 a grafika displeje na Core 1?
> **Odpověď:** ESP32-S3 je dvoujádrový čip. Vykreslování grafiky (TFT ST7789 SPI) a výpočet her je náročný na procesorový čas. Pokud by Wi-Fi i grafika běžely na stejném jádře, síťová komunikace by způsobovala zasekávání animací na displeji. Rozdělením na Core 0 (síť) a Core 1 (grafika) dosahujeme 100% plynulosti obou subsystémů.

### Q3: Jak je zajištěno, že se obě jádra nepohádají o paměť (Race Condition)?
> **Odpověď:** Veškerá data jsou centralizována v instanci `SystemState`. Každé čtení i zápis je chráněn **FreeRTOS binárním Mutexem** (`xSemaphoreTake` a `xSemaphoreGive`). Pokud jedno jádro zapisuje data ze senzorů, druhé jádro počká zlomek mikrosekundy, než mutex uvolní.

### Q4: Jak funguje Captive Portal (automatické otevření stránky po připojení)?
> **Odpověď:** Využíváme vestavěný `DNSServer` naslouchající na portu 53. Když se mobil připojí k Wi-Fi, pošle DNS dotaz na internetové servery operačního systému. Náš DNS server na jakýkoliv dotaz odpoví IP adresou `192.168.4.1`. Mobilní telefon tím rozpozná přítomnost přihlašovacího portálu a automaticky otevře dashboard v systémovém okně.

### Q5: Jak je vyřešena paměťová efektivita a ochrana proti vyčerpání RAM?
> **Odpověď:** 
> 1. HTML, CSS a JS soubory jsou uloženy v paměti Flash programu (`PROGMEM`), nezatěžují RAM.
> 2. Pro JSON je použita knihovna `ArduinoJson v7` s alokací na zásobníku (stack), což eliminuje fragmentaci haldy (`heap`).
> 3. Telemetrie se generuje a odesílá pouze tehdy, když je k WebSocketu reálně připojen alespoň jeden klient (`ws.count() > 0`).
