#include "WiFiCsiManager.h"

#include <string.h>
#include <math.h>
#include <esp_netif.h>
#include <esp_netif_net_stack.h>
#include <lwip/ip4_addr.h>
#include <lwip/netif.h>
#include <lwip/etharp.h>
#include <lwip/tcpip.h>
#include <lwip/sockets.h>
#include <lwip/prot/icmp.h>
#include <lwip/inet_chksum.h>

namespace {
constexpr uint16_t CSI_PROBE_UDP_PORT = 12345;
constexpr unsigned long CSI_STATION_SYNC_INTERVAL_MS = 120;
constexpr unsigned long CSI_STALE_PACKET_MS          = 2000;
constexpr unsigned long CSI_DECAY_START_MS           = 400;
constexpr unsigned long RSSI_STALE_TIMEOUT_MS        = 3500;
constexpr unsigned long RSSI_PREPKT_SEED_MS          = 1000;

// Výchozí realistický log-distance model pro 2.4 GHz uplink (phone TX -> ESP32-S3 AP RX)
constexpr float DEFAULT_RSSI_AT_1M = -49.0f;
constexpr float PATH_LOSS_N        = 2.50f;

// Univerzální SNR prahy (1.0 = klidové pozadí, 2.4+ = detekce pohybu)
constexpr float SNR_ENTER_TH       = 2.40f;
constexpr float SNR_EXIT_TH        = 1.70f;
constexpr float SNR_INSTANT_TH     = 3.50f;
constexpr unsigned long CSI_ENTER_HOLD_MS = 100;
constexpr unsigned long CSI_EXIT_HOLD_MS  = 350;

inline void resetSlotCsiBuffersLocked(CsiStationSlot &s) {
    memset(s.phyInitialized, 0, sizeof(s.phyInitialized));
    memset(s.phyWarmupCount, 0, sizeof(s.phyWarmupCount));
    memset(s.bgActiveMask, 0, sizeof(s.bgActiveMask));
    memset(s.prevActiveMask, 0, sizeof(s.prevActiveMask));
    memset(s.staticBg, 0, sizeof(s.staticBg));
    memset(s.prevNorm, 0, sizeof(s.prevNorm));
    s.anyPhyInitialized = false;
    s.lastCsiMathMs = 0;

    memset(s.dynamicWindow, 0, sizeof(s.dynamicWindow));
    memset(s.winValidMask, 0, sizeof(s.winValidMask));
    s.windowHead = 0;
    s.windowCount = 0;

    s.hasLastRaw = false;
    s.lastRawMetric = 0.0f;
    s.pendingRawSum = 0.0f;
    s.pendingRawMax = 0.0f;
    s.pendingOscSum = 0.0f;
    s.pendingBgSum = 0.0f;
    s.pendingCount = 0;
    s.csiPacketCount = 0;
    s.lastCsiPacketMs = 0;

    s.calibReady = false;
    s.baselineNoise = 0.25f;
    s.baselineStd = 0.04f;
    s.noiseScale = 0.12f;
    s.oscMu = 0.20f;
    s.oscNoiseScale = 0.10f;
    s.bgMu = 0.20f;
    s.bgNoiseScale = 0.08f;
    s.detectionThreshold = SNR_ENTER_TH;
    s.smoothedMetric = 1.0f;
    s.motionDetected = false;
    s.lastEnvMs = 0;
    s.enterCondStartMs = 0;
    s.exitCondStartMs = 0;

    s.calibSamples = 0;
    s.calibMean = 0.0;
    s.calibM2 = 0.0;
    s.calibOscMean = 0.0;
    s.calibOscM2 = 0.0;
    s.calibBgMean = 0.0;
    s.autoWarmupLeft = 20;
}
} // namespace

portMUX_TYPE WiFiCsiManager::csiMux = portMUX_INITIALIZER_UNLOCKED;
WiFiCsiManager *WiFiCsiManager::s_instance = nullptr;
WiFiCsiManager wifiCsiManager;

WiFiCsiManager::WiFiCsiManager()
    : initialized(false),
      running(false),
      distanceActiveMode(false),
      promiscuousActive(false),
      pausedForSleep(false),
      calibrating(false),
      calibStartMs(0),
      calibDurationMs(5000),
      rssi1mCalibrating(false),
      rssi1mStartMs(0),
      rssi1mDurationMs(1500),
      baselineNoise(1.0f),
      detectionThreshold(SNR_ENTER_TH),
      smoothedMetric(1.0f),
      motionDetected(false),
      primaryRssi(-100),
      primaryDistance(0.0f),
      nextConnectOrder(1),
      probeRrCursor(0),
      lastProbeMs(0),
      lastStationSyncMs(0),
      icmpSock(-1),
      icmpSeq(0),
      rssiCalibCacheHead(0) {
    memset(slots, 0, sizeof(slots));
    memset(rssiCalibCache, 0, sizeof(rssiCalibCache));
    s_instance = this;
}

