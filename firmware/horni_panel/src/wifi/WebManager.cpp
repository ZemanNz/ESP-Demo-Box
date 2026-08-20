#include "WebManager.h"    // Naše hlavička s deklarací třídy WebManager
#include "WebPages.h"      // Soubor, kde máme zkompilované texty webu (HTML, CSS a JS)
#include <ArduinoJson.h>   // Knihovna pro snadnou práci s formátem JSON (čtení i tvorba zpráv)

// --- Konstruktor: Vytvoření objektu WebManager ---
WebManager::WebManager() 
    : pState(nullptr),       // Zpočátku nemáme nastavený ukazatel na stav (bude předán v begin())
      server(HTTP_PORT),     // Webový server bude naslouchat na standardním HTTP portu 80
      ws("/ws"),             // WebSocket spojení bude dostupné na webové adrese "ws://.../ws"
      lastTelemetryMs(0) {   // Nastavíme časovač odesílání dat na 0
}

// --- Destruktor ---
WebManager::~WebManager() {
    // V mikroprocesorech objekt WebManager žije po celou dobu běhu, úklid není potřeba
}

// --- Funkce begin(): Spustí celý bezdrátový systém ---
bool WebManager::begin(SystemState* state) {
    pState = state; // Uložíme si odkaz na paměť celého boxu, abychom z ní mohli číst a psát do ní

    Serial.println("[WebManager] Spoustim Wi-Fi Access Point...");
    WiFi.mode(WIFI_AP); // Nastavíme ESP32 do role přístupového bodu (Access Point - chová se jako Wi-Fi router)
    
    // Nastavení IP adresy ESP32 na 192.168.4.1 a masky podsítě na 255.255.255.0
    IPAddress apIP(192, 168, 4, 1);
    IPAddress netMsk(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, netMsk); // Aplikujeme IP nastavení na Wi-Fi kartu ESP32

    // Pokud je heslo zadané a má aspoň 8 znaků, vytvoří se zaheslovaná síť, jinak otevřená
    if (strlen(WIFI_AP_PASS) >= 8) {
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONN);
    } else {
        WiFi.softAP(WIFI_AP_SSID, NULL, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONN); // Otevřená síť bez hesla
    }

    // Ladicí výpisy do sériové linky
    Serial.print("[WebManager] AP SSID: ");
    Serial.println(WIFI_AP_SSID);
    Serial.print("[WebManager] IP adresa: ");
    Serial.println(WiFi.softAPIP());

    // Spuštění DNS serveru pro Captive Portal na portu 53
    // "*" znamená: jakýkoliv webový dotaz z mobilu (např. google.com) přesměruj na naši IP adresu 192.168.4.1
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    Serial.println("[WebManager] Captive Portal DNS spusten.");

    // Nastavíme obsluhu WebSocketu a webových stránek
    setupWebSocket();
    setupRoutes();

    // Spustíme webový server
    server.begin();
    Serial.println("[WebManager] HTTP Server a WebSocket bezi na portu 80.");

    return true;
}

// --- Funkce setupWebSocket(): Nastavení chování WebSocketu ---
void WebManager::setupWebSocket() {
    // Když se na WebSocketu něco stane (připojení, zpráva), zavolá se naše metoda onWebSocketEvent
    ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, 
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->onWebSocketEvent(server, client, type, arg, data, len);
    });
    server.addHandler(&ws); // Zaregistrujeme WebSocket handler do webového serveru
}

