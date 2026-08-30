#include "WiFiCsiManager.h"

// Definice statických proměnných
float WiFiCsiManager::staticBg[CSI_MAX_SUBCARRIERS] = {0};
bool WiFiCsiManager::bgInitialized = false;
float WiFiCsiManager::dynamicWindow[CSI_WINDOW_SIZE][CSI_MAX_SUBCARRIERS] = {0};
int WiFiCsiManager::windowHead = 0;
int WiFiCsiManager::windowCount = 0;
float WiFiCsiManager::lastGrad[CSI_MAX_SUBCARRIERS] = {0};

volatile float WiFiCsiManager::currentMetric = 0.0f;
volatile int8_t WiFiCsiManager::currentRssi = -100;
volatile uint32_t WiFiCsiManager::csiPacketCount = 0;

static const uint16_t CSI_PROBE_PORT = 12345;
static const unsigned long CSI_PROBE_INTERVAL_MS = 30; // ~33 Hz aktivní vzorkování

WiFiCsiManager wifiCsiManager;

WiFiCsiManager::WiFiCsiManager()
    : initialized(false),
      running(false),
      filteredRssi(-100.0f),
      smoothedDistance(0.0f),
      calibrating(false),
      calibStartTime(0),
      calibDurationMs(5000),
      calibSum(0.0f),
      calibSamples(0),
      baselineNoise(1.0f),
      sensitivityMultiplier(1.35f),
      sensitivityOffset(0.40f),
      detectionThreshold(2.0f),
      smoothedMetric(0.0f),
      motionScore(0),
      motionDetected(false),
      lastProbeTime(0),
      targetMacValid(false) {
    memset(targetMac, 0, sizeof(targetMac));
}

WiFiCsiManager::~WiFiCsiManager() {
    stop();
}

bool WiFiCsiManager::begin() {
    if (initialized) return true;

    udp.begin(CSI_PROBE_PORT);

    wifi_csi_config_t csi_config = {
        .lltf_en           = true,
        .htltf_en          = true,
        .stbc_htltf2_en    = true,
        .ltf_merge_en      = true,
        .channel_filter_en = true,
        .manu_scale        = false,
        .shift             = false,
    };

    esp_err_t ret = esp_wifi_set_csi_config(&csi_config);
    if (ret != ESP_OK) {
        Serial.printf("[CSI] Chyba esp_wifi_set_csi_config: %d\n", ret);
        return false;
    }

    ret = esp_wifi_set_csi_rx_cb(csiRxCallback, NULL);
    if (ret != ESP_OK) {
        Serial.printf("[CSI] Chyba esp_wifi_set_csi_rx_cb: %d\n", ret);
        return false;
    }

    initialized = true;
    Serial.println("[CSI] WiFi CSI subsystém úspěšně inicializován.");
    return true;
}

void WiFiCsiManager::start() {
    if (!initialized) begin();
    if (!running) {
        esp_wifi_set_csi(true);
        running = true;
        startCalibration(5000);
        Serial.println("[CSI] WiFi CSI analýza SPUŠTĚNA.");
    }
}

void WiFiCsiManager::stop() {
    if (running) {
        esp_wifi_set_csi(false);
        running = false;
        calibrating = false;
        motionDetected = false;
        motionScore = 0;
        Serial.println("[CSI] WiFi CSI analýza ZASTAVENA.");
    }
}

void WiFiCsiManager::startCalibration(uint32_t durationMs) {
    Serial.printf("\n[CSI KALIBRACE] Zahajuji měření klidového šumu (%u ms)...\n", durationMs);
    calibrating = true;
    bgInitialized = false;
    calibStartTime = millis();
    calibDurationMs = durationMs;
    calibSum = 0.0f;
    calibSamples = 0;
    motionScore = 0;
    motionDetected = false;
    windowHead = 0;
    windowCount = 0;
}

uint8_t WiFiCsiManager::getCalibrationProgress() const {
    if (!calibrating) return 100;
    unsigned long elapsed = millis() - calibStartTime;
    if (elapsed >= calibDurationMs) return 100;
    return (uint8_t)((elapsed * 100UL) / calibDurationMs);
}

