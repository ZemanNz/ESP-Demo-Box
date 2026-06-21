# Interaktivní příklad: OLED displej & Rotační enkodér

Tento příklad spojuje **0.96" OLED displej (SSD1306)** a **rotační enkodér (např. KY-040)** do jednoho interaktivního celku. 

## Popis chování
* **Otáčení enkodéru**: Mění hodnotu počítadla od **0 do 100** (zobrazeno jako velké číslo uprostřed a na spodním progress baru). Otáčení je zpracováno efektivně a bez zadrhávání pomocí hardwarových přerušení (interrupts).
* **Stisk tlačítka enkodéru**: Přepíná "zvýrazněný/svítící" stav čísla. V tomto stavu se barva čísla invertuje a kolem něj pulzuje animovaný světelný efekt (záření).

## Propojení (Zapojení)

### 1. Zapojení OLED Displeje (I2C)

| OLED Pin | ESP32-S3 DevKit Pin | Popis |
| -------- | ------------------- | ----- |
| **VCC**  | `3V3`               | Napájení displeje (3.3 V) |
| **GND**  | `GND`               | Společná zem |
| **SDA**  | `GPIO 8`            | I2C Datová linka |
| **SCL**  | `GPIO 9`            | I2C Hodinová linka |

---

### 2. Zapojení Rotačního Enkodéru

| Enkodér Pin | ESP32-S3 DevKit Pin | Popis |
| ----------- | ------------------- | ----- |
| **CLK** (Out A) | `GPIO 18`           | Detekce otáčení (generuje přerušení) |
| **DT**  (Out B) | `GPIO 17`           | Určení směru otáčení |
| **SW**  (Switch)| `GPIO 16`           | Integrované tlačítko (generuje přerušení) |
| **+**   (VCC)   | `3V3`               | Napájení enkodéru |
| **GND**         | `GND`               | Společná zem |

*Poznámka: Tlačítko a rotační piny využívají v kódu interní pull-up odpory ESP32 (`INPUT_PULLUP`), takže není nutné zapojovat žádné externí rezistory.*

---

## Princip fungování kódu
1. **Hardwarové přerušení (Interrupts)**: CLK pin a SW pin jsou navázány na funkce přerušení pomocí `attachInterrupt()`. To znamená, že mikrokontrolér okamžitě reaguje na změnu polohy enkodéru nebo stisk tlačítka nezávisle na tom, co zrovna dělá v hlavní smyčce `loop()`.
2. **Debouncing (Ošetření zákmitů)**: U tlačítka (SW) je v kódu softwarová ochrana proti zákmitům kontaktů (debouncing) – ignorují se stisky v intervalu kratším než 250 ms.
3. **Grafické rozvržení**: 
   - Dynamické centrování čísla podle počtu cifer (1, 2 nebo 3 cifry).
   - Animace záření na základě inkrementace proměnné `glowOffset` a vykreslování zvětšujících se zaoblených obdélníků (`drawRoundRect`).

---

## Jak spustit tento příklad
1. Zapojte OLED a enkodér podle tabulek výše.
2. Zkopírujte obsah souboru `main.cpp` ze složky tohoto příkladu a nahraďte jím kód v `src/main.cpp`.
3. V PlatformIO sestavte a nahrajte projekt (**Build & Upload**).