// --- Funkce setupRoutes(): Nastavení HTTP adres, které server nabízí ---
void WebManager::setupRoutes() {
    // 1. Požadavek na hlavní stránku (adresa "/")
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Pošleme text HTML uložený ve Flash paměti (PROGMEM)
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html; charset=utf-8", INDEX_HTML);
        response->addHeader("Cache-Control", "public, max-age=3600"); // Řekneme mobilu, ať si stránku pamatuje 1 hodinu
        request->send(response); // Odešleme odpověď klientovi
    });

    // 2. Požadavek na CSS styly (adresa "/style.css")
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", STYLE_CSS);
        response->addHeader("Cache-Control", "public, max-age=86400"); // Styly ulož do mezipaměti na 24 hodin
        request->send(response);
    });

    // 3. Požadavek na JavaScript (adresa "/script.js")
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", SCRIPT_JS);
        response->addHeader("Cache-Control", "public, max-age=86400"); // Skript ulož do mezipaměti na 24 hodin
        request->send(response);
    });

    // 4. Pomocná lambda funkce pro automatické přesměrování (Redirect) na úvodní stránku
    auto captiveRedirect = [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    };

    // Tyto adresy si mobilní telefony volají na pozadí, aby zjistily, jestli funguje internet:
    server.on("/generate_204", HTTP_GET, captiveRedirect);        // Kontrola připojení pro telefony s Androidem
    server.on("/gen_204", HTTP_GET, captiveRedirect);             // Další varianta pro Android
    server.on("/hotspot-detect.html", HTTP_GET, captiveRedirect); // Kontrola pro iPhone a iPad (Apple iOS)
    server.on("/library/test/success.html", HTTP_GET, captiveRedirect); // Starší Apple zařízení
    server.on("/canonical.html", HTTP_GET, captiveRedirect);      // Firefox a další prohlížeče
    server.on("/ncsi.txt", HTTP_GET, captiveRedirect);            // Kontrola pro Windows zařízení
    server.on("/connecttest.txt", HTTP_GET, captiveRedirect);     // Windows Network Connectivity Status

    // 5. Pokud mobil zkusí otevřít jakoukoliv jinou neexistující stránku (chyba 404), přesměrujeme ho na náš web
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });
}

// --- Funkce onWebSocketEvent(): Reakce na události z WebSocketu ---
void WebManager::onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                                  AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        // Klient (mobil) se právě úspěšně připojil k WebSocketu
        case WS_EVT_CONNECT:
            Serial.printf("[WebManager] WS Klient #%u pripojen z %s\n", client->id(), client->remoteIP().toString().c_str());
            break;

        // Klient se odpojil (např. vypnul Wi-Fi nebo odešel ze stránky)
        case WS_EVT_DISCONNECT:
            Serial.printf("[WebManager] WS Klient #%u odpojen\n", client->id());
            break;

        // Od klienta přišla nová data (textová zpráva / JSON příkaz)
        case WS_EVT_DATA: {
            AwsFrameInfo *info = (AwsFrameInfo*)arg; // Informace o datovém rámci
            // Ověříme, že zpráva přišla celá a jedná se o textový formát (WS_TEXT)
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                handleClientMessage(client, data, len); // Předáme zprávu ke zpracování
            }
            break;
        }

        case WS_EVT_PONG:  // Odpověď na ping (kontrola, že spojení žije)
        case WS_EVT_ERROR: // Došlo k chybě spojení
            break;
    }
}

