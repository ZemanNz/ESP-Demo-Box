# Problém: Strategie UART přenosu – Asynchronní události vs. Periodický Heartbeat

## Popis problému
Při návrhu komunikace mezi dvěma mikrokontroléry je nutné vyřešit zásadní kompromis v časování přenosu dat:
1. **Událostní přístup (Event-Driven):** Odeslat data pouze tehdy, když dojde ke změně (např. stisknutí tlačítka nebo změna módu).
2. **Periodický přístup (Polling / Stream):** Odesílat data v pevných časových intervalech (např. každých 20 ms).

Oba tyto základní přístupy mají při samostatném použití vážné nevýhody.

---

## Porovnání základních přístupů

### Nevýhody čistě událostního přístupu (Event-Driven):
* **Riziko ztráty stavu při zarušení:** Pokud je paket odeslán pouze jednou při stisku tlačítka a na lince dojde k rušení (např. elektromagnetický impuls od motoru), paket se zahodí. Přijímač se o změně nikdy nedozví a systém zůstane v nekonzistentním stavu až do další uživatelské akce.
* **Neschopnost detekovat výpadek spojení:** Pokud se data neposílají, přijímač nepozná rozdíl mezi stavem „vše je v klidu“ a stavem „přerušil se komunikační kabel“.

### Nevýhody čistě periodického přístupu (Polling):
* **Zbytečná zátěž:** Odesílání desítek bajtů v rychlé smyčce, i když je zařízení zcela v klidu.
* **Vnesená latence:** Pokud se perioda nastaví příliš dlouhá (např. 100 ms kvůli šetření procesoru), uživatel pocítí zpoždění při ovládání tlačítek.

---

## Řešení: Hybridní komunikační model (Okamžitá změna + Záchranný Heartbeat)

V projektu ESP-Demo-Box byla navržena a implementována **hybridní strategie**, která kombinuje maximální rychlost odezvy s vysokou spolehlivostí.

### Princip fungování:
1. **Okamžitá reakce na změnu (Příznak `bottomNeedsTx`):**
   Kdykoliv aplikační kód (menu, hra, změna módu) změní jakýkoliv výstup pro dolní panel, vnitřní setter automaticky nastaví příznak `bottomNeedsTx = true`. Vlákno `Task_UART` tento příznak okamžitě zachytí a odešle paket **do 10 ms (okamžitá odezva bez postřehnutelného zpoždění)**.
2. **Periodický Heartbeat (200 ms):**
   Pokud je zařízení v klidu a po dobu 200 ms nedošlo k žádné změně, vyprší časovač a odešle se potvrzovací paket.

---

## Ukázka implementace ve FreeRTOS Tasku

```cpp
void Task_UART(void *pvParameters) {
    unsigned long lastSendTime = 0;
    TopToBottomPacket outPacket;

    for (;;) {
        unsigned long now = millis();

        // 1. Vyhodnocení obou podmínek pro odeslání
        bool hasChanged = globalState.popBottomNeedsTx(); // Změna stavu
        bool heartbeatTimeout = (now - lastSendTime >= 200); // 200ms timeout

        // Odesíláme POKUD: Nastala změna NEBO vypršel Heartbeat
        if (hasChanged || heartbeatTimeout) {
            lastSendTime = now; // Resetujeme časovač

            // Naplnění paketu aktuálními daty ze SystemState
            SensorData data = globalState.getSensorData();
            naplnPaket(outPacket, data);

            // Výpočet kontrolního součtu a odeslání
            outPacket.checksum = calculateChecksum((uint8_t*)&outPacket, sizeof(outPacket) - 2);
            SerialESP.write((const uint8_t*)&outPacket, sizeof(TopToBottomPacket));
        }

        // Krátká neblokující prodleva tasku
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

---

## Výhody hybridního modelu pro ESP-Demo-Box

1. **Nulová vnímatelná latence:** Uživatel stiskne tlačítko a příkaz odchází okamžitě v nejbližším taktu smyčky (do 10 ms).
2. **Samoozdravovací schopnost (Self-Healing):** Pokud by rušení poškodilo paket při okamžité změně, nejpozději za 200 ms dorazí Heartbeat paket, který stav na dolním panelu automaticky uvede do správného stavu.
3. **Nízké vytížení sběrnice:** Na lince 115 200 baud představuje 200ms Heartbeat vytížení sběrnice na méně než **1,5 % celkové kapacity**.
