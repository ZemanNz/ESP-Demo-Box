#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_netif_sta_list.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

#define CSI_MAX_SUBCARRIERS 64
#define CSI_WINDOW_SIZE     8
#define CSI_NUM_PHY_CLASSES 3
#define CSI_MAX_STATIONS    4
#define CSI_RSSI_WIN_SIZE   11
#define CSI_RSSI_CACHE_SIZE 8

struct SavedRssiCalib {
    uint8_t mac[6];
    float rssiAt1m;
    bool valid;
};

/**
 * @brief Nezávislý stavový buffer a DSP stav pro jeden připojený telefon (podle MAC).
 */
struct CsiStationSlot {
    bool active;
    uint8_t mac[6];
    uint32_t ipAddr;           // IPv4 v network byte order (0 = čeká na DHCP)
    uint32_t connectOrder;     // Pořadí připojení pro konzistentní zobrazení #1..#4

    // --- Odhad vzdálenosti (11-vzorkový upper-trimmed medián + dt-Kalman v dBm) ---
    int8_t rawApRssi;
    int8_t latestRxRssi;
    unsigned long lastRxRssiMs;
    volatile uint32_t rxRssiSeq;
    uint32_t lastDistSeq;
    unsigned long lastApFallbackMs;
    bool apSeeded;
    bool rssiStale;
    float rssiAt1m;
    uint8_t dipStreak;
    int8_t dipSamples[3];
    int8_t rssiHist[CSI_RSSI_WIN_SIZE];
    uint8_t rssiHistHead;
    uint8_t rssiHistCount;
    bool kfInitialized;
    float kfX;
    float kfP;
    int8_t filteredRssi;
    float smoothedDistance;
    unsigned long lastDistUpdateMs;
    double calib1mSum;
    uint16_t calib1mCount;

    // --- Per-PHY-class CSI buffery dané stanice (0: 11g, 1: HT20, 2: HT20-STBC) ---
    bool phyInitialized[CSI_NUM_PHY_CLASSES];
    uint16_t phyWarmupCount[CSI_NUM_PHY_CLASSES];
    uint64_t bgActiveMask[CSI_NUM_PHY_CLASSES];
    uint64_t prevActiveMask[CSI_NUM_PHY_CLASSES];
    float staticBg[CSI_NUM_PHY_CLASSES][CSI_MAX_SUBCARRIERS];
    float prevNorm[CSI_NUM_PHY_CLASSES][CSI_MAX_SUBCARRIERS];
    bool anyPhyInitialized;
    unsigned long lastCsiMathMs;

    // --- Sdílené dynamické okno odchylek od per-PHY statického pozadí ---
    float dynamicWindow[CSI_WINDOW_SIZE][CSI_MAX_SUBCARRIERS];
    uint64_t winValidMask[CSI_WINDOW_SIZE];
    uint8_t windowHead;
    uint8_t windowCount;

    // --- Akumulátor dávek (burst-safe pod csiMux) ---
    bool hasLastRaw;
    float lastRawMetric;
    float pendingRawSum;
    float pendingRawMax;
    float pendingOscSum;
    float pendingBgSum;
    uint16_t pendingCount;
    volatile uint32_t csiPacketCount;
    volatile unsigned long lastCsiPacketMs;

    // --- Normalizovaná SNR metrika, univerzální prahy a časový debounce ---
    bool calibReady;
    float baselineNoise;       // Klidové mu (surová metrika)
    float baselineStd;         // Klidové sigma (surová metrika)
    float noiseScale;          // Jmenovatel pro SNR normalizaci
    float oscMu;
    float oscNoiseScale;
    float bgMu;
    float bgNoiseScale;
    float detectionThreshold;  // Univerzální práh v SNR jednotkách (2.4f)
    float smoothedMetric;      // Hladká SNR metrika (1.0 = klid, 2.4+ = pohyb)
    bool motionDetected;
    unsigned long lastEnvMs;
    unsigned long enterCondStartMs;
    unsigned long exitCondStartMs;

    // --- Kalibrační akumulátor (Welford variance) + tichý warm-up při připojení ---
    uint16_t calibSamples;
    double calibMean;
    double calibM2;
    double calibOscMean;
    double calibOscM2;
    double calibBgMean;
    uint16_t autoWarmupLeft;
    uint8_t probeStep;
};

/**
 * @brief Správce Wi-Fi CSI radaru a více-stanicového měření vzdálenosti z RSSI.
 *
 * Každý připojený telefon dostává vlastní izolovaný multi-PHY CSI buffer,
 * dt-adaptivní odstraňování statického pozadí, SNR normalizaci, promiskuitní
 * zachytávání RX RSSI a eskalované ARP/UDP/ICMP sondování.
 */
