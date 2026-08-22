#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "esp_wifi.h"
#include <math.h>

// =============================================================================
// KONFIGURACE WI-FI
// =============================================================================
const char* ssid     = "zemcom";
const char* password = "radekzeman";

// =============================================================================
// GLOBÁLNÍ KONSTANTY A BUFFER PRO KLOUZAVÉ OKNO (MOVING VARIANCE)
// =============================================================================
#define MAX_SUBCARRIERS 64
#define WINDOW_SIZE 16          // Velikost okna pro výpočet rozptylu v čase

uint8_t target_bssid[6] = {0};   // MAC adresa routeru (BSSID)
bool bssid_valid = false;

WiFiUDP udp;
const uint16_t PROBE_PORT = 12345;
unsigned long last_probe_time = 0;
const unsigned long PROBE_INTERVAL_MS = 25; // 40 Hz aktivní vzorkování

// Kruhový buffer pro posledních 16 CSI rámců
static float csi_window[WINDOW_SIZE][MAX_SUBCARRIERS] = {0};
static int window_head = 0;
static int window_count = 0;

// Mezi-vláknové proměnné
static volatile float current_metric = 0.0f;
static volatile uint32_t csi_packet_count = 0;

// Filtrování a adaptivní práh
float smoothed_metric = 0.0f;
float baseline_noise = 1.5f;
float sensitivity_multiplier = 1.4f; // Násobek klidu pro vyvolání poplachu
float sensitivity_offset = 0.5f;     // Pevná rezerva
float detection_threshold = 2.5f;

// Kalibrace
bool is_calibrating = true;
unsigned long calibration_start_time = 0;
const unsigned long CALIBRATION_DURATION_MS = 3500;
float calib_sum = 0.0f;
uint32_t calib_samples = 0;

// Debounce detekce pohybu
int motion_score = 0;
const int MOTION_SCORE_MAX = 10;
const int MOTION_TRIGGER_LEVEL = 3;

// =============================================================================
// CSI CALLBACK (běží ve Wi-Fi tasku / ISR)
// =============================================================================
void csi_callback(void *ctx, wifi_csi_info_t *info) {
    if (!info || !info->buf || info->len <= 0) return;

    // 1. Filtrovat pouze náš router
    if (bssid_valid && memcmp(info->mac, target_bssid, 6) != 0) {
        return;
    }

    int8_t *csi_raw = (int8_t *)info->buf;
    int total_subcarriers = info->len / 2;
    if (total_subcarriers > MAX_SUBCARRIERS) {
        total_subcarriers = MAX_SUBCARRIERS;
    }

    // 2. Výpočet skutečných amplitud sqrt(I^2 + Q^2)
    float raw_amplitudes[MAX_SUBCARRIERS];
    float sum_amplitude = 0.0f;

    int start_sc = 6;
    int end_sc = (total_subcarriers > 12) ? (total_subcarriers - 6) : total_subcarriers;

    for (int i = start_sc; i < end_sc; i++) {
        int8_t real = csi_raw[2 * i];
        int8_t imag = csi_raw[2 * i + 1];
        float amp = sqrtf((float)(real * real + imag * imag));
        raw_amplitudes[i] = amp;
        sum_amplitude += amp;
    }

    if (sum_amplitude < 1.0f) return;

    // 3. Normalizace (Gain Lock - nezávislost na AGC a RSSI)
    for (int i = start_sc; i < end_sc; i++) {
        csi_window[window_head][i] = (raw_amplitudes[i] / sum_amplitude) * 1000.0f;
    }

    window_head = (window_head + 1) % WINDOW_SIZE;
    if (window_count < WINDOW_SIZE) {
        window_count++;
        return; // Počkáme na naplnění okna
    }

    // 4. Výpočet směrodatné odchylky (Standard Deviation) přes časové okno
    float total_std = 0.0f;
    int valid_sc = 0;

    for (int i = start_sc; i < end_sc; i++) {
        // Průměr subcarrieru v okně
        float mean = 0.0f;
        for (int w = 0; w < WINDOW_SIZE; w++) {
            mean += csi_window[w][i];
        }
        mean /= (float)WINDOW_SIZE;

        // Rozptyl
        float var = 0.0f;
        for (int w = 0; w < WINDOW_SIZE; w++) {
            float diff = csi_window[w][i] - mean;
            var += diff * diff;
        }
        var /= (float)WINDOW_SIZE;

        total_std += sqrtf(var);
        valid_sc++;
    }

    if (valid_sc > 0) {
        float avg_std = total_std / (float)valid_sc;
        current_metric = avg_std;
        csi_packet_count++;
    }
}

