#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/**
 * @brief Globální správce grafiky pro spodní panel (OLED displej)
 */
class GraphicsManager {
public:
    GraphicsManager();
    
    // Inicializace displeje na spodním panelu
    bool init();

    // -------------------------------------------------------------
    // Společné rozhraní (aby se to programovalo stejně jako nahoře)
    // -------------------------------------------------------------
    void clearScreen(uint16_t color = 0); // 0 = BLACK (zhasnuto)

    // Překreslování textu. U OLED to kreslí do interního RAM bufferu.
    void drawTextPartial(int x, int y, String text, uint16_t fgColor, uint16_t bgColor, uint8_t size = 1);
    void fillRect(int x, int y, int w, int h, uint16_t color);

    // -------------------------------------------------------------
    // Pokročilé grafické tvary (Moderní UI)
    // -------------------------------------------------------------
    // Čáry pod jakýmkoliv úhlem
    void drawLine(int x0, int y0, int x1, int y1, uint16_t color);
    
    // Trojúhelníky
    void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color);
    void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color);

    void drawCircle(int x, int y, int r, uint16_t color);
    void fillCircle(int x, int y, int r, uint16_t color);
    
    void drawRoundRect(int x, int y, int w, int h, int radius, uint16_t color);
    void fillRoundRect(int x, int y, int w, int h, int radius, uint16_t color);

    // -------------------------------------------------------------
    // Kreslení obrázků / Ikon
    // -------------------------------------------------------------
    // Pro OLED používáme jednobarevné (monochromatické) bitmapy (XBM nebo PROGMEM array)
    void drawBitmap(int x, int y, const uint8_t* bitmap, int w, int h, uint16_t color);

    // Na spodním panelu (OLED SSD1306) se všechna grafika VŽDY drží 
    // v jeho interní paměti (1 KB). Až když zavoláš pushToScreen(), 
    // pošle se to přes I2C na sklo. Nepotřebujeme tu GFXcanvas16.
    void pushToScreen();

    // Přístup k čistému objektu pro pokročilé GFX funkce
    Adafruit_SSD1306& getDisplay() { return display; }

private:
    Adafruit_SSD1306 display;
};

// Přístup ke globální instanci
extern GraphicsManager gfx;

/*
================================================================================
  PŘÍKLADY POUŽITÍ (CHEATSHEET PRO OLED)
================================================================================

1. RYCHLÉ PŘEKRESLENÍ ČÍSLA ZE SENZORU
   // Nakreslí text a pozadí. Vždy musíš zavolat pushToScreen!
   // (U OLED používáme barvy 1 pro bílou a 0 pro černou)
   gfx.drawTextPartial(10, 10, "22.5", 1, 0, 2);
   gfx.pushToScreen();

2. VYTVOŘENÍ MODERNÍHO TLAČÍTKA
   gfx.fillRoundRect(10, 20, 60, 20, 5, 1);
   // Černý text (0) na bílém tlačítku (1)
   gfx.drawTextPartial(15, 25, "OK", 0, 1, 1); 
   gfx.pushToScreen();

3. TVORBA IKONY / BÍLOČERNÉHO OBRÁZKU
   // Obrazky musí být 1-bit monochromatické pole (vygenerované z image-to-cpp)
   extern const uint8_t ikonkaWifi[] PROGMEM;
   gfx.drawBitmap(100, 0, ikonkaWifi, 16, 16, 1);
   gfx.pushToScreen();

================================================================================
*/
