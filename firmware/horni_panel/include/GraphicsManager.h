#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#define ST77XX_DARKGREY 0x4208

/**
 * @brief Globální správce grafiky pro horní panel (TFT displej)
 */
class GraphicsManager {
public:
    GraphicsManager();
    
    // Inicializace displeje (zavolá se v HardwareSetup)
    bool init();

    // -------------------------------------------------------------
    // METODA 1: Částečné překreslování (Partial Redraw)
    // - Super rychlé, nepotřebuje paměť navíc
    // - Ideální pro čísla ze senzorů (přemaže jen konkrétní místo)
    // -------------------------------------------------------------
    void clearScreen(uint16_t color = 0x0000); // 0x0000 = BLACK
    void drawTextPartial(int x, int y, String text, uint16_t fgColor, uint16_t bgColor, uint8_t size = 1);
    void fillRect(int x, int y, int w, int h, uint16_t color);

    // -------------------------------------------------------------
    // Pokročilé grafické tvary (Moderní UI)
    // -------------------------------------------------------------
    // Čáry pod jakýmkoliv úhlem (úhlopříčky)
    void drawLine(int x0, int y0, int x1, int y1, uint16_t color);
    
    // Trojúhelníky (skvělé pro šipky a ukazatele)
    void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color);
    void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color);

    void drawCircle(int x, int y, int r, uint16_t color);
    void fillCircle(int x, int y, int r, uint16_t color);
    
    // Zaoblené obdélníky (krásné pro moderní tlačítka a widgety)
    void drawRoundRect(int x, int y, int w, int h, int radius, uint16_t color);
    void fillRoundRect(int x, int y, int w, int h, int radius, uint16_t color);

    // -------------------------------------------------------------
    // Kreslení obrázků / Fotek
    // -------------------------------------------------------------
    // data: Pole pixelů vygenerované z obrázku (např. přes ImageConverter)
    void drawImage(int x, int y, const uint16_t* data, int w, int h);

    // -------------------------------------------------------------
    // METODA 2: Kompletní Double-Buffering přes PSRAM
    // - Ideální pro hry a plynulé animace celého displeje
    // -------------------------------------------------------------
    GFXcanvas16* getCanvas();
    void pushCanvasToScreen(); // Odešle snímek z paměti na TFT

    // Přístup k čistému tft objektu pro pokročilé volání
    Adafruit_ST7789& getTFT() { return tft; }

private:
    Adafruit_ST7789 tft;
    GFXcanvas16* canvas; // Pointer na paměťové plátno (150 KB)
};

// Přístup ke globální instanci
extern GraphicsManager gfx;

/*
================================================================================
  PŘÍKLADY POUŽITÍ (CHEATSHEET)
================================================================================

1. RYCHLÉ PŘEKRESLENÍ ČÍSLA ZE SENZORU (Bez blikání, šetří PSRAM)
   // Nakreslí bílý text na černém pozadí. Černé pozadí automaticky smaže starý text!
   gfx.drawTextPartial(10, 10, "22.5 C", ST77XX_WHITE, ST77XX_BLACK, 2);

2. VYTVOŘENÍ MODERNÍHO TLAČÍTKA S TEXTEM
   gfx.fillRoundRect(50, 50, 120, 40, 10, ST77XX_BLUE);
   // Text umístíme na modré pozadí
   gfx.drawTextPartial(65, 60, "START", ST77XX_WHITE, ST77XX_BLUE, 2);

3. TVORBA HRY/PLYNULÉ ANIMACE (Double Buffering s PSRAM)
   GFXcanvas16* platno = gfx.getCanvas();
   platno->fillScreen(ST77XX_BLACK); // Vyčistí paměť
   platno->fillCircle(x, y, 10, ST77XX_RED); // Nakreslí míček do paměti
   gfx.pushCanvasToScreen(); // Jednorázově a plynule přenese vše na displej

4. OBRÁZKY (PNG/JPG převedené do C-array kódu)
   // Získáš z online konvertoru: image to c array
   extern const uint16_t mojeFotka[] PROGMEM;
   gfx.drawImage(0, 0, mojeFotka, 240, 320);

5. ÚHLOPŘÍČKY A ŠIPKY
   // Šipka nahoru (zelená)
   gfx.fillTriangle(120, 20, 110, 40, 130, 40, ST77XX_GREEN);
================================================================================
*/
