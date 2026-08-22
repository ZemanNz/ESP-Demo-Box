#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"

// ZDE DOPLŇ SVOJE ÚDAJE K DOMÁCÍ WI-FI SÍTI
const char* ssid = "zemcom";
const char* password = "radekzeman";

unsigned long lastPrintTime = 0;

// Toto je speciální ESP-IDF callback funkce. 
// Spustí se na pozadí ÚPLNĚ POKAŽDÉ, když ESP32 chytí Wi-Fi paket.
void csi_callback(void *ctx, wifi_csi_info_t *info) {
    if (millis() - lastPrintTime < 100) return; 
    lastPrintTime = millis();

    int8_t *csi_data = (int8_t *)info->buf;
    static int last_amplitudes[20]; 
    int fluctuation = 0;
    int index = 0;
    
    // 1. Výpočet fluktuace (Tady se nic nemění)
    for (int i = 30; i < 50; i++) {
        int current_amplitude = abs(csi_data[i]);
        fluctuation += abs(current_amplitude - last_amplitudes[index]);
        last_amplitudes[index] = current_amplitude;
        index++;
    }

    // --- 2. INŽENÝRSKÝ FILTR (DEBOUNCE / HYSTEREZE) ---
    
    // Zvýšili jsme práh z 25 na 50 přesně podle tvých naměřených dat!
    int threshold = 50; 
    
    // Statická proměnná si pamatuje "skóre" poplachů mezi jednotlivými pakety
    static int poplach_skore = 0; 

    // Přidáváme nebo ubíráme body
    if (fluctuation > threshold) {
        poplach_skore++; // Detekován šum, přidáme bod
    } else {
        if (poplach_skore > 0) poplach_skore--; // Je klid, uklidňujeme se a ubíráme bod
    }

    // Zastropování skóre, aby nešlo do nekonečna (0 až 5)
    if (poplach_skore > 5) poplach_skore = 5;

    // --- 3. FINÁLNÍ VYHODNOCENÍ ---
    // Poplach se spustí AŽ KDYŽ skóre dosáhne hodnoty 3 (tzn. tři výkyvy po sobě)
    if (poplach_skore >= 3) {
        Serial.printf("[ZÁCHYT] Fluktuace: %3d | Skore: %d/5 | VÝSLEDEK: [ 🚨 POHYB ]\n", fluctuation, poplach_skore);
    } else {
        Serial.printf("[ZÁCHYT] Fluktuace: %3d | Skore: %d/5 | VÝSLEDEK: [ Klid ]\n", fluctuation, poplach_skore);
    }
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