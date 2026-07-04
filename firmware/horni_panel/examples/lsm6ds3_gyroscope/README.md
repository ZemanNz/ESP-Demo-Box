# Příklad: Inerciální senzor LSM6DS3 (Gyroskop, Akcelerometr & Pokročilé Funkce)

Tento příklad demonstruje kompletní vyčítání a zpracování dat ze 6-osého inerciálního senzoru (IMU) **LSM6DS3** připojeného k **ESP32-S3** přes hardwarovou sběrnici I2C0.

Složka obsahuje jak hlavní stabilní program s **komplementárním filtrem**, tak samostatné ukázky demonstrující **pokročilé hardwarové koprocesory** senzoru LSM6DS3 (krokoměr, detekce poklepání, volný pád a 6D orientace v prostoru).

---

## 1. Hlavní program: Komplementární filtr (`main.cpp`)

V hlavní ukázce (`main.cpp`) je implementován **komplementární filtr** (Complementary Filter), který kombinuje data z gyroskopu a akcelerometru a řeší tak zásadní fyzikální nedostatek inerciálních systémů – **integrační drift gyroskopu**.

### Jak komplementární filtr funguje:
1. **Gyroskop** měří úhlovou rychlost. Je velmi rychlý a přesný v krátkém čase, ale v čase driftuje (chyba se neustále kumuluje integrací $\theta = \int \omega \, dt$).
2. **Akcelerometr** měří gravitační zrychlení. Dlouhodobě ukazuje, kde je stabilní směr dolů k zemi, ale krátkodobě je velmi náchylný na jakékoliv otřesy.
3. **Filtr** tyto informace spojuje do rovnice (s váhou $\alpha = 0.98$):
   $$\theta_{nový} = \alpha \cdot (\theta_{starý} + \omega_{gyro} \cdot dt) + (1 - \alpha) \cdot \theta_{accel}$$
   * **Roll (osa X)** a **Pitch (osa Y)** jsou díky tomu naprosto stabilní, nedriftují a při navrácení do původní polohy ukazují přesně $0^\circ$.
   * **Yaw (osa Z)** je rovnoběžný s gravitačním vektorem, proto jej nelze akcelerometrem korigovat a využívá čistou numerickou integraci gyroskopu.

---

## 2. Pokročilé příklady (Advanced Hardware Examples)

Tyto přiložené soubory demonstrují výpočty probíhající přímo v hardwarovém koprocesoru senzoru LSM6DS3:

*   **`pedometer.cpp` (Krokoměr)**:
    *   Ukazuje spuštění integrovaného krokoměru a čtení počtu kroků z registru `readPedometer()` bez softwarového filtrování v ESP32.
*   **`tap_detection.cpp` (Detekce poklepání)**:
    *   Provádí přímou konfiguraci I2C registrů (`TAP_CFG`, `TAP_THS_6D`, `INT_DUR2`, `WAKEUP_THS`) pro detekci **jednoduchého a dvojitého poklepání** (Single & Double Tap) na senzor v libovolné ose.
*   **`free_fall.cpp` (Detekce volného pádu)**:
    *   Nastavuje registry pro detekci stavu bez tíže (když celkové zrychlení klesne pod definovaný práh po stanovenou dobu) a vyvolá poplach.
*   **`orientation_6d.cpp` (6D Orientace v prostoru)**:
    *   Sleduje naklonění senzoru vůči gravitaci a detekuje 6 poloh: lícem nahoru, lícem dolů, levý/pravý bok, otočení na výšku (Portrait / Landscape).

---

## Propojení (Zapojení)

Senzor LSM6DS3 je připojen na I2C0 sběrnici ESP32-S3:

| LSM6DS3 Pin | ESP32-S3 Pin | Popis |
| ----------- | ------------ | ----- |
| **VCC / VIN** | `3V3`        | Napájení senzoru (3.3 V) |
| **GND**     | `GND`        | Společná zem |
| **SDA**     | `GPIO 8`     | I2C Datová linka (SDA) |
| **SCL**     | `GPIO 9`     | I2C Hodinová linka (SCL) |

---

## Jak spustit příklady

1. Propojte senzor LSM6DS3 podle tabulky.
2. Zkopírujte kód vybraného příkladu (např. `main.cpp` nebo `tap_detection.cpp`) a nahraďte jím obsah hlavního souboru `src/main.cpp` v projektu.
3. Sestavte a nahrajte projekt do desky (**Build & Upload**).
4. Otevřete sériový monitor na rychlosti **115200 baudů**.
