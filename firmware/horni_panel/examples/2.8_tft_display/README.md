# Grafické příklady: 2.8" TFT LCD displej (ILI9341 / ST7789)

Tato složka obsahuje ukázkové příklady pro rozběhnutí a testování **2.8" TFT LCD displeje** (rozlišení 240×320, řadič ILI9341 kompatibilní se ST7789) zapojeného přes hardwarové SPI na mikrokontroléru **ESP32-S3 DevKitC-1**.

---

## Propojení (Zapojení pinů)

Displej je pevně připojen k ESP32-S3 pomocí následujícího hardwarového mapování:

| Displej Pin | ESP32-S3 GPIO | Popis |
| ----------- | ------------- | ----- |
| **VCC**     | `5V` (VEXT)   | Napájení displeje |
| **GND**     | `GND`         | Společná zem |
| **CS**      | `GPIO 10`     | Chip Select (výběr SPI čipu) |
| **RESET**   | `GPIO 14`     | Reset displeje |
| **DC**      | `GPIO 13`     | Data/Command (výběr dat/příkazu) |
| **SDI (MOSI)**| `GPIO 11`   | Hardware SPI Master Out Slave In |
| **SCK**     | `GPIO 12`     | Hardware SPI Clock (hodinový signál) |
| **LED**     | `3.3V`        | Podsvícení displeje (napojeno trvale) |
| **SDO (MISO)**| *Nezapojeno* | Master In Slave Out (dotyková vrstva se neřeší) |

---

## Přehled příkladů v této složce

Ve složce naleznete tyto ukázkové zdrojové kódy:

1. **[main.cpp](main.cpp)**:
   * **Základní test displeje**: Vykreslí statický barevný testovací obrazec, zarovnané textové popisky a ohraničující linky. Ideální pro první ověření správného zapojení pinů a komunikace.
2. **[dashboard.cpp](dashboard.cpp)**:
   * **Prémiový systémový panel**: Zobrazuje simulované metriky systému (Uptime, volná paměť RAM, teplota CPU), animovaný progress bar, blikající stavovou LED diodu a především **živý, plynulý osciloskop** kreslící neonově zelenou vlnovou křivku bez blikání displeje.
3. **[3d_engine.cpp](3d_engine.cpp)**:
   * **3D Projekční engine**: Matematicky počítá a perspektivně promítá **dvě drátěné kostky** rotující v opačných směrech ve všech 3 osách. Na pozadí se pohybuje 3D hvězdné pole s jasem závislým na hloubce hvězdy. Celé zobrazení je doplněno futuristickým HUD rámem s reálným čítačem FPS (~20 FPS) a úhlů rotace.
4. **[multi_view.cpp](multi_view.cpp)**:
   * **Multi-View manažer (3 obrazovky)**: Spojuje Dashboard, 3D Engine a **retro hru Had (Snake autoplay)** do jednoho běžícího celku. Každých 5 sekund provede **laserový přechodový efekt** (tyrkysový paprsek zamete obrazovku) a přepne zobrazení na další v pořadí.
   * *Autoplay Had*: Had se sám naviguje za jablkem, zvětšuje se, připočítává skóre a při zablokování se automaticky resetuje.

---

## Jak spustit jakýkoliv příklad

1. Propojte displej s ESP32-S3 přesně podle tabulky zapojení výše.
2. Vyberte si ukázkový soubor (např. `multi_view.cpp`).
3. Zkopírujte celý jeho obsah.
4. Otevřete hlavní soubor projektu **[src/main.cpp](../../src/main.cpp)** a přepište jej zkopírovaným obsahem.
5. V PlatformIO spusťte nahrávání (**Build and Upload**).
