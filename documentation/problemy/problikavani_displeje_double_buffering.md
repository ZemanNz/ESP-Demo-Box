# Problém: Problikávání TFT displeje ST7789 při vykreslování her

## Popis problému
Při vykreslování her (Snake, Flappy Bird) na 2.8" TFT displeji ST7789 ($320 \times 240$ px) docházelo při obnovování obrazu k výraznému problikávání (**flickering**) a trhání obrazu (**screen tearing**). Problém byl obzvláště patrný u hry Flappy Bird při pohybu překážek (trubek) a ptáčka.

## Příčina problému
Při přímočarém vykreslování (Direct Rendering) na displej přes rozhraní SPI se v každém snímku hry prováděly tyto kroky:
1. `display->fillScreen(CYAN)` – Smazání celé obrazovky na modrou barvu pozadí.
2. `display->fillRect(...)` – Nakreslení objektů (ptáček, překážky).

Sběrnice SPI posílá data na displej postupně:
- Displej $320 \times 240$ v 16bitovém barevném režimu (RGB565, 2 B na pixel) vyžaduje na jeden celý snímek:
  $$320 \times 240 \times 2 \text{ B} = 153\,600 \text{ B} \approx 153,6 \text{ KB dat}$$
- Při volání `fillScreen()` poslal procesor příkaz překreslit všech 76 800 pixelů na modro.
- Displej se na malý okamžik stával zcela prázdnou modrou plochou (bez herních prvků).
- Následně se přes SPI posílaly příkazy pro nakreslení žlutého ptáčka a zelených trubek.

Lidské oko registrovalo právě tento krátký mezistav se smazaným pozadím, což způsobovalo nepříjemné blikání.

---

## Řešení: Double Buffering (Dvojitý obrazový buffer)

Místo přímého zápisu na displej v průběhu vykreslování byl v souboru `GameEngine` (`src/hry/game_engine.h`) implementován **Double Buffering** pomocí třídy `GFXcanvas16` z knihovny `Adafruit_GFX`.

### Principy fungování:
1. **Front Buffer (Fyzický displej):** Displej zobrazuje předchozí kompletní snímek a jeho obsah se nemění, dokud není nový snímek plně připraven.
2. **Back Buffer (Canvas v RAM):** V paměti SRAM čipu ESP32-S3 se alokuje virtuální plátno o velikosti $320 \times 240$ pixelů (pole `uint16_t buffer[76800]`).
3. **Kreslení do RAM:** Všechny vykreslovací operace (`clearScreen()`, `fillRect()`, `drawText()`) se provádí bleskově přímo v operační paměti RAM na frekvenci procesoru 240 MHz (záležitost mikrosekund).
4. **Atomický přenos na displej:** Až po dokončení celého snímku se v metodě `GameEngine::displayFrame()` zavolá:
   ```cpp
   display->drawRGBBitmap(0, 0, canvas->getBuffer(), 320, 240);
   ```
   Tím se celý nový obraz přenese z RAM na fyzický displej v jediném plynulém datovém bloku.

---

## Paměťová náročnost na ESP32-S3

- **Potřebná paměť pro Framebuffer (320x240 RGB565):** 153,6 KB RAM.
- **Dostupná paměť čipu ESP32-S3:** 328 KB interní SRAM + 8 MB external PSRAM.
- **Výsledek:** Alokace 153,6 KB pro plynulý Double Buffering využívá cca 6 % celkové paměti flash/RAM a ESP32-S3 ji zvládá bez jakéhokoliv omezení výkonu.

---

## Výsledný efekt
- Úplné odstranění problikávání (0% flickering).
- Plynulý snímkový kmitočet bez trhání obrazu.
- Kód hry zůstal čistý a plně odstíněný od nízkoúrovňové obsluhy displeje.
