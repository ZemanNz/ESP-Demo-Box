# Architektura: Rozdělení Master-Slave mezi dvěma mikrokontroléry ESP32

## Popis architektury
Projekt ESP-Demo-Box využívá pro řízení celého systému dva nezávislé mikrokontroléry:
* **Horní panel (Master):** Výkonnější čip **ESP32-S3 DevKitC** (dvoujádrový procesor Xtensa LX7 @ 240 MHz, podpora vektorových instrukcí, velká SRAM).
* **Dolní panel (Slave / I/O expandér):** Osvědčený čip **ESP32-WROOM** (dvoujádrový procesor Xtensa LX6 @ 240 MHz).

Tyto dvě jednotky jsou propojeny vysokorychlostní plně duplexní sériovou linkou UART (piny TX/RX) a tvoří distribuovaný vestavěný systém.

---

## Proč je rozdělení na Master a Slave výhodné?

Použití dvou spolupracujících mikrokontrolérů namísto jediného centralizovaného čipu přináší několik zásadních technických výhod:

### 1. Dostatek GPIO pinů pro všechny moduly
ESP-Demo-Box obsahuje dohromady více než 25 různých senzorů, akčních členů, displejů a tlačítek. Žádný běžný mikrokontrolér nemá dostatek vyvedených fyzických pinů, aby obsloužil:
* 2.8" SPI grafický displej ST7789,
* 0.96" I2C OLED displej, I2C LCD 1602, sedmisegmentový displej přes 74HC595,
* Dvě oddělená I2C rozhraní pro senzory (VL53L0X, LSM6DS3, TCS34725),
* Dva nezávislé 8-LED WS2812B pásky (vyžadující přesné časování RMT/DMA),
* Servomotory, DC motor s PWM, rotační enkodér, analogový joystick a tlačítka.

Rozdělením na dvě desky má každé ESP k dispozici dostatek dedikovaných GPIO pinů bez nutnosti složitých externích multiplexerů.

---

### 2. Oddělení výpočetně náročné grafiky od přesného generování signálů
* **Horní panel (Master):** Vykreslování her a animací v rozlišení $320 \times 240$ v 16bitových barvách (Double Buffering) alokuje 153,6 KB RAM a vytěžuje SPI sběrnici na vysokých frekvencích (40–80 MHz). K tomu běží Wi-Fi AP a asynchronní webový server.
* **Dolní panel (Slave):** Generuje přesné PWM pulzy pro servomotory (50 Hz), obsluhuje přerušení z rotačního enkodéru a čte ADC převodníky.

> **Výhoda:** Vykreslování grafiky nebo síťová komunikace na horním panelu **nikdy nezpůsobí zpoždění generování PWM pulzů pro serva**, což eliminuje jakékoliv chvění (*jitter*) motorů.

---

### 3. Logické rozdělení rolí a odpovědnosti

| Vlastnost | Horní panel (Master / Mozek) | Dolní panel (Slave / I/O podsystém) |
| :--- | :--- | :--- |
| **Čip** | ESP32-S3 | ESP32-WROOM |
| **Primární role** | Uživatelské rozhraní, logika, hry, web | Fyzické rozhraní, čtení vstupů, výkonové řízení |
| **Vstupy** | DHT, lasery, ultrazvuk, barvy, IMU | Joystick, 5 tlačítek, enkodér, potenciometr, 2 switche |
| **Výstupy** | 2.8" TFT displej, 3x LED, bzučák, 7-seg | Servomotory, DC motor, 0.96" OLED, spodní LED pásek |
| **Komunikace** | Wi-Fi WebServer + UART Master | UART Slave |

---

## Princip spolupráce po sběrnici UART

1. **Směr Dolní $\rightarrow$ Horní (Telemetrie):**  
   Dolní panel nepřetržitě čte své vstupy (joystick, tlačítka, přepínače) a posílá kompletní balíček `BottomToTopPacket` nahoru do mozku systému.
2. **Směr Horní $\rightarrow$ Dolní (Řídicí příkazy):**  
   Horní panel vyhodnocuje stav her a uživatelského menu. Při změně odešle balíček `TopToBottomPacket` s požadovanými úhly serv, rychlostí motoru, textem pro OLED nebo barvami LED pásku.

---

## Shrnutí pro text maturitní práce
Architektura Master-Slave představuje moderní modulární přístup k návrhu vestavěných systémů. Umožňuje izolovat výkonově náročné akční členy a mechanické ovládací prvky na dedikovaném periferním řadiči (Slave), zatímco hlavní procesor (Master) se plně soustředí na grafické rozhraní, herní algoritmy a síťovou konektivitu.
