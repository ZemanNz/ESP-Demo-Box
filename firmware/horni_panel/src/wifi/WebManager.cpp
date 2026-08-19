#include "WebManager.h"
#include "WebPages.h"
#include <ArduinoJson.h>

WebManager::WebManager() 
    : pState(nullptr), server(HTTP_PORT), ws("/ws"), lastTelemetryMs(0) {
}

WebManager::~WebManager() {
}

bool WebManager::begin(SystemState* state) {
    pState = state;

    Serial.println("[WebManager] Spoustim Wi-Fi Access Point...");
    WiFi.mode(WIFI_AP);
    
    // Nastavení IP adresy AP (192.168.4.1)
    IPAddress apIP(192, 168, 4, 1);
    IPAddress netMsk(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, netMsk);

    // Spuštění AP (pokud je heslo kratší než 8 znaků, vytvoří otevřenou síť)
    if (strlen(WIFI_AP_PASS) >= 8) {
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONN);
    } else {
        WiFi.softAP(WIFI_AP_SSID, NULL, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONN);
    }

    Serial.print("[WebManager] AP SSID: ");
    Serial.println(WIFI_AP_SSID);
    Serial.print("[WebManager] IP adresa: ");
    Serial.println(WiFi.softAPIP());

    // Spuštění DNS serveru pro Captive Portal (všechny DNS požadavky přesměrovat na náš AP)
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    Serial.println("[WebManager] Captive Portal DNS spusten.");

    // Nastavení WebSocketu
    setupWebSocket();

    // Nastavení HTTP rout
    setupRoutes();

    // Spuštění HTTP serveru
    server.begin();
    Serial.println("[WebManager] HTTP Server a WebSocket bezi na portu 80.");

    return true;
}

void WebManager::setupWebSocket() {
    ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, 
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->onWebSocketEvent(server, client, type, arg, data, len);
    });
    server.addHandler(&ws);
}

void WebManager::setupRoutes() {
    // 1. Hlavní stránka (index.html)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html; charset=utf-8", INDEX_HTML);
        response->addHeader("Cache-Control", "public, max-age=3600");
        request->send(response);
    });

    // 2. Kaskádové styly (style.css)
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", STYLE_CSS);
        response->addHeader("Cache-Control", "public, max-age=86400");
        request->send(response);
    });

    // 3. JavaScript (script.js)
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", SCRIPT_JS);
        response->addHeader("Cache-Control", "public, max-age=86400");
        request->send(response);
    });

    // 4. Captive Portal redirecty (pro Android, Apple, Windows)
    auto captiveRedirect = [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    };

    server.on("/generate_204", HTTP_GET, captiveRedirect);        // Android
    server.on("/gen_204", HTTP_GET, captiveRedirect);             // Android
    server.on("/hotspot-detect.html", HTTP_GET, captiveRedirect); // Apple iOS/macOS
    server.on("/library/test/success.html", HTTP_GET, captiveRedirect);
    server.on("/canonical.html", HTTP_GET, captiveRedirect);
    server.on("/ncsi.txt", HTTP_GET, captiveRedirect);            // Windows
    server.on("/connecttest.txt", HTTP_GET, captiveRedirect);

    // 5. Fallback pro neznámé routy -> přesměrovat na index
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });
}

