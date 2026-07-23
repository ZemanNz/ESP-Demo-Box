# Horní senzorový panel – ESP32-S3 Firmware
**Maturitní projekt | ESP32-S3 DevKitC (44 pinů) | PlatformIO + Arduino Framework**

Tento projekt obsahuje plně modulární, produkčně laděný firmware pro **Horní senzorový panel** předváděcího demo boxu. Všechny moduly jsou řízeny přes konfigurační makra (`#define ENABLE_*`) v souboru `include/config.h` a běží v neblokující smyčce `loop()` s přesným časováním `millis()`.

---

## 🚀 Stav vývoje a testování
> **Dnešní stav (23. 7. 2026):**  
> Všechny moduly byly odzkoušeny, zkompilovány a **úspěšně nahrány a ověřeny na reálném hardwaru ESP32-S3 DevKitC**. Systém běží stabilně, bez pádu procesoru i bez zásahů Task Watchdog Timeru (`TG1WDT_SYS_RST`).

---

## 🛠️ Funkcionalita a součinnost modulů

1. **Barevná interakce (TCS34725 ➔ WS2812B LED pásek)**:
   - Senzor **TCS34725** načítá RGB složky přiloženého objektu.
   - Pásek **WS2812B (8 RGB LED)** dynamicky zrcadlí naměřenou barvu v reálném čase.
2. **Duální I2C sběrnice (Vyřešení kolize adres 0x29)**:
   - `I2C_0` (Wire, SDA: 8, SCL: 9): Řídí **LSM6DS3** (0x6A), **LCD1602** (0x27) a **VL53L0X** (0x29).
   - `I2C_1` (Wire1, SDA: 5, SCL: 6): Samostatná sběrnice pro **TCS34725** (0x29) zamezuje konfliktům adres.
3. **Měření vzdálenosti a překážek**:
   - **VL53L0X**: Přesné laserové měření vzdálenosti v milimetrech přes I2C_0.
   - **HC-SR04**: Ultrazvukové měření vzdálenosti v centimetrech (TRIG = GPIO 4, ECHO = GPIO 18).
   - **IR Senzor 1**: Digitální detekce překážek na GPIO 38 (LOW = detekce).
4. **Zobrazení dat**:
   - **2.8" TFT ST7789** (SPI): Střídá 3 grafické obrazovky (Senzory, Vzdálenost, Uptime).
   - **LCD 1602 I2C**: Zobrazuje aktuální teplotu a vlhkost ze senzoru DHT11.
   - **74HC595 Sedmisegment**: Bit-banging čítač 0–999 na 3 číslicích (DATA = 15, CLK = 16, LATCH = 17).
5. **Orientační senzor**:
   - **LSM6DS3**: Gyroskop a akcelerometr s automatickou klidovou kalibrací offsetu při startu (výpočet Roll, Pitch, Yaw).
6. **Zvuková a vizuální odezva**:
   - **Pasivní bzučák**: Zvuková znělka při startu + krátké pípnutí každých 5 sekund.
   - **DHT11**: Teplotní a vlhkostní senzor na GPIO 3.

---

## 📌 Kompletní mapa zapojení pinů (ESP32-S3)

| Zařízení / Modul | Signál | ESP32-S3 GPIO | Poznámka |
| :--- | :--- | :--- | :--- |
| **DHT11** | DATA | **GPIO 3** | Teplota & Vlhkost (4k7 pull-up) |
| **HC-SR04** | TRIG | **GPIO 4** | Ultrazvuk Trigger |
| **HC-SR04** | ECHO | **GPIO 18** | Ultrazvuk Echo *(mimo USB piny)* |
| **IR Senzor 1** | OUT | **GPIO 38** | Detekce překážky *(mimo Octal Flash)* |
| **WS2812B** | DATA | **GPIO 42** | 8 RGB LED *(Adafruit_NeoPixel)* |
| **Bzučák** | Vstup | **GPIO 7** | Pasivní bzučák |
| **74HC595** | DS (DATA) | **GPIO 15** | Sedmisegmentová sériová data |
| **74HC595** | SH_CP (CLK)| **GPIO 16** | Sedmisegmentové hodiny |
| **74HC595** | ST_CP (LATCH)| **GPIO 17** | Sedmisegmentový Latch |
| **TFT 2.8"** | CS | **GPIO 10** | Chip Select |
| **TFT 2.8"** | SDI (MOSI) | **GPIO 11** | Hardware SPI Data |
| **TFT 2.8"** | SCK | **GPIO 12** | Hardware SPI Clock |
| **TFT 2.8"** | DC | **GPIO 13** | Data / Command |
| **TFT 2.8"** | RESET | **GPIO 14** | Reset displeje |
| **TFT 2.8"** | LED | **GPIO 21** / 3.3V | Podsvícení |
| **I2C_0 (Wire)** | SDA / SCL | **GPIO 8 / GPIO 9** | LSM6DS3, LCD1602, VL53L0X |
| **I2C_1 (Wire1)**| SDA / SCL | **GPIO 5 / GPIO 6** | TCS34725 (0x29) |
| **Fotorezistory**| IN1 / IN2 | **GPIO 1 / GPIO 2** | Analogové fotorezistory |
| **Tlačítka** | BTN1 / BTN2 | **GPIO 47 / GPIO 48** | INPUT_PULLUP |

### ⚠️ Vyhrazené piny (Nepoužívat pro I/O!):
- **GPIO 19, 20**: Vnitřně vyhrazeno pro rozhraní **USB D- / D+**.
- **GPIO 33, 34, 35, 36, 37**: Vnitřně vyhrazeno pro paměťovou sběrnici **Octal Flash / OPI PSRAM** na ESP32-S3 N16R8.

---

## ⚙️ Kompilace a nahrání (PlatformIO)

 Sestavení projektu:
```bash
pio run -e demo_panel
```

 Nahrání do ESP32-S3:
```bash
pio run -e demo_panel -t upload --upload-port /dev/ttyUSB0
```

 Sériový monitor (115200 baud):
```bash
pio device monitor -b 115200
```
