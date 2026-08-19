#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

// --- Základní knihovny pro ESP32 a síť ---
#include <Arduino.h>              // Základní funkce Arduina (Serial, millis, datové typy)
#include <WiFi.h>                 // Ovladač pro Wi-Fi hardware v čipu ESP32
#include <ESPAsyncWebServer.h>    // Rychlý asynchronní webový a WebSocket server
#include <DNSServer.h>            // DNS server pro Captive Portal (automatické otevření stránky)
#include "SystemState.h"          // Náš model paměti (kde jsou uložena data všech senzorů a módů)

// =============================================================================
// KONFIGURACE WI-FI SÍTĚ A SERVERU
// =============================================================================
#define WIFI_AP_SSID        "ESP-Demo-Box" // Název Wi-Fi sítě, kterou ESP32 vysílá do vzduchu
#define WIFI_AP_PASS        ""             // Heslo sítě (prázdné = otevřená síť bez nutnosti psát heslo)
#define WIFI_AP_CHANNEL     1              // Radiový kanál Wi-Fi (1 je standardní 2.4 GHz)
#define WIFI_AP_MAX_CONN    4              // Maximální počet současně připojených mobilů (šetří paměť RAM)
#define DNS_PORT            53             // Standardní síťový port pro překlad domén (DNS)
#define HTTP_PORT           80             // Standardní síťový port pro webové stránky (HTTP)

#define TELEMETRY_INTERVAL_MS 100          // Jak často (v milisekundách) posíláme nová data na web (100 ms = 10x za sekundu)

// =============================================================================
// HLAVNÍ TŘÍDA PRO SPRÁVU WEBU A WI-FI
// =============================================================================
class WebManager {
public:
    WebManager();                          // Konstruktor (nastaví výchozí hodnoty objektu)
    ~WebManager();                         // Destruktor (uklízí po zániku objektu)

    // Spuštění Wi-Fi AP, DNS serveru, Web serveru a WebSocketu (předáváme odkaz na sdílený stav boxu)
    bool begin(SystemState* state);

    // Pravidelná obsluha (volá se stále dokola v nekonečné smyčce ve vlákně pro Wi-Fi)
    void update();

private:
    SystemState* pState;                   // Ukazatel na sdílenou paměť boxu (čteme senzory, zapisujeme povely)
    AsyncWebServer server;                 // Objekt samotného webového serveru na portu 80
    AsyncWebSocket ws;                     // Objekt WebSocket kanálu na adrese "/ws" pro živá data
    DNSServer dnsServer;                   // Objekt DNS serveru, který nutí mobil otevřít naši stránku
    
    unsigned long lastTelemetryMs;         // Pamatuje si čas (millis), kdy jsme naposledy odeslali data na mobil

    void setupRoutes();                    // Nastaví, co má server poslat při otevření webu (HTML, CSS, JS)
    void setupWebSocket();                 // Nastaví reakce na události WebSocketu (připojení, odpojení, příjem zprávy)
    
    // Funkce, kterou zavolá systém, když se na WebSocketu něco stane
    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                          AwsEventType type, void *arg, uint8_t *data, size_t len);

    // Zpracuje příchozí příkaz z mobilu (např. rozsviť LED, pohni servem, změň mód)
    void handleClientMessage(AsyncWebSocketClient *client, uint8_t *data, size_t len);

    // Zabalí aktuální stav senzorů do JSONu a pošle ho všem připojeným mobilům
    void broadcastTelemetry();
};

#endif // WEB_MANAGER_H