void WebManager::onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                                  AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WebManager] WS Klient #%u pripojen z %s\n", client->id(), client->remoteIP().toString().c_str());
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("[WebManager] WS Klient #%u odpojen\n", client->id());
            break;

        case WS_EVT_DATA: {
            AwsFrameInfo *info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                handleClientMessage(client, data, len);
            }
            break;
        }

        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void WebManager::handleClientMessage(AsyncWebSocketClient *client, uint8_t *data, size_t len) {
    if (!pState) return;

    // Parsování příchozího JSON příkazu bez dynamických alokací
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        Serial.print("[WebManager] JSON parse error: ");
        Serial.println(error.f_str());
        return;
    }

    const char* cmd = doc["cmd"] | "";

    // 1. Přepnutí módu displeje
    if (strcmp(cmd, "setMode") == 0) {
        int mode = doc["mode"] | 0;
        pState->setMode((AppMode)mode);
    }
    // 2. Přepnutí stavové LED
    else if (strcmp(cmd, "setLed") == 0) {
        int id = doc["id"] | 1;
        bool state = doc["state"] | false;
        SensorData d = pState->getSensorData();
        if (id == 1) pState->updateLeds(state, d.led2, d.led3);
        else if (id == 2) pState->updateLeds(d.led1, state, d.led3);
        else if (id == 3) pState->updateLeds(d.led1, d.led2, state);
    }
    // 3. Nastavení klasického serva (0-180)
    else if (strcmp(cmd, "setServo") == 0) {
        int val = doc["val"] | 90;
        pState->updateServo((uint8_t)val);
    }
    // 4. Nastavení chytrého serva
    else if (strcmp(cmd, "setSmartServo") == 0) {
        int val = doc["val"] | 0;
        pState->updateSmartServo((int16_t)val);
    }
    // 5. Nastavení motoru
    else if (strcmp(cmd, "setMotor") == 0) {
        int val = doc["val"] | 0;
        pState->updateMotor((int16_t)val);
    }
    // 6. Nastavení RGB LED pásku
    else if (strcmp(cmd, "setLedStrip") == 0) {
        uint8_t r = doc["r"] | 0;
        uint8_t g = doc["g"] | 0;
        uint8_t b = doc["b"] | 0;
        uint8_t bright = doc["bright"] | 60;
        
        uint32_t col = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        uint32_t leds[8];
        for (int i = 0; i < 8; i++) leds[i] = col;
        pState->updateLedStripTop(leds, bright);
    }
    // 7. Bzučák
    else if (strcmp(cmd, "beep") == 0) {
        uint16_t freq = doc["freq"] | 1000;
        pState->updateBuzzer(true, freq);
    }
}

void WebManager::broadcastTelemetry() {
    if (!pState || ws.count() == 0) return;

    SensorData d = pState->getSensorData();
    AppMode mode = pState->getMode();

    // Sestavení lehkého telemetrického JSONu přímo na stacku
    JsonDocument doc;

    // Prostředí
    doc["t"] = d.temperature;
    doc["h"] = d.humidity;

    // Vzdálenosti
    doc["laser"] = d.laserDistanceMm;
    doc["ultra"] = d.ultrasonicDistanceCm;
    doc["ir"] = d.irDistanceCm;
    doc["irobs"] = d.irObstacle;

    // IMU
    doc["ax"] = d.accelX;
    doc["ay"] = d.accelY;
    doc["az"] = d.accelZ;
    doc["gx"] = d.gyroX;
    doc["gy"] = d.gyroY;
    doc["gz"] = d.gyroZ;

    // Světlo & barva
    doc["p1"] = d.photo1;
    doc["p2"] = d.photo2;
    doc["cr"] = d.colorR;
    doc["cg"] = d.colorG;
    doc["cb"] = d.colorB;

    // Ovládací prvky
    doc["jx"] = d.joyX;
    doc["jy"] = d.joyY;
    doc["jbtn"] = d.joyBtn;
    doc["pot"] = d.potentiometer;
    doc["enc"] = d.encoderPos;
    doc["ebtn"] = d.encoderBtn;
    doc["btntop"] = d.btnTop;
    doc["sw1"] = d.switch1;
    doc["sw2"] = d.switch2;

    JsonArray btndArr = doc["btnd"].to<JsonArray>();
    for (int i = 0; i < 5; i++) {
        btndArr.add(d.btnDown[i]);
    }

    // Stavy výstupů
    doc["led1"] = d.led1;
    doc["led2"] = d.led2;
    doc["led3"] = d.led3;

    // Systém
    doc["mode"] = (int)mode;
    doc["uptime"] = millis();
    doc["heap"] = ESP.getFreeHeap();
    doc["clients"] = (int)ws.count();

    // Serializace do fixního bufferu a odeslání všem připojeným klientům
    char buffer[768];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    if (len > 0) {
        ws.textAll(buffer, len);
    }
}

void WebManager::update() {
    // 1. Zpracování požadavků Captive Portalu (DNS)
    dnsServer.processNextRequest();

    // 2. Úklid neaktivních / odpojených WS klientů
    ws.cleanupClients();

    // 3. Periodické odesílání telemetrie přes WebSocket (pouze když je někdo připojen)
    if (ws.count() > 0) {
        unsigned long now = millis();
        if (now - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
            lastTelemetryMs = now;
            broadcastTelemetry();
        }
    }
}