// --- Funkce handleClientMessage(): Zpracování povelu z mobilu a změna stavu boxu ---
void WebManager::handleClientMessage(AsyncWebSocketClient *client, uint8_t *data, size_t len) {
    if (!pState) return; // Pokud nemáme přístup k paměti boxu, nic neděláme

    // Vytvoříme JSON dokument přímo na zásobníku paměti (stack) - bezpečné a bleskově rychlé
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len); // Převedeme příchozí text na JSON

    if (error) {
        Serial.print("[WebManager] Chyba parsovani JSON: ");
        Serial.println(error.f_str());
        return; // Pokud byl text poškozený, skončíme
    }

    // Přečteme název příkazu (hodnota klíče "cmd"), např. "setMode", "setLed", ...
    const char* cmd = doc["cmd"] | "";

    // 1. Příkaz pro změnu aktivního režimu na displeji ESP32
    if (strcmp(cmd, "setMode") == 0) {
        int mode = doc["mode"] | 0;             // Načteme číslo požadovaného módu
        pState->setMode((AppMode)mode);         // Uložíme do SystemState (vláknově bezpečně přes mutex)
    }
    // 2. Příkaz pro rozsvícení / zhasnutí jedné ze tří stavových LED
    else if (strcmp(cmd, "setLed") == 0) {
        int id = doc["id"] | 1;                 // Číslo LED (1, 2 nebo 3)
        bool state = doc["state"] | false;      // true = zapnuto, false = vypnuto
        SensorData d = pState->getSensorData(); // Načteme aktuální stavy ostatních LED
        if (id == 1) pState->updateLeds(state, d.led2, d.led3);
        else if (id == 2) pState->updateLeds(d.led1, state, d.led3);
        else if (id == 3) pState->updateLeds(d.led1, d.led2, state);
    }
    // 3. Příkaz pro nastavení úhlu klasického serva (0 až 180 stupňů)
    else if (strcmp(cmd, "setServo") == 0) {
        int val = doc["val"] | 90;
        pState->updateServo((uint8_t)val);
    }
    // 4. Příkaz pro nastavení chytrého serva (-180 až +180 stupňů)
    else if (strcmp(cmd, "setSmartServo") == 0) {
        int val = doc["val"] | 0;
        pState->updateSmartServo((int16_t)val);
    }
    // 5. Příkaz pro nastavení rychlosti a směru motoru (-255 až +255)
    else if (strcmp(cmd, "setMotor") == 0) {
        int val = doc["val"] | 0;
        pState->updateMotor((int16_t)val);
    }
    // 6. Příkaz pro nastavení barvy a jasu na RGB LED pásku WS2812B
    else if (strcmp(cmd, "setLedStrip") == 0) {
        uint8_t r = doc["r"] | 0;               // Červená složka (0-255)
        uint8_t g = doc["g"] | 0;               // Zelená složka (0-255)
        uint8_t b = doc["b"] | 0;               // Modrá složka (0-255)
        uint8_t bright = doc["bright"] | 60;   // Celkový jas (0-255)
        
        // Spojíme R, G, B bajty do jednoho 32-bitového barevného čísla (0x00RRGGBB)
        uint32_t col = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        uint32_t leds[8];
        for (int i = 0; i < 8; i++) leds[i] = col; // Nastavíme stejnou barvu pro všech 8 LED na pásku
        pState->updateLedStripTop(leds, bright);   // Zapíšeme do globálního stavu
    }
    // 7. Příkaz pro pípnutí bzučáku s danou frekvencí
    else if (strcmp(cmd, "beep") == 0) {
        uint16_t freq = doc["freq"] | 1000;     // Frekvence v Hz (např. 1000 Hz)
        pState->updateBuzzer(true, freq);       // Zapneme bzučák
    }
}