WiFiCsiManager::~WiFiCsiManager() {
    pauseForSleep();
    if (icmpSock >= 0) {
        lwip_close(icmpSock);
        icmpSock = -1;
    }
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void WiFiCsiManager::configureRadioCallbacks() {
    wifi_csi_config_t csi_config = {
        .lltf_en           = true,
        .htltf_en          = true,
        .stbc_htltf2_en    = true,
        .ltf_merge_en      = true,
        .channel_filter_en = false,
        .manu_scale        = false,
        .shift             = 0,
    };

    esp_err_t ret = esp_wifi_set_csi_config(&csi_config);
    if (ret != ESP_OK) {
        Serial.printf("[CSI] Chyba esp_wifi_set_csi_config: %d\n", ret);
    }

    ret = esp_wifi_set_csi_rx_cb(csiRxCallback, this);
    if (ret != ESP_OK) {
        Serial.printf("[CSI] Chyba esp_wifi_set_csi_rx_cb: %d\n", ret);
    }

    wifi_promiscuous_filter_t promFilter = {};
    promFilter.filter_mask = WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_MGMT;
    esp_wifi_set_promiscuous_filter(&promFilter);
    esp_wifi_set_promiscuous_rx_cb(promiscuousRxCallback);
}

bool WiFiCsiManager::begin() {
    if (initialized) return true;
    s_instance = this;

    configureRadioCallbacks();
    esp_wifi_set_csi(false);
    esp_wifi_set_promiscuous(false);
    promiscuousActive = false;

    ensureIcmpSocket();
    initialized = true;
    Serial.println("[CSI] Multi-PHY CSI & Promiscuous RSSI subsystém inicializován.");
    return true;
}

void WiFiCsiManager::ensureIcmpSocket() {
    if (icmpSock >= 0) return;
    int s = lwip_socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (s >= 0) {
        lwip_fcntl(s, F_SETFL, O_NONBLOCK);
        icmpSock = s;
    }
}

void WiFiCsiManager::updatePromiscuousState() {
    bool wantPromisc = initialized && !pausedForSleep && !running && distanceActiveMode;
    if (wantPromisc == promiscuousActive) return;

    if (wantPromisc) {
        wifi_promiscuous_filter_t promFilter = {};
        promFilter.filter_mask = WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_MGMT;
        esp_wifi_set_promiscuous_filter(&promFilter);
        esp_wifi_set_promiscuous_rx_cb(promiscuousRxCallback);
        if (esp_wifi_set_promiscuous(true) == ESP_OK) {
            promiscuousActive = true;
        }
    } else {
        promiscuousActive = false;
        esp_wifi_set_promiscuous(false);
    }
}

void WiFiCsiManager::setDistanceActiveMode(bool active) {
    if (!initialized && !pausedForSleep) begin();
    distanceActiveMode = active;
    updatePromiscuousState();
}

void WiFiCsiManager::start() {
    if (pausedForSleep) return;
    if (!initialized) begin();
    if (!running) {
        // Zajištění vzájemného vyloučení s promiskuitním režimem
        if (promiscuousActive) {
            promiscuousActive = false;
            esp_wifi_set_promiscuous(false);
        }

        portENTER_CRITICAL(&csiMux);
        for (int i = 0; i < CSI_MAX_STATIONS; i++) {
            if (slots[i].active) {
                resetSlotCsiBuffersLocked(slots[i]);
            }
        }
        portEXIT_CRITICAL(&csiMux);

        smoothedMetric = 1.0f;
        motionDetected = false;
        detectionThreshold = SNR_ENTER_TH;
        baselineNoise = 1.0f;

        esp_wifi_set_csi(true);
        running = true;
        Serial.println("[CSI] Multi-PHY radar spuštěn (SNR režim, promiskuitní RX vypnut).");
    }
}

void WiFiCsiManager::stop() {
    if (running) {
        running = false;
        calibrating = false;
        esp_wifi_set_csi(false);
        motionDetected = false;
        smoothedMetric = 1.0f;
        Serial.println("[CSI] Radar pozastaven.");
    }
    updatePromiscuousState();
}

void WiFiCsiManager::pauseForSleep() {
    pausedForSleep = true;
    calibrating = false;
    rssi1mCalibrating = false;
    if (running) {
        running = false;
        esp_wifi_set_csi(false);
    }
    if (promiscuousActive) {
        promiscuousActive = false;
        esp_wifi_set_promiscuous(false);
    }
    motionDetected = false;
    smoothedMetric = 1.0f;
}

void WiFiCsiManager::resumeFromSleep() {
    pausedForSleep = false;
    if (!initialized) {
        begin();
        return;
    }
    configureRadioCallbacks();
    esp_wifi_set_csi(false);
    esp_wifi_set_promiscuous(false);
    promiscuousActive = false;
    if (icmpSock >= 0) {
        lwip_close(icmpSock);
        icmpSock = -1;
    }
    ensureIcmpSocket();
    syncStations(millis(), true);
    updatePromiscuousState();
}

void WiFiCsiManager::startCalibration(uint32_t durationMs) {
    portENTER_CRITICAL(&csiMux);
    for (int i = 0; i < CSI_MAX_STATIONS; i++) {
        slots[i].calibReady = false;
        slots[i].calibSamples = 0;
        slots[i].calibMean = 0.0;
        slots[i].calibM2 = 0.0;
        slots[i].calibOscMean = 0.0;
        slots[i].calibOscM2 = 0.0;
        slots[i].calibBgMean = 0.0;
        slots[i].autoWarmupLeft = 0;
        slots[i].motionDetected = false;
        slots[i].enterCondStartMs = 0;
        slots[i].exitCondStartMs = 0;
        slots[i].smoothedMetric = 1.0f;
        slots[i].detectionThreshold = SNR_ENTER_TH;
    }
    portEXIT_CRITICAL(&csiMux);

    calibDurationMs = durationMs;
    calibStartMs = millis();
    calibrating = true;
    motionDetected = false;
    smoothedMetric = 1.0f;
    detectionThreshold = SNR_ENTER_TH;
    Serial.printf("[CSI] Zahájena SNR kalibrace (%lu ms)...\n", (unsigned long)durationMs);
}

uint8_t WiFiCsiManager::getCalibrationProgress() const {
    if (!calibrating) return 100;
    unsigned long elapsed = millis() - calibStartMs;
    if (elapsed >= calibDurationMs) return 100;
    return (uint8_t)((elapsed * 100UL) / calibDurationMs);
}

uint8_t WiFiCsiManager::getCalibrationSecondsLeft() const {
    if (!calibrating) return 0;
    unsigned long elapsed = millis() - calibStartMs;
    if (elapsed >= calibDurationMs) return 0;
    return (uint8_t)((calibDurationMs - elapsed + 999UL) / 1000UL);
}

void WiFiCsiManager::startRssi1mCalibration(uint32_t durationMs) {
    if (rssi1mCalibrating) return;
    for (int i = 0; i < CSI_MAX_STATIONS; i++) {
        slots[i].calib1mSum = 0.0;
        slots[i].calib1mCount = 0;
    }
    rssi1mDurationMs = durationMs;
    rssi1mStartMs = millis();
    rssi1mCalibrating = true;
    Serial.printf("[RSSI] Zahájena 1m kalibrace vzdálenosti (%lu ms)...\n", (unsigned long)durationMs);
}

void WiFiCsiManager::finishRssi1mCalibration() {
    rssi1mCalibrating = false;
    for (int i = 0; i < CSI_MAX_STATIONS; i++) {
        CsiStationSlot &s = slots[i];
        if (!s.active) continue;

        float refRssi = s.rssiAt1m;
        if (s.calib1mCount >= 2) {
            refRssi = (float)(s.calib1mSum / (double)s.calib1mCount);
        } else if (s.kfInitialized) {
            refRssi = s.kfX;
        } else if (s.rawApRssi < 0 && s.rawApRssi > -98) {
            refRssi = (float)s.rawApRssi;
        }

        s.rssiAt1m = constrain(refRssi, -70.0f, -25.0f);
        saveRssiAt1m(s.mac, s.rssiAt1m);

        if (s.kfInitialized) {
            float exponent = (s.rssiAt1m - s.kfX) / (10.0f * PATH_LOSS_N);
            s.smoothedDistance = constrain(powf(10.0f, exponent), 0.3f, 15.0f);
        }
        Serial.printf("[RSSI] Slot %d 1m kalibrace hotova: rssiAt1m=%.1f dBm (%u vz.) -> %.2fm\n",
                      i, s.rssiAt1m, s.calib1mCount, s.smoothedDistance);
    }
}

float WiFiCsiManager::lookupSavedRssiAt1m(const uint8_t *mac) const {
    if (mac == nullptr) return DEFAULT_RSSI_AT_1M;
    for (int i = 0; i < CSI_RSSI_CACHE_SIZE; i++) {
        if (rssiCalibCache[i].valid && memcmp(rssiCalibCache[i].mac, mac, 6) == 0) {
            return rssiCalibCache[i].rssiAt1m;
        }
    }
    return DEFAULT_RSSI_AT_1M;
}

void WiFiCsiManager::saveRssiAt1m(const uint8_t *mac, float rssiAt1m) {
    if (mac == nullptr) return;
    for (int i = 0; i < CSI_RSSI_CACHE_SIZE; i++) {
        if (rssiCalibCache[i].valid && memcmp(rssiCalibCache[i].mac, mac, 6) == 0) {
            rssiCalibCache[i].rssiAt1m = rssiAt1m;
            return;
        }
    }
    for (int i = 0; i < CSI_RSSI_CACHE_SIZE; i++) {
        if (!rssiCalibCache[i].valid) {
            memcpy(rssiCalibCache[i].mac, mac, 6);
            rssiCalibCache[i].rssiAt1m = rssiAt1m;
            rssiCalibCache[i].valid = true;
            return;
        }
    }
    memcpy(rssiCalibCache[rssiCalibCacheHead].mac, mac, 6);
    rssiCalibCache[rssiCalibCacheHead].rssiAt1m = rssiAt1m;
    rssiCalibCache[rssiCalibCacheHead].valid = true;
    rssiCalibCacheHead = (rssiCalibCacheHead + 1) % CSI_RSSI_CACHE_SIZE;
}

int WiFiCsiManager::findSlotByMacLocked(const uint8_t *mac) const {
    if (mac == nullptr) return -1;
    for (int i = 0; i < CSI_MAX_STATIONS; i++) {
        if (slots[i].active && memcmp(slots[i].mac, mac, 6) == 0) {
            return i;
        }
    }
    return -1;
}

int WiFiCsiManager::findFreeSlotLocked() const {
    for (int i = 0; i < CSI_MAX_STATIONS; i++) {
        if (!slots[i].active) return i;
    }
    return -1;
}

void WiFiCsiManager::resetSlot(CsiStationSlot &s, const uint8_t *mac, uint32_t ipAddr,
                               int8_t initialRssi, unsigned long now, float savedRssi1m) {
    memset(&s, 0, sizeof(CsiStationSlot));
    s.active = true;
    memcpy(s.mac, mac, 6);
    s.ipAddr = ipAddr;
    s.connectOrder = nextConnectOrder++;

    s.rawApRssi = initialRssi;
    s.latestRxRssi = -100;
    s.lastRxRssiMs = 0;
    s.rxRssiSeq = 0;
    s.lastDistSeq = 0;
    s.lastApFallbackMs = now - RSSI_PREPKT_SEED_MS;
    s.apSeeded = false;
    s.rssiStale = false;
    s.rssiAt1m = savedRssi1m;
    s.filteredRssi = (initialRssi < 0 && initialRssi > -98) ? initialRssi : -100;
    s.kfInitialized = false;
    s.smoothedDistance = 0.0f;
    s.lastDistUpdateMs = now;

    resetSlotCsiBuffersLocked(s);
}

void WiFiCsiManager::syncStations(unsigned long now, bool force) {
    if (pausedForSleep) return;
    if (!force && (now - lastStationSyncMs < CSI_STATION_SYNC_INTERVAL_MS)) {
        return;
    }
    lastStationSyncMs = now;

    wifi_sta_list_t wifiList;
    memset(&wifiList, 0, sizeof(wifiList));
    if (esp_wifi_ap_get_sta_list(&wifiList) != ESP_OK) {
        return;
    }

    esp_netif_sta_list_t netifList;
    memset(&netifList, 0, sizeof(netifList));
    bool hasNetif = (esp_netif_get_sta_list(&wifiList, &netifList) == ESP_OK);

    int staCount = wifiList.num;
    if (staCount < 0) staCount = 0;
    if (staCount > ESP_WIFI_MAX_CONN_NUM) staCount = ESP_WIFI_MAX_CONN_NUM;

    float cachedRssi1m[ESP_WIFI_MAX_CONN_NUM];
    uint32_t resolvedIps[ESP_WIFI_MAX_CONN_NUM];
    for (int k = 0; k < staCount; k++) {
        const uint8_t *mac = wifiList.sta[k].mac;
        cachedRssi1m[k] = lookupSavedRssiAt1m(mac);
        uint32_t ip = 0;
        if (hasNetif) {
            for (int m = 0; m < netifList.num && m < ESP_WIFI_MAX_CONN_NUM; m++) {
                if (memcmp(netifList.sta[m].mac, mac, 6) == 0) {
                    ip = netifList.sta[m].ip.addr;
                    break;
                }
            }
        }
        resolvedIps[k] = ip;
    }

    portENTER_CRITICAL(&csiMux);

    bool slotSeen[CSI_MAX_STATIONS] = {false, false, false, false};

    for (int k = 0; k < staCount; k++) {
        const uint8_t *mac = wifiList.sta[k].mac;
        int8_t rssi = wifiList.sta[k].rssi;
        uint32_t ip = resolvedIps[k];

        int existing = findSlotByMacLocked(mac);
        if (existing >= 0) {
            slotSeen[existing] = true;
            slots[existing].rawApRssi = rssi;
            if (ip != 0) slots[existing].ipAddr = ip;
        } else {
            int freeIdx = findFreeSlotLocked();
            if (freeIdx >= 0) {
                resetSlot(slots[freeIdx], mac, ip, rssi, now, cachedRssi1m[k]);
                slotSeen[freeIdx] = true;
            }
        }
    }

    for (int i = 0; i < CSI_MAX_STATIONS; i++) {
        if (slots[i].active && !slotSeen[i]) {
            slots[i].active = false;
        }
    }

    portEXIT_CRITICAL(&csiMux);
}

void WiFiCsiManager::getOrderedActiveIndices(int *outIdx, uint8_t &outCount) const {
    outCount = 0;
    for (int i = 0; i < CSI_MAX_STATIONS; i++) {
        if (slots[i].active) {
            outIdx[outCount++] = i;
        }
    }
    for (uint8_t a = 0; a < outCount; a++) {
        for (uint8_t b = a + 1; b < outCount; b++) {
            if (slots[outIdx[b]].connectOrder < slots[outIdx[a]].connectOrder) {
                int tmp = outIdx[a];
                outIdx[a] = outIdx[b];
                outIdx[b] = tmp;
            }
        }
    }
}

void WiFiCsiManager::drainIcmpReplies() {
    if (icmpSock < 0) return;
    uint8_t rxBuf[64];
    struct sockaddr_in fromAddr;
    socklen_t fromLen = sizeof(fromAddr);
    for (int i = 0; i < 8; i++) {
        int r = lwip_recvfrom(icmpSock, rxBuf, sizeof(rxBuf), MSG_DONTWAIT,
                              (struct sockaddr *)&fromAddr, &fromLen);
        if (r <= 0) break;
    }
}

void WiFiCsiManager::arpTcpipCallback(void *ctx) {
    uint32_t ipRaw = (uint32_t)(uintptr_t)ctx;
    if (ipRaw == 0) return;

    esp_netif_t *apNetif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (apNetif == nullptr) return;

    struct netif *lwipNetif = static_cast<struct netif *>(esp_netif_get_netif_impl(apNetif));
    if (lwipNetif == nullptr || !netif_is_up(lwipNetif)) return;

    ip4_addr_t target;
    target.addr = ipRaw;
    etharp_request(lwipNetif, &target);
}

void WiFiCsiManager::triggerArpProbe(uint32_t targetIpNetOrder) {
    if (targetIpNetOrder == 0) return;
    tcpip_try_callback(arpTcpipCallback, (void *)(uintptr_t)targetIpNetOrder);
}

void WiFiCsiManager::sendRoundRobinProbe(unsigned long now, unsigned long intervalMs, bool useCsiRxClock) {
    if (pausedForSleep || (now - lastProbeMs < intervalMs)) return;

    int activeIdx[CSI_MAX_STATIONS];
    uint8_t activeCount = 0;
    getOrderedActiveIndices(activeIdx, activeCount);
    if (activeCount == 0) return;

    lastProbeMs = now;
    if (probeRrCursor >= activeCount) {
        probeRrCursor = 0;
    }
    int targetSlotIdx = activeIdx[probeRrCursor];
    probeRrCursor = (probeRrCursor + 1) % activeCount;

    CsiStationSlot &targetSlot = slots[targetSlotIdx];
    uint32_t targetIp = targetSlot.ipAddr;
    unsigned long lastRxMs = 0;

    portENTER_CRITICAL(&csiMux);
    lastRxMs = useCsiRxClock ? targetSlot.lastCsiPacketMs : targetSlot.lastRxRssiMs;
    portEXIT_CRITICAL(&csiMux);

    unsigned long silenceMs = (lastRxMs == 0) ? 9999UL : (now - lastRxMs);
    unsigned long silentThresholdMs = useCsiRxClock ? 200UL : 400UL;
    bool slotSilent = (silenceMs > silentThresholdMs);

    ensureIcmpSocket();
    drainIcmpReplies();

    uint8_t step = targetSlot.probeStep++;

    if (targetIp == 0) {
        // Stanice čeká na DHCP -> jen občasný jemný UDP signál, bez zahlcení broadcastem
        if ((step & 0x03) == 0) {
            probeUdp.beginPacket(IPAddress(192, 168, 4, 255), CSI_PROBE_UDP_PORT);
            uint8_t p = (uint8_t)(++icmpSeq & 0xFF);
            probeUdp.write(&p, 1);
            probeUdp.endPacket();
        }
        return;
    }

    // Pro tiché stanice vždy prioritizujeme ARP dotaz (telefon jej nemůže firewallovat) + sekundární TIM probuzení
    if (slotSilent) {
        triggerArpProbe(targetIp);
    }

    if (!slotSilent || (step & 0x01) == 0) {
        if (icmpSock >= 0) {
            uint8_t pkt[20];
            memset(pkt, 0, sizeof(pkt));
            struct icmp_echo_hdr *iecho = (struct icmp_echo_hdr *)pkt;
            ICMPH_TYPE_SET(iecho, ICMP_ECHO);
            ICMPH_CODE_SET(iecho, 0);
            iecho->chksum = 0;
            iecho->id = htons(0x4553);
            iecho->seqno = htons(++icmpSeq);
            pkt[8]  = 'E'; pkt[9]  = 'S'; pkt[10] = 'P';
            pkt[11] = 'C'; pkt[12] = 'S'; pkt[13] = 'I';
            iecho->chksum = inet_chksum(pkt, sizeof(pkt));

            struct sockaddr_in destAddr;
            memset(&destAddr, 0, sizeof(destAddr));
            destAddr.sin_family = AF_INET;
            destAddr.sin_addr.s_addr = targetIp;

            lwip_sendto(icmpSock, pkt, sizeof(pkt), MSG_DONTWAIT,
                        (struct sockaddr *)&destAddr, sizeof(destAddr));
        }
        // Každý čtvrtý klidový cyklus přidáme lehký ARP dotaz pro udržení ARP tabulky
        if (!slotSilent && (step & 0x03) == 3) {
            triggerArpProbe(targetIp);
        }
    } else {
        // Sekundární unicast UDP sonda (nastaví TIM bit v beaconu pro probuzení spícího mobilu)
        probeUdp.beginPacket(IPAddress(targetIp), CSI_PROBE_UDP_PORT);
        uint8_t p = (uint8_t)(++icmpSeq & 0xFF);
        probeUdp.write(&p, 1);
        probeUdp.endPacket();
    }
}

void WiFiCsiManager::applyDistanceSample(CsiStationSlot &s, int8_t sampleRssi, unsigned long now) {
    // 1. Odmítnutí hlubokých ojedinělých RF propadů (> 8 dB), pokud netrvají >= 3 po sobě jdoucí pakety
    if (s.kfInitialized && ((s.kfX - (float)sampleRssi) > 8.0f)) {
        if (s.dipStreak < 3) {
            s.dipSamples[s.dipStreak++] = sampleRssi;
        }
        if (s.dipStreak < 3) {
            return;
        }
        int32_t dSum = (int32_t)s.dipSamples[0] + (int32_t)s.dipSamples[1] + (int32_t)s.dipSamples[2];
        sampleRssi = (int8_t)lroundf((float)dSum / 3.0f);
        s.dipStreak = 0;
    } else {
        s.dipStreak = 0;
    }

    // 2. Vložení do unikátního kruhového bufferu (11 reálných RX paketů)
    s.rssiHist[s.rssiHistHead] = sampleRssi;
    s.rssiHistHead = (s.rssiHistHead + 1) % CSI_RSSI_WIN_SIZE;
    if (s.rssiHistCount < CSI_RSSI_WIN_SIZE) {
        s.rssiHistCount++;
    }

    int8_t sorted[CSI_RSSI_WIN_SIZE];
    uint8_t n = s.rssiHistCount;
    for (uint8_t i = 0; i < n; i++) {
        sorted[i] = s.rssiHist[i];
    }
    for (uint8_t i = 1; i < n; i++) {
        int8_t key = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    // Upper-trimmed mediánový odhad (odřízne spodních ~30% vícecestných propadů a horní špičku)
    uint8_t lowCut = (n >= 5) ? (n / 3) : 0;
    uint8_t highCut = (n >= 7) ? 1 : 0;
    uint8_t startIdx = lowCut;
    uint8_t endIdx = (n > highCut) ? (n - highCut) : n;
    if (startIdx >= endIdx) {
        startIdx = 0;
        endIdx = n;
    }

    uint8_t coreLen = endIdx - startIdx;
    uint8_t midIdx = startIdx + (coreLen / 2);
    float estRssi = 0.0f;
    if (coreLen >= 3) {
        int32_t sum = 0;
        for (uint8_t i = startIdx; i < endIdx; i++) {
            sum += sorted[i];
        }
        float coreMean = (float)sum / (float)coreLen;
        estRssi = 0.5f * coreMean + 0.5f * (float)sorted[midIdx];
    } else {
        estRssi = (float)sorted[midIdx];
    }

    // 3. dt-škálovaný 1D Kalmanův filtr přímo v dBm
    float dtSec = (now > s.lastDistUpdateMs) ? ((float)(now - s.lastDistUpdateMs) * 0.001f) : 0.20f;
    dtSec = constrain(dtSec, 0.02f, 2.00f);
    s.lastDistUpdateMs = now;

    constexpr float KF_Q_PER_SEC = 1.80f; // Přirozená dynamika chůze v interiéru
    constexpr float KF_R         = 5.00f; // Rozptyl jednotlivých RX RSSI vzorků
    if (!s.kfInitialized) {
        s.kfX = estRssi;
        s.kfP = 5.0f;
        s.kfInitialized = true;
    } else {
        float pPred = s.kfP + (KF_Q_PER_SEC * dtSec);
        float k = pPred / (pPred + KF_R);
        s.kfX = s.kfX + k * (estRssi - s.kfX);
        s.kfP = (1.0f - k) * pPred;
    }

    s.filteredRssi = (int8_t)lroundf(s.kfX);

    // 4. Log-distance model s individuální 1m kalibrací slotu
    float ref1m = (s.rssiAt1m < -20.0f && s.rssiAt1m > -80.0f) ? s.rssiAt1m : DEFAULT_RSSI_AT_1M;
    float exponent = (ref1m - s.kfX) / (10.0f * PATH_LOSS_N);
    s.smoothedDistance = constrain(powf(10.0f, exponent), 0.3f, 15.0f);
}

void WiFiCsiManager::updateSlotDistance(CsiStationSlot &s, int8_t sampleRssi, unsigned long now) {
    if (sampleRssi >= 0 || sampleRssi <= -98) {
        return;
    }
    applyDistanceSample(s, sampleRssi, now);
}

void WiFiCsiManager::updateDistanceContinuous(bool enableActiveProbe) {
    if (pausedForSleep) return;

    unsigned long now = millis();
    syncStations(now);

    int activeIdx[CSI_MAX_STATIONS];
    uint8_t activeCount = 0;
    getOrderedActiveIndices(activeIdx, activeCount);

    if (rssi1mCalibrating && activeCount == 0) {
        rssi1mCalibrating = false;
    }

    if ((enableActiveProbe || rssi1mCalibrating) && !running && activeCount > 0) {
        unsigned long baseInterval = rssi1mCalibrating ? 80UL : 250UL;
        unsigned long minInterval  = rssi1mCalibrating ? 40UL : 100UL;
        unsigned long probeInterval = (activeCount == 1) ? baseInterval : max(minInterval, baseInterval / activeCount);
        sendRoundRobinProbe(now, probeInterval, false);
    }

    if (activeCount == 0) {
        primaryRssi = -100;
        primaryDistance = 0.0f;
        return;
    }

    float bestDist = 999.0f;
    int8_t bestRssi = -100;

    for (uint8_t i = 0; i < activeCount; i++) {
        CsiStationSlot &s = slots[activeIdx[i]];

        uint32_t seq = 0;
        int8_t latestRx = -100;
        unsigned long lastRxMs = 0;
        int8_t apRssi = -100;

        portENTER_CRITICAL(&csiMux);
        seq = s.rxRssiSeq;
        latestRx = s.latestRxRssi;
        lastRxMs = s.lastRxRssiMs;
        apRssi = s.rawApRssi;
        portEXIT_CRITICAL(&csiMux);

        if (seq != s.lastDistSeq) {
            s.lastDistSeq = seq;
            if (latestRx < 0 && latestRx > -98) {
                s.rssiStale = false;
                if (rssi1mCalibrating) {
                    s.calib1mSum += (double)latestRx;
                    s.calib1mCount++;
                }
                updateSlotDistance(s, latestRx, now);
            }
        } else if (lastRxMs == 0 && !s.apSeeded && apRssi < 0 && apRssi > -98 &&
                   (now - s.lastApFallbackMs >= RSSI_PREPKT_SEED_MS)) {
            // Počáteční seed před prvním zachyceným RX paketem (max 1x za 1 s, bez duplikování okna)
            s.lastApFallbackMs = now;
            s.apSeeded = true;
            updateSlotDistance(s, apRssi, now);
        }

        if (lastRxMs > 0) {
            s.rssiStale = ((now - lastRxMs) > RSSI_STALE_TIMEOUT_MS);
        } else if (s.kfInitialized) {
            s.rssiStale = ((now - s.lastDistUpdateMs) > RSSI_STALE_TIMEOUT_MS);
        } else {
            s.rssiStale = false;
        }

        if (s.smoothedDistance > 0.05f && s.smoothedDistance < bestDist) {
            bestDist = s.smoothedDistance;
            bestRssi = s.filteredRssi;
        }
    }

    if (rssi1mCalibrating && (now - rssi1mStartMs >= rssi1mDurationMs)) {
        finishRssi1mCalibration();
        bestDist = 999.0f;
        bestRssi = -100;
        for (uint8_t i = 0; i < activeCount; i++) {
            const CsiStationSlot &s = slots[activeIdx[i]];
            if (s.smoothedDistance > 0.05f && s.smoothedDistance < bestDist) {
                bestDist = s.smoothedDistance;
                bestRssi = s.filteredRssi;
            }
        }
    }

    if (bestDist < 999.0f) {
        primaryDistance = bestDist;
        primaryRssi = bestRssi;
    } else {
        primaryDistance = 0.0f;
        primaryRssi = -100;
    }
}

bool WiFiCsiManager::hasSilentStation(unsigned long silentMs) const {
    if (pausedForSleep) return false;
    unsigned long now = millis();
    for (int i = 0; i < CSI_MAX_STATIONS; i++) {
        if (!slots[i].active) continue;
        unsigned long lastRx = running ? slots[i].lastCsiPacketMs : slots[i].lastRxRssiMs;
        if (lastRx == 0 || (now - lastRx > silentMs)) {
            return true;
        }
    }
    return false;
}

void WiFiCsiManager::copyStationMetrics(uint8_t &outCount, int8_t *outRssi, float *outDistM,
                                          bool *outActive, uint8_t maxSlots, bool *outStale) const {
    int activeIdx[CSI_MAX_STATIONS];
    uint8_t count = 0;
    getOrderedActiveIndices(activeIdx, count);
    outCount = count;

    for (uint8_t i = 0; i < maxSlots; i++) {
        if (i < count) {
            const CsiStationSlot &s = slots[activeIdx[i]];
            if (outActive) outActive[i] = true;
            if (outRssi) outRssi[i] = s.filteredRssi;
            if (outDistM) outDistM[i] = s.smoothedDistance;
            if (outStale) outStale[i] = s.rssiStale;
        } else {
            if (outActive) outActive[i] = false;
            if (outRssi) outRssi[i] = -100;
            if (outDistM) outDistM[i] = 0.0f;
            if (outStale) outStale[i] = false;
        }
    }
}

void WiFiCsiManager::finalizeSlotThreshold(CsiStationSlot &s, bool fromGlobalCalib) {
    if (s.calibSamples < 5) {
        // Málo vzorků během 5s -> nevzdáme kalibraci, necháme běžet plynulý warm-up při příchodu paketů
        if (fromGlobalCalib) {
            portENTER_CRITICAL(&csiMux);
            s.autoWarmupLeft = (uint16_t)(15 - s.calibSamples);
            s.calibReady = false;
            portEXIT_CRITICAL(&csiMux);
        }
        return;
    }

    float mean = (float)s.calibMean;
    float var = (s.calibSamples > 1) ? (float)(s.calibM2 / (double)(s.calibSamples - 1)) : 0.0f;
    if (var < 0.0f) var = 0.0f;
    float sigma = sqrtf(var);

    // Pro < 15 vzorků použijeme o něco širší konzervativní rozptyl
    float minRelFrac = (s.calibSamples < 15) ? 0.22f : 0.18f;
    float nScale = max(max(3.5f * sigma, minRelFrac * mean), 0.04f);

    float oMean = (float)s.calibOscMean;
    float oVar = (s.calibSamples > 1) ? (float)(s.calibOscM2 / (double)(s.calibSamples - 1)) : 0.0f;
    if (oVar < 0.0f) oVar = 0.0f;
    float oSigma = sqrtf(oVar);
    float oScale = max(max(3.5f * oSigma, 0.18f * oMean), 0.03f);

    float bMean = (float)s.calibBgMean;
    float bScale = max(0.25f * bMean, 0.04f);

    portENTER_CRITICAL(&csiMux);
    s.baselineNoise = mean;
    s.baselineStd = sigma;
    s.noiseScale = nScale;
    s.oscMu = oMean;
    s.oscNoiseScale = oScale;
    s.bgMu = bMean;
    s.bgNoiseScale = bScale;
    s.detectionThreshold = SNR_ENTER_TH;
    s.calibReady = true;
    s.autoWarmupLeft = 0;
    portEXIT_CRITICAL(&csiMux);
}

void WiFiCsiManager::finishGlobalCalibration() {
    calibrating = false;

    for (int i = 0; i < CSI_MAX_STATIONS; i++) {
        if (slots[i].active) {
            finalizeSlotThreshold(slots[i], true);
            Serial.printf("[CSI] Slot %d SNR kalibrace (%u vz.): mu=%.3f sigma=%.3f scale=%.3f ready=%d\n",
                          i, slots[i].calibSamples, slots[i].baselineNoise,
                          slots[i].baselineStd, slots[i].noiseScale, (int)slots[i].calibReady);
        }
    }
}

void WiFiCsiManager::promiscuousRxCallback(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type == WIFI_PKT_MISC || buf == nullptr) return;
    WiFiCsiManager *self = s_instance;
    if (self != nullptr) {
        self->handlePromiscuousPacket(static_cast<const wifi_promiscuous_pkt_t *>(buf), type);
    }
}

void WiFiCsiManager::handlePromiscuousPacket(const wifi_promiscuous_pkt_t *pkt, wifi_promiscuous_pkt_type_t type) {
    (void)type;
    if (!promiscuousActive || running || pausedForSleep || pkt == nullptr) return;
    if (pkt->rx_ctrl.rx_state != 0 || pkt->rx_ctrl.sig_len < 16) return;

    int8_t pktRssi = (int8_t)pkt->rx_ctrl.rssi;
    if (pktRssi >= 0 || pktRssi <= -98) return;

    // 802.11 Addr2 (Transmitter Address) na bajtech 10..15 hlavičky
    const uint8_t *addr2 = pkt->payload + 10;
    unsigned long now = millis();

    portENTER_CRITICAL(&csiMux);
    int sIdx = findSlotByMacLocked(addr2);
    if (sIdx >= 0) {
        slots[sIdx].latestRxRssi = pktRssi;
        slots[sIdx].lastRxRssiMs = now;
        slots[sIdx].rxRssiSeq++;
    }
    portEXIT_CRITICAL(&csiMux);
}

void WiFiCsiManager::csiRxCallback(void *ctx, wifi_csi_info_t *info) {
    WiFiCsiManager *self = static_cast<WiFiCsiManager *>(ctx);
    if (self != nullptr) {
        self->handleCsiPacket(info);
    }
}

void WiFiCsiManager::handleCsiPacket(wifi_csi_info_t *info) {
    if (!running || pausedForSleep || info == nullptr || info->buf == nullptr || info->len < 104) {
        return;
    }

    // Sledujeme pouze nepoškozené 20 MHz rámce
    if (info->rx_ctrl.rx_state != 0 || info->rx_ctrl.cwb != 0) {
        return;
    }

    int8_t *csi_raw = (int8_t *)info->buf;
    int total_subcarriers = info->len / 2;
    if (total_subcarriers > CSI_MAX_SUBCARRIERS) {
        total_subcarriers = CSI_MAX_SUBCARRIERS;
    }

    // Respektujeme first_word_invalid (sc==0 přeskočíme vždy, sc==1 pokud je neplatné)
    int start_sc = info->first_word_invalid ? 2 : 1;
    int end_sc = total_subcarriers;
    if (end_sc - start_sc < 16) return;

    // 1. Výpočet amplitud mimo kritickou sekci
    float raw_amplitudes[CSI_MAX_SUBCARRIERS];
    memset(raw_amplitudes, 0, sizeof(raw_amplitudes));
    float sum_all = 0.0f;

    for (int i = start_sc; i < end_sc; i++) {
        int re = (int)csi_raw[2 * i];
        int im = (int)csi_raw[2 * i + 1];
        float amp = sqrtf((float)(re * re + im * im));
        raw_amplitudes[i] = amp;
        sum_all += amp;
    }

    float mean_raw = sum_all / (float)(end_sc - start_sc);
    if (mean_raw < 0.05f) return;

    // Vynecháme DC a nulové ochranné subcarriery podle nízké energie
    uint64_t activeMask = 0;
    float activeSum = 0.0f;
    int activeCount = 0;
    float guardCutoff = mean_raw * 0.12f;

    for (int i = start_sc; i < end_sc; i++) {
        if (raw_amplitudes[i] > guardCutoff) {
            activeMask |= (1ULL << i);
            activeSum += raw_amplitudes[i];
            activeCount++;
        }
    }

    if (activeCount < 16 || activeSum < 1.0f) return;

    // 2. Normalizace podle průměrné amplitudy aktivní subnosné (odolné vůči změně počtu platných tónů)
    float norm_amp[CSI_MAX_SUBCARRIERS];
    memset(norm_amp, 0, sizeof(norm_amp));
    float invScale = (20.0f * (float)activeCount) / activeSum;
    for (int i = start_sc; i < end_sc; i++) {
        if (activeMask & (1ULL << i)) {
            norm_amp[i] = raw_amplitudes[i] * invScale;
        }
    }

    uint8_t phy = 0;
    if (info->rx_ctrl.sig_mode != 0) {
        phy = (info->rx_ctrl.stbc > 0) ? 2 : 1;
    }

    unsigned long now = millis();
    int8_t pktRssi = (int8_t)info->rx_ctrl.rssi;

    portENTER_CRITICAL(&csiMux);

    // 3. Přísný filtr na MAC připojeného telefonu
    int sIdx = findSlotByMacLocked(info->mac);
    if (sIdx < 0) {
        portEXIT_CRITICAL(&csiMux);
        return;
    }

    CsiStationSlot &s = slots[sIdx];
    if (pktRssi < 0 && pktRssi > -98) {
        s.latestRxRssi = pktRssi;
        s.lastRxRssiMs = now;
        s.rxRssiSeq++;
    }

    float dt = (s.anyPhyInitialized && now >= s.lastCsiMathMs)
        ? ((float)(now - s.lastCsiMathMs) * 0.001f)
        : 0.05f;
    if (dt < 0.015f) dt = 0.015f;
    if (dt > 1.0f) dt = 1.0f;

    // 4. Bez-výpadkové per-PHY-class buffery (žádné zahazování 11g/HT20/STBC rámců)
    if (!s.phyInitialized[phy]) {
        for (int i = start_sc; i < end_sc; i++) {
            s.staticBg[phy][i] = norm_amp[i];
            s.prevNorm[phy][i] = norm_amp[i];
        }
        s.bgActiveMask[phy] = activeMask;
        s.prevActiveMask[phy] = activeMask;
        s.phyInitialized[phy] = true;
        s.phyWarmupCount[phy] = 1;
        if (!s.anyPhyInitialized) {
            s.anyPhyInitialized = true;
            s.lastCsiMathMs = now;
        }
        s.lastCsiPacketMs = now;
        portEXIT_CRITICAL(&csiMux);
        return;
    }

    s.lastCsiMathMs = now;
    if (s.phyWarmupCount[phy] < 65000) {
        s.phyWarmupCount[phy]++;
    }

    // Doplníme nově objevené subnosné (např. při změně first_word_invalid) před výpočtem rozdílu
    uint64_t newBgBits = activeMask & (~s.bgActiveMask[phy]);
    uint64_t newPrevBits = activeMask & (~s.prevActiveMask[phy]);
    if (newBgBits || newPrevBits) {
        for (int i = start_sc; i < end_sc; i++) {
            uint64_t bit = (1ULL << i);
            if (newBgBits & bit) {
                s.staticBg[phy][i] = norm_amp[i];
            }
            if (newPrevBits & bit) {
                s.prevNorm[phy][i] = norm_amp[i];
            }
        }
        s.bgActiveMask[phy] |= activeMask;
        s.prevActiveMask[phy] |= activeMask;
    }

    // 5. Okamžitý rozdíl stejné PHY třídy: stepDiff + prostorový gradDiff
    uint64_t samePrevMask = activeMask & s.prevActiveMask[phy];
    float stepSum = 0.0f;
    float gradSum = 0.0f;
    int stepCnt = 0;
    int gradCnt = 0;

    for (int i = start_sc; i < end_sc; i++) {
        uint64_t bit = (1ULL << i);
        if (samePrevMask & bit) {
            stepSum += fabsf(norm_amp[i] - s.prevNorm[phy][i]);
            stepCnt++;
            if (i > start_sc && (samePrevMask & (1ULL << (i - 1)))) {
                float gradNow = norm_amp[i] - norm_amp[i - 1];
                float gradPrev = s.prevNorm[phy][i] - s.prevNorm[phy][i - 1];
                gradSum += fabsf(gradNow - gradPrev);
                gradCnt++;
            }
        }
        if (activeMask & bit) {
            s.prevNorm[phy][i] = norm_amp[i];
        }
    }
    s.prevActiveMask[phy] = activeMask;

    float stepDiff = (stepCnt > 0) ? (stepSum / (float)stepCnt) : 0.0f;
    float gradDiff = (gradCnt > 0) ? (gradSum / (float)gradCnt) : 0.0f;

    // 6. Uložení odchylky od per-PHY statického pozadí do sdíleného dynamického okna (W=8)
    uint64_t curBgValid = activeMask & s.bgActiveMask[phy];
    uint8_t latestWinIdx = s.windowHead;
    s.winValidMask[latestWinIdx] = curBgValid;
    for (int i = start_sc; i < end_sc; i++) {
        if (curBgValid & (1ULL << i)) {
            s.dynamicWindow[latestWinIdx][i] = norm_amp[i] - s.staticBg[phy][i];
        } else {
            s.dynamicWindow[latestWinIdx][i] = 0.0f;
        }
    }
    s.windowHead = (s.windowHead + 1) % CSI_WINDOW_SIZE;
    if (s.windowCount < CSI_WINDOW_SIZE) {
        s.windowCount++;
    }

    // 7. Výpočet dynStd (s odčítáním průměru okna -> odolné vůči statickému posunu) a bgDev
    float dynStd = 0.0f;
    float bgDev = 0.0f;
    int validSc = 0;

    if (s.windowCount >= 3) {
        uint8_t nWin = s.windowCount;
        uint64_t winMask = curBgValid;
        for (uint8_t w = 0; w < nWin; w++) {
            winMask &= s.winValidMask[w];
        }

        const float invW = 1.0f / (float)nWin;
        float totalStd = 0.0f;
        float devSqSum = 0.0f;

        for (int i = start_sc; i < end_sc; i++) {
            if (!(winMask & (1ULL << i))) continue;

            float mean = 0.0f;
            for (uint8_t w = 0; w < nWin; w++) {
                mean += s.dynamicWindow[w][i];
            }
            mean *= invW;

            float var = 0.0f;
            for (uint8_t w = 0; w < nWin; w++) {
                float d = s.dynamicWindow[w][i] - mean;
                var += d * d;
            }
            var *= invW;

            totalStd += sqrtf(var);
            float latestDev = s.dynamicWindow[latestWinIdx][i];
            devSqSum += latestDev * latestDev;
            validSc++;
        }

        if (validSc >= 12) {
            dynStd = totalStd / (float)validSc;
            bgDev = sqrtf(devSqSum / (float)validSc);
        }
    }

    float dynOsc = (0.65f * dynStd) + (0.35f * stepDiff);

    // 8. dt-řízená adaptace statického pozadí (zamezí trvalému zablokování po zastavení osoby)
    float tauBg = 3.00f;
    if (calibrating || s.autoWarmupLeft > 0 || s.phyWarmupCount[phy] < 15) {
        tauBg = 0.60f;
    } else if (s.calibReady) {
        float dynOscNorm = 1.0f + ((dynOsc > s.oscMu) ? ((dynOsc - s.oscMu) / s.oscNoiseScale) : 0.0f);
        bool bgHigh = (bgDev > (s.bgMu + 1.5f * s.bgNoiseScale));
        if (dynOscNorm > 1.80f) {
            tauBg = 2.50f; // Aktivní oscilace -> zachová pomalý pohyb
        } else if (bgHigh || s.motionDetected) {
            tauBg = 0.70f; // Osoba se zastavila -> pohltí statický stín do ~2.1 s
        } else {
            tauBg = 3.00f; // Klidový stav
        }
    } else {
        tauBg = 1.50f;
    }

    float alphaBg = 1.0f - expf(-dt / tauBg);
    for (int i = start_sc; i < end_sc; i++) {
        if (curBgValid & (1ULL << i)) {
            s.staticBg[phy][i] += alphaBg * (norm_amp[i] - s.staticBg[phy][i]);
        }
    }

    // 9. Kombinovaná surová metrika + akumulátor dávek pro Web/UI smyčku
    if (s.windowCount >= 3 && validSc >= 12) {
        float rawMetric = (0.45f * dynStd) + (0.25f * stepDiff) + (0.15f * gradDiff) + (0.15f * bgDev);

        // Mírné omezení extrémní 1-paketové korupce bez zpoždění reálného pohybu
        float stepCap = s.hasLastRaw ? (s.lastRawMetric * 2.5f + 0.60f) : 12.0f;
        float cappedRaw = (rawMetric > stepCap) ? stepCap : rawMetric;
        s.lastRawMetric = cappedRaw;
        s.hasLastRaw = true;

        s.pendingRawSum += cappedRaw;
        if (s.pendingCount == 0 || cappedRaw > s.pendingRawMax) {
            s.pendingRawMax = cappedRaw;
        }
        s.pendingOscSum += dynOsc;
        s.pendingBgSum += bgDev;
        s.pendingCount++;
        s.csiPacketCount++;
    }

    s.lastCsiPacketMs = now;
    portEXIT_CRITICAL(&csiMux);
}

void WiFiCsiManager::update() {
    if (!running || pausedForSleep) return;

    unsigned long now = millis();
    syncStations(now);

    int activeIdx[CSI_MAX_STATIONS];
    uint8_t activeCount = 0;
    getOrderedActiveIndices(activeIdx, activeCount);

    // Udržitelné round-robin sondování (~55 ms pro 1 mobil, rozprostřené >= 25 ms/slot pro více mobilů)
    if (activeCount > 0) {
        unsigned long probeInterval = (activeCount == 1) ? 55UL : max(25UL, 55UL / activeCount);
        sendRoundRobinProbe(now, probeInterval, true);
    }

    bool calibJustFinished = false;
    if (calibrating && (now - calibStartMs >= calibDurationMs)) {
        calibJustFinished = true;
    }

    for (uint8_t i = 0; i < activeCount; i++) {
        CsiStationSlot &s = slots[activeIdx[i]];

        uint16_t burstCount = 0;
        float burstRawSum = 0.0f;
        float burstRawMax = 0.0f;
        float burstOscSum = 0.0f;
        float burstBgSum = 0.0f;
        unsigned long lastPktMs = 0;

        portENTER_CRITICAL(&csiMux);
        burstCount = s.pendingCount;
        burstRawSum = s.pendingRawSum;
        burstRawMax = s.pendingRawMax;
        burstOscSum = s.pendingOscSum;
        burstBgSum = s.pendingBgSum;
        lastPktMs = s.lastCsiPacketMs;

        s.pendingCount = 0;
        s.pendingRawSum = 0.0f;
        s.pendingRawMax = 0.0f;
        s.pendingOscSum = 0.0f;
        s.pendingBgSum = 0.0f;
        portEXIT_CRITICAL(&csiMux);

        bool hasFreshPkt = (burstCount > 0);
        unsigned long silenceMs = (lastPktMs == 0) ? 9999UL : (now - lastPktMs);

        // Při výpadku paketů pozvolný dt-řízený pokles ke klidové hodnotě 1.0
        if (!hasFreshPkt) {
            if (lastPktMs == 0 || silenceMs > CSI_DECAY_START_MS) {
                float dtDecay = (s.lastEnvMs > 0 && now > s.lastEnvMs)
                    ? ((float)(now - s.lastEnvMs) * 0.001f)
                    : 0.01f;
                if (dtDecay >= 0.02f) {
                    dtDecay = constrain(dtDecay, 0.01f, 0.50f);
                    s.lastEnvMs = now;
                    float tauDecay = (silenceMs > CSI_STALE_PACKET_MS) ? 0.35f : 0.80f;
                    float alphaDecay = 1.0f - expf(-dtDecay / tauDecay);
                    s.smoothedMetric += alphaDecay * (1.0f - s.smoothedMetric);
                }

                if (silenceMs > CSI_STALE_PACKET_MS) {
                    portENTER_CRITICAL(&csiMux);
                    s.motionDetected = false;
                    portEXIT_CRITICAL(&csiMux);
                    s.enterCondStartMs = 0;
                    s.exitCondStartMs = 0;
                } else if (s.motionDetected && s.smoothedMetric < SNR_EXIT_TH) {
                    if (s.exitCondStartMs == 0) {
                        s.exitCondStartMs = now;
                    } else if (now - s.exitCondStartMs >= CSI_EXIT_HOLD_MS) {
                        portENTER_CRITICAL(&csiMux);
                        s.motionDetected = false;
                        portEXIT_CRITICAL(&csiMux);
                        s.exitCondStartMs = 0;
                        s.enterCondStartMs = 0;
                    }
                }
            }
            continue;
        }

        float pktRaw = burstRawSum / (float)burstCount;
        if (burstCount > 1) {
            pktRaw = (0.65f * pktRaw) + (0.35f * burstRawMax);
        }
        float pktOsc = burstOscSum / (float)burstCount;
        float pktBg  = burstBgSum / (float)burstCount;

        if (calibrating || s.autoWarmupLeft > 0) {
            s.calibSamples++;
            double delta = (double)pktRaw - s.calibMean;
            s.calibMean += delta / (double)s.calibSamples;
            double delta2 = (double)pktRaw - s.calibMean;
            s.calibM2 += delta * delta2;

            double oDelta = (double)pktOsc - s.calibOscMean;
            s.calibOscMean += oDelta / (double)s.calibSamples;
            double oDelta2 = (double)pktOsc - s.calibOscMean;
            s.calibOscM2 += oDelta * oDelta2;

            double bDelta = (double)pktBg - s.calibBgMean;
            s.calibBgMean += bDelta / (double)s.calibSamples;

            s.smoothedMetric = (s.smoothedMetric * 0.82f) + (1.0f * 0.18f);
            s.lastEnvMs = now;
            s.enterCondStartMs = 0;
            s.exitCondStartMs = 0;

            if (s.motionDetected) {
                portENTER_CRITICAL(&csiMux);
                s.motionDetected = false;
                portEXIT_CRITICAL(&csiMux);
            }

            if (!calibrating && s.autoWarmupLeft > 0) {
                s.autoWarmupLeft--;
                if (s.autoWarmupLeft == 0) {
                    finalizeSlotThreshold(s, false);
                }
            }
            continue;
        }

        // Normalizace do univerzální SNR škály (1.0 = klid, 2.4 = práh detekce)
        float mu = s.calibReady ? s.baselineNoise : (float)s.calibMean;
        float nScale = s.calibReady ? s.noiseScale : max(0.25f * mu, 0.08f);
        float dev = pktRaw - mu;
        float instSnr = 1.0f + ((dev > 0.0f) ? (dev / nScale) : 0.0f);
        instSnr = constrain(instSnr, 1.0f, 8.0f);

        float dt = (s.lastEnvMs > 0 && now > s.lastEnvMs)
            ? ((float)(now - s.lastEnvMs) * 0.001f)
            : 0.05f;
        dt = constrain(dt, 0.005f, 0.50f);
        s.lastEnvMs = now;

        constexpr float TAU_ATT = 0.12f;
        constexpr float TAU_REL = 0.35f;
        float tau = (instSnr > s.smoothedMetric) ? TAU_ATT : TAU_REL;
        float alphaEnv = 1.0f - expf(-dt / tau);
        s.smoothedMetric += alphaEnv * (instSnr - s.smoothedMetric);

        bool nextMotion = s.motionDetected;
        if (!s.motionDetected) {
            if (s.smoothedMetric >= SNR_INSTANT_TH) {
                nextMotion = true;
                s.enterCondStartMs = 0;
                s.exitCondStartMs = 0;
            } else if (s.smoothedMetric >= SNR_ENTER_TH) {
                if (s.enterCondStartMs == 0) {
                    s.enterCondStartMs = now;
                } else if (now - s.enterCondStartMs >= CSI_ENTER_HOLD_MS) {
                    nextMotion = true;
                    s.enterCondStartMs = 0;
                    s.exitCondStartMs = 0;
                }
            } else {
                s.enterCondStartMs = 0;
            }
        } else {
            if (s.smoothedMetric < SNR_EXIT_TH) {
                if (s.exitCondStartMs == 0) {
                    s.exitCondStartMs = now;
                } else if (now - s.exitCondStartMs >= CSI_EXIT_HOLD_MS) {
                    nextMotion = false;
                    s.exitCondStartMs = 0;
                    s.enterCondStartMs = 0;
                }
            } else {
                s.exitCondStartMs = 0;
            }
        }

        if (nextMotion != s.motionDetected) {
            portENTER_CRITICAL(&csiMux);
            s.motionDetected = nextMotion;
            portEXIT_CRITICAL(&csiMux);
        }
    }

    if (calibJustFinished) {
        finishGlobalCalibration();
    }

    if (activeCount == 0) {
        smoothedMetric = (smoothedMetric * 0.85f) + (1.0f * 0.15f);
        motionDetected = false;
        detectionThreshold = SNR_ENTER_TH;
        baselineNoise = 1.0f;
        return;
    }

    int domIdx = activeIdx[0];
    float bestMetric = -1.0f;
    bool anyMotion = false;

    for (uint8_t i = 0; i < activeCount; i++) {
        const CsiStationSlot &s = slots[activeIdx[i]];
        if (s.motionDetected) anyMotion = true;
        if (s.smoothedMetric > bestMetric) {
            bestMetric = s.smoothedMetric;
            domIdx = activeIdx[i];
        }
    }

    smoothedMetric = slots[domIdx].smoothedMetric;
    detectionThreshold = slots[domIdx].detectionThreshold;
    baselineNoise = 1.0f;
    motionDetected = anyMotion;
}
