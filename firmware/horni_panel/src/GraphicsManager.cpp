#include "GraphicsManager.h"
#include "config.h"

// Globální instance
GraphicsManager gfx;

GraphicsManager::GraphicsManager() 
    : tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST), canvas(nullptr) {}

bool GraphicsManager::init() {
    Serial.println("[GFX] Inicializace GraphicsManager (TFT)...");
    
    // Základní HW piny pro podsvícení
    pinMode(PIN_TFT_LED, OUTPUT);
    digitalWrite(PIN_TFT_LED, HIGH);

    // Inicializace ovladače
    tft.init(TFT_W, TFT_H);
    tft.invertDisplay(false);
    tft.setRotation(1); // Na šířku
    tft.fillScreen(ST77XX_BLACK);
    
    return true;
}

void GraphicsManager::clearScreen(uint16_t color) {
    tft.fillScreen(color);
}

void GraphicsManager::drawTextPartial(int x, int y, String text, uint16_t fgColor, uint16_t bgColor, uint8_t size) {
    // Trik: Když zadáme textColor a zároveň bgColor, Adafruit automaticky
    // překresluje i pozadí za písmenem. Staré číslo tedy beze zbytku zmizí 
    // a displej vůbec neproblikne.
    tft.setTextColor(fgColor, bgColor);
    tft.setTextSize(size);
    tft.setCursor(x, y);
    tft.print(text);
}

void GraphicsManager::fillRect(int x, int y, int w, int h, uint16_t color) {
    tft.fillRect(x, y, w, h, color);
}

void GraphicsManager::drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
    tft.drawLine(x0, y0, x1, y1, color);
}

void GraphicsManager::drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
    tft.drawTriangle(x0, y0, x1, y1, x2, y2, color);
}

void GraphicsManager::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, color);
}

void GraphicsManager::drawCircle(int x, int y, int r, uint16_t color) {
    tft.drawCircle(x, y, r, color);
}

void GraphicsManager::fillCircle(int x, int y, int r, uint16_t color) {
    tft.fillCircle(x, y, r, color);
}

void GraphicsManager::drawRoundRect(int x, int y, int w, int h, int radius, uint16_t color) {
    tft.drawRoundRect(x, y, w, h, radius, color);
}

void GraphicsManager::fillRoundRect(int x, int y, int w, int h, int radius, uint16_t color) {
    tft.fillRoundRect(x, y, w, h, radius, color);
}

void GraphicsManager::drawImage(int x, int y, const uint16_t* data, int w, int h) {
    // Adafruit GFX má funkci drawRGBBitmap, která bere pole pixelů a pošle je na displej
    tft.drawRGBBitmap(x, y, data, w, h);
}

GFXcanvas16* GraphicsManager::getCanvas() {
    if (canvas == nullptr) {
        Serial.println("[GFX] Alokuji 150KB Canvas do PSRAM pro Double Buffering...");
        // Na ESP32-S3 s povolenou PSRAM (v platformio.ini) se toto
        // automaticky nalokuje do obrovské externí PSRAM paměti.
        // POZOR: Displej je rotovaný na šířku, takže plátno musí mít 
        // šířku TFT_H (320) a výšku TFT_W (240)!
        canvas = new GFXcanvas16(TFT_H, TFT_W); 
    }
    return canvas;
}

void GraphicsManager::pushCanvasToScreen() {
    if (canvas != nullptr) {
        // Zkopíruje celou paměť plátna přímo na fyzický displej
        // Zápis proběhne přes SPI extrémně rychle (cca 30 FPS)
        tft.drawRGBBitmap(0, 0, canvas->getBuffer(), canvas->width(), canvas->height());
    }
}
