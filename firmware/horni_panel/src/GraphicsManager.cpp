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

void GraphicsManager::drawRect(int x, int y, int w, int h, uint16_t color) {
    tft.drawRect(x, y, w, h, color);
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

void GraphicsManager::drawRadialGauge(int cx, int cy, int rIn, int rOut, float percent, uint16_t activeColor, uint16_t bgColor) {
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 1.0f) percent = 1.0f;
    float maxAngle = percent * 360.0f;
    for (int deg = 0; deg < 360; deg += 6) {
        float rad = (deg - 90) * 0.0174532925f; // Převod na radiány (-90 = nahoře)
        float cosA = cos(rad);
        float sinA = sin(rad);
        uint16_t c = (deg <= maxAngle) ? activeColor : bgColor;
        
        int x1 = cx + (int)(rIn * cosA);
        int y1 = cy + (int)(rIn * sinA);
        int x2 = cx + (int)(rOut * cosA);
        int y2 = cy + (int)(rOut * sinA);
        tft.drawLine(x1, y1, x2, y2, c);
    }
}

void GraphicsManager::drawPacmanGauge(int cx, int cy, int radius, float percent, uint16_t activeColor, uint16_t bgColor) {
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 1.0f) percent = 1.0f;

    // 1. Podkladový kruh
    tft.fillCircle(cx, cy, radius, bgColor);

    // 2. Zaplnění koláčového výseku (Pac-man tělo)
    int maxAngle = (int)(percent * 360.0f);
    if (maxAngle > 0) {
        for (int deg = 0; deg <= maxAngle; deg += 3) {
            float rad = (deg - 90) * 0.0174532925f; // Začátek nahoře na 12 hodinách (-90 deg)
            int x = cx + (int)(radius * cos(rad));
            int y = cy + (int)(radius * sin(rad));
            tft.drawLine(cx, cy, x, y, activeColor);
        }
    }

    // 3. Černý obrys
    tft.drawCircle(cx, cy, radius, ST77XX_BLACK);
}

void GraphicsManager::drawWifiIcon(int cx, int cy, uint16_t color, uint16_t bgColor) {
    tft.fillCircle(cx, cy, 3, color);
    tft.drawCircle(cx, cy, 10, color);
    tft.drawCircle(cx, cy, 11, color);
    tft.drawCircle(cx, cy, 18, color);
    tft.drawCircle(cx, cy, 19, color);
    tft.fillRect(cx - 24, cy + 1, 48, 18, bgColor); // Odříznutí spodku vln
}

void GraphicsManager::drawThickLine(int x0, int y0, int x1, int y1, int thickness, uint16_t color) {
    if (thickness <= 1) {
        tft.drawLine(x0, y0, x1, y1, color);
        return;
    }
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float nx = -dy / len * (thickness / 2.0f);
    float ny = dx / len * (thickness / 2.0f);
    
    int p1x = x0 + (int)nx, p1y = y0 + (int)ny;
    int p2x = x0 - (int)nx, p2y = y0 - (int)ny;
    int p3x = x1 - (int)nx, p3y = y1 - (int)ny;
    int p4x = x1 + (int)nx, p4y = y1 + (int)ny;
    
    tft.fillTriangle(p1x, p1y, p2x, p2y, p3x, p3y, color);
    tft.fillTriangle(p1x, p1y, p3x, p3y, p4x, p4y, color);
}

void GraphicsManager::drawArrow(int x0, int y0, int x1, int y1, int headSize, uint16_t color) {
    drawThickLine(x0, y0, x1, y1, 6, color);
    
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;
    
    float ux = dx / len;
    float uy = dy / len;
    
    float nx = -uy;
    float ny = ux;
    
    float bx = x1 - ux * headSize;
    float by = y1 - uy * headSize;
    
    int a1x = x1, a1y = y1;
    int a2x = (int)(bx + nx * (headSize * 0.8f)), a2y = (int)(by + ny * (headSize * 0.8f));
    int a3x = (int)(bx - nx * (headSize * 0.8f)), a3y = (int)(by - ny * (headSize * 0.8f));
    
    tft.fillTriangle(a1x, a1y, a2x, a2y, a3x, a3y, color);
}