class WiFiCsiManager {
public:
    WiFiCsiManager();
    ~WiFiCsiManager();

    bool begin();
    void start();
    void stop();
    void setDistanceActiveMode(bool active);
    void pauseForSleep();
    void resumeFromSleep();
    bool isRunning() const { return running; }

    void startCalibration(uint32_t durationMs = 5000);
    bool isCalibrating() const { return calibrating; }
    uint8_t getCalibrationProgress() const;
    uint8_t getCalibrationSecondsLeft() const;

    void startRssi1mCalibration(uint32_t durationMs = 1500);
    bool isRssi1mCalibrating() const { return rssi1mCalibrating; }

    // Aktualizace CSI detekce pohybu + aktivního round-robin sondování (v MODE_WIFI_DETECTION)
    void update();

    // Průběžný výpočet vzdáleností všech připojených telefonů (v QR módu s jemným pingováním)
    void updateDistanceContinuous(bool enableActiveProbe = false);

    // Zjištění, zda některý připojený telefon mlčí déle než silentMs (pro WebSocket ping)
    bool hasSilentStation(unsigned long silentMs) const;

    // Souhrnné gettery (zpětná kompatibilita pro grafy a web)
    int8_t getRssi() const { return primaryRssi; }
    float getDistanceMeters() const { return primaryDistance; }
    float getSmoothedMetric() const { return smoothedMetric; }
    float getThreshold() const { return detectionThreshold; }
    bool isMotionDetected() const { return motionDetected; }

    // Bezpečný snapshot individuálních vzdáleností, RSSI a stavu čerstvosti (#1..#4)
    void copyStationMetrics(uint8_t &outCount, int8_t *outRssi, float *outDistM,
                            bool *outActive, uint8_t maxSlots, bool *outStale = nullptr) const;

private:
    bool initialized;
    volatile bool running;
    volatile bool distanceActiveMode;
    volatile bool promiscuousActive;
    volatile bool pausedForSleep;

    volatile bool calibrating;
    unsigned long calibStartMs;
    unsigned long calibDurationMs;

    bool rssi1mCalibrating;
    unsigned long rssi1mStartMs;
    unsigned long rssi1mDurationMs;

    float baselineNoise;
    float detectionThreshold;
    float smoothedMetric;
    bool motionDetected;

    int8_t primaryRssi;
    float primaryDistance;

    uint32_t nextConnectOrder;
    uint8_t probeRrCursor;
    unsigned long lastProbeMs;
    unsigned long lastStationSyncMs;

    int icmpSock;
    uint16_t icmpSeq;
    WiFiUDP probeUdp;

    CsiStationSlot slots[CSI_MAX_STATIONS];
    SavedRssiCalib rssiCalibCache[CSI_RSSI_CACHE_SIZE];
    uint8_t rssiCalibCacheHead;

    static portMUX_TYPE csiMux;
    static WiFiCsiManager *s_instance;

    void configureRadioCallbacks();
    void updatePromiscuousState();
    float lookupSavedRssiAt1m(const uint8_t *mac) const;
    void saveRssiAt1m(const uint8_t *mac, float rssiAt1m);

    void syncStations(unsigned long now, bool force = false);
    void sendRoundRobinProbe(unsigned long now, unsigned long intervalMs, bool useCsiRxClock);
    void triggerArpProbe(uint32_t targetIpNetOrder);
    void ensureIcmpSocket();
    void drainIcmpReplies();
    void updateSlotDistance(CsiStationSlot &slot, int8_t sampleRssi, unsigned long now);
    void applyDistanceSample(CsiStationSlot &slot, int8_t sampleRssi, unsigned long now);
    void finishRssi1mCalibration();
    void resetSlot(CsiStationSlot &slot, const uint8_t *mac, uint32_t ipAddr,
                   int8_t initialRssi, unsigned long now, float savedRssi1m);
    int findSlotByMacLocked(const uint8_t *mac) const;
    int findFreeSlotLocked() const;
    void finishGlobalCalibration();
    void finalizeSlotThreshold(CsiStationSlot &slot, bool fromGlobalCalib);
    void getOrderedActiveIndices(int *outIdx, uint8_t &outCount) const;

    static void csiRxCallback(void *ctx, wifi_csi_info_t *info);
    static void promiscuousRxCallback(void *buf, wifi_promiscuous_pkt_type_t type);
    static void arpTcpipCallback(void *ctx);
    void handleCsiPacket(wifi_csi_info_t *info);
    void handlePromiscuousPacket(const wifi_promiscuous_pkt_t *pkt, wifi_promiscuous_pkt_type_t type);
};

extern WiFiCsiManager wifiCsiManager;
