#include "GraphicsManager.h"

// Definice pinů a displeje pro spodní panel (z tvých starých kódů)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

// Globální instance
GraphicsManager gfx;

GraphicsManager::GraphicsManager() 
    : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET) {}

bool GraphicsManager::init() {
    Serial.println("[GFX] Inicializace GraphicsManager (OLED)...");
    
    // SSD1306_SWITCHCAPVCC generuje 3.3V pro displej interně
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("[CHYBA] OLED SSD1306 alokace selhala!"));
        return false;
    }
    
    // Vymaže logo Adafruit, které je tam z továrny
    display.clearDisplay();
    display.display();
    
    return true;
}

void GraphicsManager::clearScreen(uint16_t color) {
    display.fillScreen(color == 0 ? SSD1306_BLACK : SSD1306_WHITE);
}

void GraphicsManager::drawTextPartial(int x, int y, String text, uint16_t fgColor, uint16_t bgColor, uint8_t size) {
    // Pro OLED používáme 0 pro černou a 1 pro bílou.
    display.setTextColor(fgColor == 0 ? SSD1306_BLACK : SSD1306_WHITE, 
                         bgColor == 0 ? SSD1306_BLACK : SSD1306_WHITE);
    display.setTextSize(size);
    display.setCursor(x, y);
    display.print(text);
}

void GraphicsManager::fillRect(int x, int y, int w, int h, uint16_t color) {
    display.fillRect(x, y, w, h, color == 0 ? SSD1306_BLACK : SSD1306_WHITE);
}

void GraphicsManager::drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
    display.drawLine(x0, y0, x1, y1, color == 0 ? SSD1306_BLACK : SSD1306_WHITE);
}

void GraphicsManager::drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
    display.drawTriangle(x0, y0, x1, y1, x2, y2, color == 0 ? SSD1306_BLACK : SSD1306_WHITE);
}

void GraphicsManager::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
    display.fillTriangle(x0, y0, x1, y1, x2, y2, color == 0 ? SSD1306_BLACK : SSD1306_WHITE);
}

void GraphicsManager::drawCircle(int x, int y, int r, uint16_t color) {
    display.drawCircle(x, y, r, color == 0 ? SSD1306_BLACK : SSD1306_WHITE);
}

void GraphicsManager::fillCircle(int x, int y, int r, uint16_t color) {
    display.fillCircle(x, y, r, color == 0 ? SSD1306_BLACK : SSD1306_WHITE);
}

void GraphicsManager::drawRoundRect(int x, int y, int w, int h, int radius, uint16_t color) {
    display.drawRoundRect(x, y, w, h, radius, color == 0 ? SSD1306_BLACK : SSD1306_WHITE);
}

void GraphicsManager::fillRoundRect(int x, int y, int w, int h, int radius, uint16_t color) {
    display.fillRoundRect(x, y, w, h, radius, color == 0 ? SSD1306_BLACK : SSD1306_WHITE);
}

void GraphicsManager::drawBitmap(int x, int y, const uint8_t* bitmap, int w, int h, uint16_t color) {
    // Kreslení černobílých (1-bit) obrázků, např. ikon pro menu
    display.drawBitmap(x, y, bitmap, w, h, color == 0 ? SSD1306_BLACK : SSD1306_WHITE);
}

void GraphicsManager::pushToScreen() {
    // Tohle vezme ten 1KB blok paměti z RAM ESP32 a pošle ho po I2C do displeje
    display.display();
}