void GraphicsManager::drawColorWheel(int cx, int cy, int radius) {
    // 1. Plný bílý podklad, aby barva pozadí nikde neprosvítala
    tft.fillCircle(cx, cy, radius, ST77XX_WHITE);

    // 2. Vykreslíme plné barevné trojúhelníkové výseče (100% krytí)
    float prevRad = 0.0f;
    int prevX = cx + radius;
    int prevY = cy;

    for (int deg = 2; deg <= 360; deg += 2) {
        float rad = deg * 0.0174532925f;
        float h = deg / 60.0f;
        int i = (int)h;
        float f = h - i;
        uint8_t q = (uint8_t)(255 * (1.0f - f));
        uint8_t t = (uint8_t)(255 * f);
        uint8_t r = 0, g = 0, b = 0;
        switch (i % 6) {
            case 0: r = 255; g = t;   b = 0;   break;
            case 1: r = q;   g = 255; b = 0;   break;
            case 2: r = 0;   g = 255; b = t;   break;
            case 3: r = 0;   g = q;   b = 255; break;
            case 4: r = t;   g = 0;   b = 255; break;
            case 5: r = 255; g = 0;   b = q;   break;
        }
        uint16_t col = tft.color565(r, g, b);
        int curX = cx + (int)(radius * cos(rad));
        int curY = cy + (int)(radius * sin(rad));

        tft.fillTriangle(cx, cy, prevX, prevY, curX, curY, col);

        prevX = curX;
        prevY = curY;
    }

    // 3. Čistý černý obrys
    tft.drawCircle(cx, cy, radius, ST77XX_BLACK);
}

// 29x29 QR kód pro okamžité Wi-Fi připojení: WIFI:S:ESP-Demo-Box;T:nopass;;
static const uint32_t QR_CODE_DATA[29] PROGMEM = {
    0x1FCEE77F, // Radek 0
    0x1046F641, // Radek 1
    0x1753AD5D, // Radek 2
    0x15576355, // Radek 3
    0x1759355D, // Radek 4
    0x10520E41, // Radek 5
    0x1FD5557F, // Radek 6
    0x00145D00, // Radek 7
    0x17C81B7C, // Radek 8
    0x14B5E529, // Radek 9
    0x0FD37A45, // Radek 10
    0x0224ADC1, // Radek 11
    0x0155693F, // Radek 12
    0x1335372B, // Radek 13
    0x075E08C4, // Radek 14
    0x193054C8, // Radek 15
    0x05721AD8, // Radek 16
    0x1B2BE2AB, // Radek 17
    0x13E77A1A, // Radek 18
    0x1282A7B1, // Radek 19
    0x12FA69F3, // Radek 20
    0x00173318, // Radek 21
    0x1FC20B58, // Radek 22
    0x105A571F, // Radek 23
    0x175315F4, // Radek 24
    0x15596C46, // Radek 25
    0x175CB812, // Radek 26
    0x104A6F5B, // Radek 27
    0x1FD923C2  // Radek 28
};

void GraphicsManager::drawQRCode(int startX, int startY, int moduleSize) {
    int qrPixelSize = 29 * moduleSize;
    int quietZone = 6;
    
    // Bílý podklad a černý obrys
    tft.fillRect(startX - quietZone, startY - quietZone, qrPixelSize + 2 * quietZone, qrPixelSize + 2 * quietZone, ST77XX_WHITE);
    tft.drawRect(startX - quietZone - 1, startY - quietZone - 1, qrPixelSize + 2 * quietZone + 2, qrPixelSize + 2 * quietZone + 2, ST77XX_BLACK);

    // Vykreslení černých modulů
    for (int r = 0; r < 29; r++) {
        uint32_t rowData = pgm_read_dword(&QR_CODE_DATA[r]);
        for (int c = 0; c < 29; c++) {
            if ((rowData >> (28 - c)) & 0x01) {
                tft.fillRect(startX + c * moduleSize, startY + r * moduleSize, moduleSize, moduleSize, ST77XX_BLACK);
            }
        }
    }
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