// =============================================================================
// AKTIVNÍ PROBING (odesílá UDP paket pro stabilní vzorkování 40 Hz)
// =============================================================================
void send_probe_packet() {
    if (WiFi.status() == WL_CONNECTED) {
        IPAddress gateway = WiFi.gatewayIP();
        uint8_t dummy_data[4] = {0xAA, 0x55, 0xAA, 0x55};
        udp.beginPacket(gateway, PROBE_PORT);
        udp.write(dummy_data, sizeof(dummy_data));
        udp.endPacket();
    }
}

// =============================================================================
// KALIBRACE
// =============================================================================
void start_calibration() {
    Serial.println("\n[KALIBRACE] Zahajuji měření klidového šumu místnosti (3.5s)...");
    Serial.println("[KALIBRACE] Prosím, nehýbejte se v místnosti!");
    is_calibrating = true;
    calibration_start_time = millis();
    calib_sum = 0.0f;
    calib_samples = 0;
    motion_score = 0;
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println("\n=======================================================");
    Serial.println("   ESP32-S3 Wi-Fi CSI Radar v2.1 (Moving Variance)   ");
    Serial.println("=======================================================");

    // 1. Připojení k Wi-Fi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    Serial.print("Připojuji k Wi-Fi: ");
    Serial.print(ssid);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        Serial.print(".");
    }
    Serial.println("\nPřipojeno!");
    Serial.print("IP adresa:  "); Serial.println(WiFi.localIP());
    Serial.print("Gateway IP: "); Serial.println(WiFi.gatewayIP());

    // 2. Uložení BSSID routeru
    uint8_t* bssid = WiFi.BSSID();
    if (bssid != nullptr) {
        memcpy(target_bssid, bssid, 6);
        bssid_valid = true;
        Serial.printf("Router BSSID (MAC): %02X:%02X:%02X:%02X:%02X:%02X\n",
                      target_bssid[0], target_bssid[1], target_bssid[2],
                      target_bssid[3], target_bssid[4], target_bssid[5]);
    } else {
        Serial.println("VAROVÁNÍ: Nepodařilo se načíst BSSID routeru!");
    }

    // 3. UDP socket pro aktivní dotazování
    udp.begin(PROBE_PORT);

    // 4. Konfigurace CSI
    wifi_csi_config_t csi_config = {
        .lltf_en           = true,
        .htltf_en          = true,
        .stbc_htltf2_en    = true,
        .ltf_merge_en      = true,
        .channel_filter_en = true,
        .manu_scale        = false,
        .shift             = false,
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(csi_callback, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));
    
    Serial.println("CSI subsystém aktivován.");
    start_calibration();
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
    unsigned long now = millis();

    // 1. Aktivní probing každých 25 ms (40 Hz)
    if (now - last_probe_time >= PROBE_INTERVAL_MS) {
        last_probe_time = now;
        send_probe_packet();
    }

    // 2. Příkazy z klávesnice
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'c' || cmd == 'C') {
            start_calibration();
        } else if (cmd == '1') {
            sensitivity_multiplier = 1.25f;
            sensitivity_offset = 0.35f;
            detection_threshold = (baseline_noise * sensitivity_multiplier) + sensitivity_offset;
            Serial.printf("[NASTAVENÍ] Citlivost: VYSOKÁ (Práh: %.2f)\n", detection_threshold);
        } else if (cmd == '2') {
            sensitivity_multiplier = 1.40f;
            sensitivity_offset = 0.50f;
            detection_threshold = (baseline_noise * sensitivity_multiplier) + sensitivity_offset;
            Serial.printf("[NASTAVENÍ] Citlivost: STŘEDNÍ (Práh: %.2f)\n", detection_threshold);
        } else if (cmd == '3') {
            sensitivity_multiplier = 1.65f;
            sensitivity_offset = 0.75f;
            detection_threshold = (baseline_noise * sensitivity_multiplier) + sensitivity_offset;
            Serial.printf("[NASTAVENÍ] Citlivost: NÍZKÁ (Práh: %.2f)\n", detection_threshold);
        }
    }

    // 3. Pravidelné vyhodnocení (každých 100 ms)
    static unsigned long last_eval_time = 0;
    if (now - last_eval_time >= 100) {
        last_eval_time = now;

        float metric = current_metric;
        smoothed_metric = (smoothed_metric * 0.5f) + (metric * 0.5f);

        // --- Kalibrační fáze ---
        if (is_calibrating) {
            calib_sum += metric;
            calib_samples++;

            if (now - calibration_start_time >= CALIBRATION_DURATION_MS) {
                is_calibrating = false;
                if (calib_samples > 0) {
                    baseline_noise = calib_sum / (float)calib_samples;
                } else {
                    baseline_noise = 1.5f;
                }
                
                // Dynamický výpočet prahu na základě reálného klidu
                detection_threshold = (baseline_noise * sensitivity_multiplier) + sensitivity_offset;

                Serial.println("\n[KALIBRACE DOKONČENA]");
                Serial.printf("  Klidový šum (Baseline):   %.2f\n", baseline_noise);
                Serial.printf("  Detekční práh (Threshold): %.2f\n", detection_threshold);
                Serial.println("  (Klávesy: 'c' = rekalibrace, '1' = vysoká citlivost, '2' = střední, '3' = nízká)\n");
            }
            return;
        }

        // --- Pomalu adaptivní drift klidu v pozadí (pokud je klid) ---
        if (motion_score == 0 && smoothed_metric < detection_threshold) {
            baseline_noise = (baseline_noise * 0.99f) + (smoothed_metric * 0.01f);
            detection_threshold = (baseline_noise * sensitivity_multiplier) + sensitivity_offset;
        }

        // --- Hystereze / Skóre pohybu ---
        if (smoothed_metric > detection_threshold) {
            if (motion_score < MOTION_SCORE_MAX) motion_score += 2;
        } else {
            if (motion_score > 0) motion_score--;
        }

        bool is_motion = (motion_score >= MOTION_TRIGGER_LEVEL);

        // --- Vizuální sloupec ---
        // Střed grafu (pozice 15) odpovídá přesně detekčnímu prahu
        int bar_length = (int)((smoothed_metric / (detection_threshold * 2.0f)) * 30.0f);
        if (bar_length < 0) bar_length = 0;
        if (bar_length > 30) bar_length = 30;

        char bar[32];
        for (int i = 0; i < 30; i++) {
            if (i < bar_length) {
                bar[i] = '#';
            } else if (i == 15) {
                bar[i] = '|'; // Ryska prahu
            } else {
                bar[i] = ' ';
            }
        }
        bar[30] = '\0';

        // --- Výpis na sériovou linku ---
        if (is_motion) {
            Serial.printf("[POHYB  ] Signál: %4.2f | Práh: %4.2f | Skóre: %2d/%d | [%s] -> 🚨 POHYB DETEKOVÁN!\n",
                          smoothed_metric, detection_threshold, motion_score, MOTION_SCORE_MAX, bar);
        } else {
            Serial.printf("[KLID   ] Signál: %4.2f | Práh: %4.2f | Skóre: %2d/%d | [%s] -> (Klid)\n",
                          smoothed_metric, detection_threshold, motion_score, MOTION_SCORE_MAX, bar);
        }
    }
}