uint8_t WiFiCsiManager::getCalibrationSecondsLeft() const {
    if (!calibrating) return 0;
    unsigned long elapsed = millis() - calibStartTime;
    if (elapsed >= calibDurationMs) return 0;
    uint32_t leftMs = calibDurationMs - elapsed;
    return (uint8_t)((leftMs + 999UL) / 1000UL);
}

void WiFiCsiManager::updateDistanceContinuous() {
    wifi_sta_list_t clients;
    if (esp_wifi_ap_get_sta_list(&clients) == ESP_OK && clients.num > 0) {
        int8_t rawRssi = clients.sta[0].rssi;
        if (rawRssi > -95 && rawRssi < 0) {
            currentRssi = rawRssi;

            if (filteredRssi <= -99.0f) {
                filteredRssi = (float)rawRssi;
            } else {
                // Robustní filtr proti stínům a skokům: omezíme maximální skok na 4 dB na krok
                float diff = (float)rawRssi - filteredRssi;
                if (diff > 4.0f) diff = 4.0f;
                else if (diff < -4.0f) diff = -4.0f;
                filteredRssi += diff * 0.35f;
            }

            // Log-distance model: d = 10^((RSSI_0 - RSSI) / (10 * n))
            // RSSI_0 = signál v 1 metru (-38.0 dBm pro ESP32 SoftAP)
            // n = 2.25 v interiéru
            float rssi0 = -38.0f;
            float n = 2.25f;
            float exponent = (rssi0 - filteredRssi) / (10.0f * n);
            float targetDist = powf(10.0f, exponent);
            targetDist = constrain(targetDist, 0.3f, 15.0f);

            // Vyhlazení výsledné vzdálenosti
            if (smoothedDistance <= 0.05f) {
                smoothedDistance = targetDist;
            } else {
                smoothedDistance = (smoothedDistance * 0.65f) + (targetDist * 0.35f);
            }
        }
    } else {
        currentRssi = -100;
        filteredRssi = -100.0f;
        smoothedDistance = 0.0f;
    }
}

void WiFiCsiManager::updateTargetStation() {
    wifi_sta_list_t clients;
    if (esp_wifi_ap_get_sta_list(&clients) == ESP_OK && clients.num > 0) {
        memcpy(targetMac, clients.sta[0].mac, 6);
        targetMacValid = true;
    } else {
        targetMacValid = false;
    }
}

void WiFiCsiManager::sendProbePacket() {
    if (WiFi.softAPgetStationNum() > 0) {
        IPAddress broadcastIp = WiFi.softAPIP();
        broadcastIp[3] = 255;
        uint8_t dummy[4] = {0xAA, 0x55, 0xAA, 0x55};
        udp.beginPacket(broadcastIp, CSI_PROBE_PORT);
        udp.write(dummy, sizeof(dummy));
        udp.endPacket();
    }
}

