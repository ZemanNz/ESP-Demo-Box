#ifndef WIFI_CSI_MANAGER_H
#define WIFI_CSI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "esp_wifi.h"
#include <math.h>

#define CSI_MAX_SUBCARRIERS 64
#define CSI_WINDOW_SIZE 8

class WiFiCsiManager {
public:
    WiFiCsiManager();
    ~WiFiCsiManager();

    // Inicializace CSI hardware a callbacků
    bool begin();

    // Zapnutí a vypnutí aktivního zpracování CSI (šetří CPU v jiných módech)
    void start();
    void stop();
    bool isRunning() const { return running; }

    // Spuštění kalibrace klidového šumu místnosti (výchozí 5 sekund)
    void startCalibration(uint32_t durationMs = 5000);
    bool isCalibrating() const { return calibrating; }
    uint8_t getCalibrationProgress() const; // 0 až 100 %
    uint8_t getCalibrationSecondsLeft() const; // 5.. 4.. 3.. 2.. 1.. 0

    // Pravidelná aktualizace a vyhodnocení (volá se ve smyčce Task_WiFi_Web)
    void update();
    void updateDistanceContinuous(); // Běží neustále ve všech módech

    // Výsledné hodnoty pro SystemState a UI
    float getSmoothedMetric() const { return smoothedMetric; }
    float getThreshold() const { return detectionThreshold; }
    float getBaselineNoise() const { return baselineNoise; }
    bool isMotionDetected() const { return motionDetected; }
    int8_t getRssi() const { return currentRssi; }
    float getDistanceMeters() const { return smoothedDistance; }

    // Statický interní callback volaný z Wi-Fi ovladače ESP-IDF
    static void csiRxCallback(void *ctx, wifi_csi_info_t *info);

private:
    bool initialized;
    bool running;

    // Vzdálenost a RSSI filtr
    float filteredRssi;
    float smoothedDistance;

    // Kalibrace
    bool calibrating;
    unsigned long calibStartTime;
    uint32_t calibDurationMs;
    float calibSum;
    uint32_t calibSamples;

    // Detekce a adaptivní práh
    float baselineNoise;
    float sensitivityMultiplier;
    float sensitivityOffset;
    float detectionThreshold;
    float smoothedMetric;

    // Debounce filtrace (Fast Attack / Smooth Release)
    int motionScore;
    bool motionDetected;

    // Aktivní probing pro stálý tok paketů
    WiFiUDP udp;
    unsigned long lastProbeTime;

    // Filtrování MAC adresy připojeného telefonu
    uint8_t targetMac[6];
    bool targetMacValid;

    // Interní CSI stav (statické pozadí a klouzavé okno)
    static float staticBg[CSI_MAX_SUBCARRIERS];
    static bool bgInitialized;
    static float dynamicWindow[CSI_WINDOW_SIZE][CSI_MAX_SUBCARRIERS];
    static int windowHead;
    static int windowCount;
    static float lastGrad[CSI_MAX_SUBCARRIERS];

    static volatile float currentMetric;
    static volatile int8_t currentRssi;
    static volatile uint32_t csiPacketCount;

    void updateTargetStation();
    void sendProbePacket();
};

extern WiFiCsiManager wifiCsiManager;

#endif // WIFI_CSI_MANAGER_H