// --- Funkce broadcastTelemetry(): Odeslání aktuálních dat ze všech senzorů na mobil ---
void WebManager::broadcastTelemetry() {
    // Pokud nemáme stav nebo NENÍ připojen žádný mobil, ušetříme 100 % CPU a hned skončíme!
    if (!pState || ws.count() == 0) return;

    // Vláknově bezpečně si vytáhneme aktuální snímek všech senzorů a aktivního režimu
    SensorData d = pState->getSensorData();
    AppMode mode = pState->getMode();

    // Vytvoříme lehký JSON dokument na zásobníku (stack)
    JsonDocument doc;

    // 1. Prostředí (Teploměr DHT11)
    doc["t"] = d.temperature;                  // Teplota ve stupních Celsia
    doc["h"] = d.humidity;                     // Vlhkost vzduchu v procentech

    // 2. Vzdálenostní senzory
    doc["laser"] = d.laserDistanceMm;          // Laserový ToF senzor v milimetrech
    doc["ultra"] = d.ultrasonicDistanceCm;     // Ultrazvukový senzor v centimetrech
    doc["ir"] = d.irDistanceCm;                // Analogový infračervený senzor v centimetrech
    doc["irobs"] = d.irObstacle;               // Digitální detekce překážky (true/false)

    // 3. Gyroskop a Akcelerometr (LSM6DS3)
    doc["ax"] = d.accelX;                      // Zrychlení v ose X [g]
    doc["ay"] = d.accelY;                      // Zrychlení v ose Y [g]
    doc["az"] = d.accelZ;                      // Zrychlení v ose Z [g]
    doc["gx"] = d.gyroX;                       // Úhlová rychlost X [°/s]
    doc["gy"] = d.gyroY;                       // Úhlová rychlost Y [°/s]
    doc["gz"] = d.gyroZ;                       // Úhlová rychlost Z [°/s]

    // 4. Světlo a Barva
    doc["p1"] = d.photo1;                      // Hodnota z fotorezistoru 1 (úroveň světla)
    doc["p2"] = d.photo2;                      // Hodnota z fotorezistoru 2
    doc["cr"] = d.colorR;                      // Barevný senzor TCS34725: Červená
    doc["cg"] = d.colorG;                      // Barevný senzor TCS34725: Zelená
    doc["cb"] = d.colorB;                      // Barevný senzor TCS34725: Modrá

    // 5. Ovládací prvky a tlačítka
    doc["jx"] = d.joyX;                        // Osa X analogového joysticku
    doc["jy"] = d.joyY;                        // Osa Y analogového joysticku
    doc["jbtn"] = d.joyBtn;                    // Stisknutí tlačítka v joysticku
    doc["pot"] = d.potentiometer;              // Otočný potenciometr
    doc["enc"] = d.encoderPos;                 // Pozice rotačního enkodéru
    doc["ebtn"] = d.encoderBtn;                // Stisk tlačítka enkodéru
    doc["btntop"] = d.btnTop;                  // Tlačítko na horním panelu
    doc["sw1"] = d.switch1;                    // Páčkový přepínač 1
    doc["sw2"] = d.switch2;                    // Páčkový přepínač 2

    // Pole 5 tlačítek na spodním panelu
    JsonArray btndArr = doc["btnd"].to<JsonArray>();
    for (int i = 0; i < 5; i++) {
        btndArr.add(d.btnDown[i]);
    }

    // 6. Stavy výstupů
    doc["led1"] = d.led1;                      // Stav LED 1
    doc["led2"] = d.led2;                      // Stav LED 2
    doc["led3"] = d.led3;                      // Stav LED 3

    // 7. Systémové informace
    doc["mode"] = (int)mode;                   // Aktuální číslo běžícího módu na displeji
    doc["uptime"] = millis();                  // Jak dlouho ESP32 běží od zapnutí (v milisekundách)
    doc["heap"] = ESP.getFreeHeap();           // Velikost volné operační paměti RAM v bajtech
    doc["clients"] = (int)ws.count();          // Počet právě připojených telefonů

    // Převedeme JSON do textového řetězce ve statickém poli buffer[768] (bezpečně bez dynamické alokace)
    char buffer[768];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    
    // Pokud se JSON úspěšně vytvořil, rozešleme ho všem připojeným klientům přes WebSocket
    if (len > 0) {
        ws.textAll(buffer, len);
    }
}

// --- Funkce update(): Pravidelná obsluha volaná v nekonečné smyčce ---
void WebManager::update() {
    // 1. Zpracujeme případné příchozí požadavky pro přesměrování DNS (Captive Portal)
    dnsServer.processNextRequest();

    // 2. Uklidíme z paměti klienty, kteří se už odpojili
    ws.cleanupClients();

    // 3. Pokud je připojen alespoň jeden mobil a uplynul požadovaný čas (100 ms), pošleme nová data
    if (ws.count() > 0) {
        unsigned long now = millis(); // Aktuální čas procesoru
        if (now - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
            lastTelemetryMs = now;    // Uložíme čas tohoto odeslání
            broadcastTelemetry();     // Zabalíme a pošleme telemetrii
        }
    }
}
