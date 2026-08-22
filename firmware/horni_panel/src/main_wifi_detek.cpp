#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"

// ZDE DOPLŇ SVOJE ÚDAJE K DOMÁCÍ WI-FI SÍTI
const char* ssid = "TVOJE_WIFI_JMENO";
const char* password = "TVOJE_WIFI_HESLO";

unsigned long lastPrintTime = 0;

// Toto je speciální ESP-IDF callback funkce. 
// Spustí se na pozadí ÚPLNĚ POKAŽDÉ, když ESP32 chytí Wi-Fi paket.
void csi_callback(void *ctx, wifi_csi_info_t *info) {
    
    // Zpomalovač: Vypíšeme data jen každých 500 milisekund, ať to stíháš číst
    if (millis() - lastPrintTime < 500) {
        return; 
    }
    lastPrintTime = millis();

    // Vytažení základních informací o paketu
    int rssi = info->rx_ctrl.rssi;
    
    Serial.printf("\n[ZÁCHYT] RSSI: %d dBm | MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                  rssi, 
                  info->mac[0], info->mac[1], info->mac[2], 
                  info->mac[3], info->mac[4], info->mac[5]);

    // V info->buf jsou schovaná ta kouzelná surová data (CSI) - pole amplitud a fází.
    // Vypíšeme si jen prvních 10 hodnot pro ukázku, jak to vlnění vypadá.
    int8_t *csi_data = (int8_t *)info->buf;
    
    Serial.print("CSI Amplitudy (vzorek): ");
    for (int i = 0; i < 10; i++) {
        Serial.printf("%4d ", csi_data[i]);
    }
    Serial.println("\n--------------------------------------------------");
}

void setup() {
    Serial.begin(115200);
    delay(2000); // Počkáme 2 vteřiny, ať stihneš otevřít terminál
    Serial.println("\n--- Startuji ESP32-S3 Wi-Fi CSI Radar ---");

    // 1. Klasické připojení přes Arduino framework
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    Serial.print("Připojuji se k Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nPripojeno! IP adresa: " + WiFi.localIP().toString());

    // 2. Nízkoúrovňová magie ESP-IDF: Konfigurace CSI
    wifi_csi_config_t csi_config;
    csi_config.lltf_en = true;
    csi_config.htltf_en = true;
    csi_config.stbc_htltf2_en = true;
    csi_config.ltf_merge_en = true;
    csi_config.channel_filter_en = true;
    csi_config.manu_scale = false;
    csi_config.shift = false;
    
    // Zapnutí CSI, přiřazení naší callback funkce a aktivace
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(csi_callback, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));
    
    Serial.println("CSI Radar plně aktivován! Skenuji prostor...");
}

void loop() {
    // V hlavní smyčce nemusíme dělat VŮBEC NIC! 
    // FreeRTOS operační systém řeší chytání paketů a spouštění callbacku 
    // čistě na pozadí, takže procesor má hromadu času na jinou práci.
    delay(1000);
}