void WiFiCsiManager::csiRxCallback(void *ctx, wifi_csi_info_t *info) {
    if (!info || !info->buf || info->len <= 0) return;

    currentRssi = info->rx_ctrl.rssi;
    int8_t *csi_raw = (int8_t *)info->buf;
    int total_subcarriers = info->len / 2;
    if (total_subcarriers > CSI_MAX_SUBCARRIERS) {
        total_subcarriers = CSI_MAX_SUBCARRIERS;
    }

    // 1. Výpočet amplitud sqrt(I^2 + Q^2)
    float raw_amplitudes[CSI_MAX_SUBCARRIERS];
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

    // 2. Normalizace (odstranění celkového AGC zisku)
    float norm_amp[CSI_MAX_SUBCARRIERS];
    for (int i = start_sc; i < end_sc; i++) {
        norm_amp[i] = (raw_amplitudes[i] / sum_amplitude) * 1000.0f;
    }

    // 3. Odečtení statického pozadí (Background Subtraction) & Gradient
    if (!bgInitialized) {
        for (int i = start_sc; i < end_sc; i++) {
            staticBg[i] = norm_amp[i];
            lastGrad[i] = 0.0f;
        }
        bgInitialized = true;
    }

    float grad_diff_sum = 0.0f;
    int grad_count = 0;

    for (int i = start_sc; i < end_sc; i++) {
        // Dynamická složka = odchylka od statického kanálu (vymaže přímý LOS paprsek)
        float dyn = norm_amp[i] - staticBg[i];
        dynamicWindow[windowHead][i] = dyn;

        // Pomalu adaptujeme statické pozadí
        staticBg[i] = (staticBg[i] * 0.985f) + (norm_amp[i] * 0.015f);

        // Diference mezi sousedními subcarriery (potlačí společný šum, vytáhne multipath)
        if (i > start_sc) {
            float current_grad = norm_amp[i] - norm_amp[i - 1];
            grad_diff_sum += fabsf(current_grad - lastGrad[i]);
            lastGrad[i] = current_grad;
            grad_count++;
        }
    }

    windowHead = (windowHead + 1) % CSI_WINDOW_SIZE;
    if (windowCount < CSI_WINDOW_SIZE) {
        windowCount++;
        return;
    }

    // 4. Výpočet směrodatné odchylky dynamické složky přes klouzavé okno 8 rámců
    float total_std = 0.0f;
    int valid_sc = 0;

    for (int i = start_sc; i < end_sc; i++) {
        float mean = 0.0f;
        for (int w = 0; w < CSI_WINDOW_SIZE; w++) {
            mean += dynamicWindow[w][i];
        }
        mean /= (float)CSI_WINDOW_SIZE;

        float var = 0.0f;
        for (int w = 0; w < CSI_WINDOW_SIZE; w++) {
            float diff = dynamicWindow[w][i] - mean;
            var += diff * diff;
        }
        var /= (float)CSI_WINDOW_SIZE;

        total_std += sqrtf(var);
        valid_sc++;
    }

    if (valid_sc > 0) {
        float dyn_metric = total_std / (float)valid_sc;
        float grad_metric = (grad_count > 0) ? (grad_diff_sum / (float)grad_count) : 0.0f;
        
        // Sloučení rozptylu s gradientem pro okamžitou citlivost
        currentMetric = (dyn_metric * 0.7f) + (grad_metric * 0.3f);
        csiPacketCount++;
    }
}

void WiFiCsiManager::update() {
    if (!running) return;

    unsigned long now = millis();

    // 1. Aktivní probing paketů
    if (now - lastProbeTime >= CSI_PROBE_INTERVAL_MS) {
        lastProbeTime = now;
        sendProbePacket();
    }

    // 2. Rychlé vyhodnocení metriky (Fast Attack filtr)
    float metric = currentMetric;
    smoothedMetric = (smoothedMetric * 0.35f) + (metric * 0.65f);

    // 3. Kalibrační fáze
    if (calibrating) {
        calibSum += metric;
        calibSamples++;

        if (now - calibStartTime >= calibDurationMs) {
            calibrating = false;
            if (calibSamples > 0) {
                baselineNoise = calibSum / (float)calibSamples;
            } else {
                baselineNoise = 1.0f;
            }
            detectionThreshold = (baselineNoise * sensitivityMultiplier) + sensitivityOffset;
            Serial.printf("[CSI KALIBRACE DOKONČENA] Šum: %.2f | Práh: %.2f | RSSI: %d dBm\n",
                          baselineNoise, detectionThreshold, currentRssi);
        }
        return;
    }

    // 4. Pomalá adaptace klidového šumu na pozadí při klidu
    if (motionScore == 0 && smoothedMetric < detectionThreshold) {
        baselineNoise = (baselineNoise * 0.995f) + (smoothedMetric * 0.005f);
        detectionThreshold = (baselineNoise * sensitivityMultiplier) + sensitivityOffset;
    }

    // 5. Fast Attack / Smooth Release Debounce
    if (smoothedMetric > detectionThreshold) {
        motionScore += 3;
        if (motionScore > 10) motionScore = 10;
    } else {
        if (motionScore > 0) motionScore--;
    }

    motionDetected = (motionScore >= 3);
}
