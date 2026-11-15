#include "GraphicsManager.h"
#include "config.h"
#include <math.h>

namespace {

// -----------------------------------------------------------------------------
// Interní barvy a vykreslovací helpery pro Světelný mód (classic_arc, 320x240)
// -----------------------------------------------------------------------------
constexpr float kLightPi = 3.14159265f;

struct LightRgb8 {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

constexpr uint16_t lightRgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (uint16_t)(b >> 3);
}

inline float clampFloat(float v, float lo, float hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

uint16_t lerpLightRgb565(LightRgb8 a, LightRgb8 b, float t) {
    float u = clampFloat(t, 0.0f, 1.0f);
    uint8_t r  = (uint8_t)lroundf((float)a.r + ((float)b.r - (float)a.r) * u);
    uint8_t g  = (uint8_t)lroundf((float)a.g + ((float)b.g - (float)a.g) * u);
    uint8_t bl = (uint8_t)lroundf((float)a.b + ((float)b.b - (float)a.b) * u);
    return lightRgb565(r, g, bl);
}

constexpr uint16_t kColLightBg          = lightRgb565(0x06, 0x0e, 0x17);
constexpr uint16_t kColLightCardBg      = lightRgb565(0x0a, 0x18, 0x28);
constexpr uint16_t kColLightCardBorder  = lightRgb565(0x1a, 0x38, 0x52);
constexpr uint16_t kColLightTrackBg     = lightRgb565(0x0c, 0x1c, 0x2e);
constexpr uint16_t kColLightTrackGroove = lightRgb565(0x13, 0x2a, 0x42);
constexpr uint16_t kColLightTrackTick   = lightRgb565(0x2e, 0x55, 0x78);
constexpr uint16_t kColLightCyan        = lightRgb565(0x00, 0xe5, 0xff);
constexpr uint16_t kColLightYellow      = lightRgb565(0xfc, 0xe8, 0x03);
constexpr uint16_t kColLightGreen       = lightRgb565(0x00, 0xff, 0x66);
constexpr uint16_t kColLightMuted       = lightRgb565(0x8b, 0x94, 0x9e);
constexpr uint16_t kColLightWhite       = lightRgb565(0xff, 0xff, 0xff);
constexpr uint16_t kColLightBgDark      = lightRgb565(0x04, 0x0a, 0x12);
constexpr uint16_t kColLightBarBorder   = lightRgb565(0x1d, 0x39, 0x52);
constexpr uint16_t kColLightTickDark    = lightRgb565(0x18, 0x32, 0x4a);

// Barvy pro INFO overlay (scrollovatelné tělo + lišta)
constexpr uint16_t kColInfoTrackBg      = 0x2104;
constexpr uint16_t kColInfoArrowDim     = 0x3186;
constexpr uint16_t kColInfoDivider      = 0x528A;

uint16_t lightPhotoGlowColor(float pct) {
    float t = clampFloat(pct / 100.0f, 0.0f, 1.0f);
    if (t < 0.35f) {
        return lerpLightRgb565({0x13, 0x22, 0x33}, {0x1f, 0x6f, 0x8b}, t / 0.35f);
    }
    if (t < 0.75f) {
        return lerpLightRgb565({0x1f, 0x6f, 0x8b}, {0xf5, 0x9e, 0x0b}, (t - 0.35f) / 0.40f);
    }
    return lerpLightRgb565({0xf5, 0x9e, 0x0b}, {0xff, 0xf3, 0xa1}, (t - 0.75f) / 0.25f);
}

uint16_t lightSunCoreColor(float brightnessPct) {
    float t = clampFloat(brightnessPct / 100.0f, 0.0f, 1.0f);
    if (t < 0.5f) {
        return lerpLightRgb565({0x8a, 0x46, 0x00}, {0xff, 0xbe, 0x0b}, t / 0.5f);
    }
    return lerpLightRgb565({0xff, 0xbe, 0x0b}, {0xff, 0xf8, 0xc9}, (t - 0.5f) / 0.5f);
}

void canvasDrawText(GFXcanvas16* c, int x, int y, const char* text, uint16_t color, uint8_t size, int align = -1) {
    if (!c || !text) return;
    c->setTextSize(size);
    c->setTextWrap(false);
    c->setTextColor(color);
    int drawX = x;
    if (align >= 0) {
        int16_t x1 = 0, y1 = 0;
        uint16_t w = 0, h = 0;
        c->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
        drawX = (align == 0) ? (x - (int)(w / 2)) : (x - (int)w);
    }
    c->setCursor(drawX, y);
    c->print(text);
}

void canvasDrawThickLine(GFXcanvas16* c, int x0, int y0, int x1, int y1, int thickness, uint16_t color) {
    if (thickness <= 1) {
        c->drawLine(x0, y0, x1, y1, color);
        return;
    }
    float dx = (float)(x1 - x0);
    float dy = (float)(y1 - y0);
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float nx = (-dy / len) * (thickness * 0.5f);
    float ny = (dx / len) * (thickness * 0.5f);

    int p1x = (int)lroundf((float)x0 + nx), p1y = (int)lroundf((float)y0 + ny);
    int p2x = (int)lroundf((float)x0 - nx), p2y = (int)lroundf((float)y0 - ny);
    int p3x = (int)lroundf((float)x1 - nx), p3y = (int)lroundf((float)y1 - ny);
    int p4x = (int)lroundf((float)x1 + nx), p4y = (int)lroundf((float)y1 + ny);

    c->fillTriangle(p1x, p1y, p2x, p2y, p3x, p3y, color);
    c->fillTriangle(p1x, p1y, p3x, p3y, p4x, p4y, color);
}

void canvasDrawArcPoly(GFXcanvas16* c, int cx, int cy, int r, float startRad, float endRad, int steps, uint16_t color, int thickness) {
    int n = max(4, steps);
    int prevX = cx + (int)lroundf((float)r * cosf(startRad));
    int prevY = cy + (int)lroundf((float)r * sinf(startRad));
    for (int i = 1; i <= n; i++) {
        float a = startRad + ((endRad - startRad) * (float)i) / (float)n;
        int nx = cx + (int)lroundf((float)r * cosf(a));
        int ny = cy + (int)lroundf((float)r * sinf(a));
        canvasDrawThickLine(c, prevX, prevY, nx, ny, thickness, color);
        prevX = nx;
        prevY = ny;
    }
}

void canvasDrawVerticalMeter(GFXcanvas16* c, int x, int y, int w, int h, float pct, uint16_t fillCol) {
    c->fillRoundRect(x, y, w, h, 2, kColLightBgDark);
    c->drawRoundRect(x, y, w, h, 2, kColLightCardBorder);
    int innerH = h - 4;
    int fillH = (int)lroundf((clampFloat(pct, 0.0f, 100.0f) / 100.0f) * (float)innerH);
    if (fillH > 0) {
        c->fillRect(x + 2, y + h - 2 - fillH, w - 4, fillH, fillCol);
    }
    for (int q = 1; q <= 3; q++) {
        int ty = y + (int)lroundf((float)(h * q) / 4.0f);
        c->drawLine(x + 2, ty, x + w - 3, ty, kColLightTickDark);
    }
}

void canvasDrawHorizontalBar(GFXcanvas16* c, int x, int y, int w, int h, float pct, uint16_t fillCol) {
    c->fillRect(x, y, w, h, kColLightBgDark);
    c->drawRect(x, y, w, h, kColLightBarBorder);
    int fillW = (int)lroundf((clampFloat(pct, 0.0f, 100.0f) / 100.0f) * (float)(w - 2));
    if (fillW > 0) {
        c->fillRect(x + 1, y + 1, fillW, h - 2, fillCol);
    }
}

void canvasDrawSun(GFXcanvas16* c, float sx, float sy, float brightnessPct, unsigned long timeMs) {
    int cx = (int)lroundf(sx);
    int cy = (int)lroundf(sy);
    float b = clampFloat(brightnessPct / 100.0f, 0.0f, 1.0f);

    if (b <= 0.04f) {
        c->fillCircle(cx, cy, 6, lightRgb565(0x13, 0x22, 0x38));
        c->drawCircle(cx, cy, 6, lightRgb565(0x50, 0x72, 0x96));
        c->fillCircle(cx - 1, cy - 1, 2, lightRgb565(0x6b, 0x8e, 0xb5));
        return;
    }

    int coreR = (int)lroundf(6.0f + b * 5.0f);
    uint16_t outerHaloCol = lerpLightRgb565({0x06, 0x0e, 0x17}, {0xff, 0x91, 0x00}, 0.25f + 0.35f * b);
    uint16_t innerHaloCol = lerpLightRgb565({0x2b, 0x1a, 0x00}, {0xff, 0xd0, 0x00}, 0.45f + 0.55f * b);
    uint16_t rayCol       = lerpLightRgb565({0x99, 0x62, 0x00}, {0xff, 0xe6, 0x6d}, b);

    c->fillCircle(cx, cy, coreR + 5, outerHaloCol);
    c->fillCircle(cx, cy, coreR + 2, innerHaloCol);

    float rot = fmodf((float)timeMs * 0.0008f, 2.0f * kLightPi);
    float rStart = (float)(coreR + 2);
    float rEnd = (float)(coreR + 3 + (int)lroundf(b * 3.0f));
    for (int i = 0; i < 8; i++) {
        float ang = rot + ((float)i * kLightPi) / 4.0f;
        int x0 = cx + (int)lroundf(rStart * cosf(ang));
        int y0 = cy + (int)lroundf(rStart * sinf(ang));
        int x1 = cx + (int)lroundf(rEnd * cosf(ang));
        int y1 = cy + (int)lroundf(rEnd * sinf(ang));
        c->drawLine(x0, y0, x1, y1, rayCol);
    }

    c->fillCircle(cx, cy, coreR, lightSunCoreColor(brightnessPct));
    c->drawCircle(cx, cy, coreR, lightRgb565(0xff, 0xf3, 0xb0));
    int innerDotR = max(2, (int)lroundf((float)coreR * 0.45f));
    c->fillCircle(cx, cy, innerDotR, lightRgb565(0xff, 0xfc, 0xe6));
}

void canvasDrawPhotoDot(GFXcanvas16* c, int cx, int cy, char letter, float pct, bool isDominant) {
    float t = clampFloat(pct / 100.0f, 0.0f, 1.0f);
    if (t > 0.08f) {
        c->fillCircle(cx, cy, 19, lerpLightRgb565({0x06, 0x0e, 0x17}, {0xff, 0xbe, 0x0b}, t * 0.42f));
        c->fillCircle(cx, cy, 16, lerpLightRgb565({0x06, 0x0e, 0x17}, {0x00, 0xe5, 0xff}, t * 0.55f));
    } else {
        c->drawCircle(cx, cy, 17, lightRgb565(0x16, 0x29, 0x3d));
    }

    c->fillCircle(cx, cy, 14, lightPhotoGlowColor(pct));
    uint16_t ringCol = (isDominant && t > 0.08f)
        ? kColLightCyan
        : (t > 0.72f ? kColLightYellow : lightRgb565(0x3d, 0x5c, 0x78));
    c->drawCircle(cx, cy, 14, ringCol);
    c->drawCircle(cx, cy, 13, ringCol);

    uint16_t txtCol = (t > 0.56f) ? kColLightBg : kColLightWhite;
    const char label[2] = {letter, '\0'};
    canvasDrawText(c, cx, cy - 7, label, txtCol, 2, 0);
}

void canvasDrawLightRay(GFXcanvas16* c, float sunX, float sunY, int targetX, int targetY, float pctVal, float brightnessPct) {
    float dx = (float)targetX - sunX;
    float dy = (float)targetY - sunY;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 36.0f) return;

    float ux = dx / dist;
    float uy = dy / dist;
    float startD = 14.0f + (brightnessPct / 100.0f) * 4.0f;
    float endD = dist - 20.0f;
    if (endD <= startD) return;

    int x0 = (int)lroundf(sunX + ux * startD);
    int y0 = (int)lroundf(sunY + uy * startD);
    int x1 = (int)lroundf(sunX + ux * endD);
    int y1 = (int)lroundf(sunY + uy * endD);
    float strength = clampFloat(pctVal / 100.0f, 0.0f, 1.0f);
    uint16_t rayCol = lerpLightRgb565({0x13, 0x28, 0x3d}, {0xff, 0xe0, 0x66}, strength);
    int width = (strength > 0.7f) ? 2 : 1;
    canvasDrawThickLine(c, x0, y0, x1, y1, (strength > 0.35f) ? width : 1, rayCol);
}

void canvasDrawLightCardBase(GFXcanvas16* c, int x, int w, const char* title,
                             uint16_t titleCol, uint16_t borderCol,
                             const char* valueText, uint16_t valCol) {
    const int y = 198;
    const int h = 39;
    const int centerX = x + w / 2;
    c->fillRoundRect(x, y, w, h, 4, kColLightCardBg);
    c->drawRoundRect(x, y, w, h, 4, borderCol);
    canvasDrawText(c, centerX, 202, title, titleCol, 1, 0);
    canvasDrawText(c, centerX, 214, valueText, valCol, 1, 0);
}

void drawInfoTextRow(GFXcanvas16* c, int x, int y, const InfoLine& line) {
    c->setTextSize(2);
    c->setTextWrap(false);
    if (line.tag != nullptr && line.tag[0] != '\0') {
        c->setTextColor(ST77XX_CYAN, ST77XX_BLACK);
        c->setCursor(x, y);
        c->print(line.tag);
        if (line.body != nullptr && line.body[0] != '\0') {
            int bodyX = x + (static_cast<int>(strlen(line.tag)) + 1) * 12;
            c->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
            c->setCursor(bodyX, y);
            c->print(line.body);
        }
    } else if (line.body != nullptr) {
        c->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        c->setCursor(x, y);
        c->print(line.body);
    }
}

} // namespace

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

    // 2. Naprosto plné, 100% neprůhledné vyplnění koláčového výseku (vyplněné trojúhelníky)
    int maxAngle = (int)(percent * 360.0f);
    if (maxAngle >= 360) {
        tft.fillCircle(cx, cy, radius, activeColor);
    } else if (maxAngle > 0) {
        float prevRad = -90.0f * 0.0174532925f;
        int prevX = cx + (int)(radius * cos(prevRad));
        int prevY = cy + (int)(radius * sin(prevRad));
        for (int deg = 1; deg <= maxAngle; deg++) {
            float rad = (deg - 90) * 0.0174532925f;
            int currX = cx + (int)(radius * cos(rad));
            int currY = cy + (int)(radius * sin(rad));
            tft.fillTriangle(cx, cy, prevX, prevY, currX, currY, activeColor);
            prevX = currX;
            prevY = currY;
        }
    }

    // 3. Černý obrys
    tft.drawCircle(cx, cy, radius, ST77XX_BLACK);
}

void GraphicsManager::drawWifiIcon(int cx, int cy, uint16_t color, uint16_t bgColor) {
    tft.fillCircle(cx, cy, 2, color);
    tft.drawCircle(cx, cy, 6, color);
    tft.drawCircle(cx, cy, 7, color);
    tft.drawCircle(cx, cy, 11, color);
    tft.drawCircle(cx, cy, 12, color);
    tft.fillRect(cx - 15, cy + 1, 30, 15, bgColor); // Odříznutí spodku vln
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

void GraphicsManager::drawColorWheel(int cx, int cy, int radius, bool drawBorder) {
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

    // 3. Volitelný čistý černý obrys
    if (drawBorder) {
        tft.drawCircle(cx, cy, radius, ST77XX_BLACK);
    }
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

void GraphicsManager::drawLightMode(uint16_t rawPhotoL, uint16_t rawPhotoR,
                                    float smoothL, float smoothR,
                                    unsigned long nowMs, bool redrawFallback) {
    GFXcanvas16* c = getCanvas();
    if (!c) {
        if (redrawFallback) {
            clearScreen(ST77XX_BLACK);
            drawTextPartial(20, 100, "SVETELNY MOD (CHYBI PSRAM)", ST77XX_YELLOW, ST77XX_BLACK, 1);
        }
        return;
    }

    // Zesílení úhlové citlivosti pro ploché fotorezistory v pokojovém osvětlení (-10 deg @ 45 deg -> ~-45 deg)
    constexpr float kLightAngleGain = 4.5f;

    float sL = clampFloat(smoothL, 0.0f, 4095.0f);
    float sR = clampFloat(smoothR, 0.0f, 4095.0f);
    float pctL = clampFloat((sL / 4095.0f) * 100.0f, 0.0f, 100.0f);
    float pctR = clampFloat((sR / 4095.0f) * 100.0f, 0.0f, 100.0f);
    float brightnessPct = max(pctL, pctR);
    float rawRatio = (sR - sL) / (sL + sR + 120.0f);
    float normAngle = clampFloat(rawRatio * kLightAngleGain, -1.0f, 1.0f);
    int angleDeg = constrain((int)lroundf(normAngle * 90.0f), -90, 90);

    const char* dirText = "VYVAZENO";
    uint16_t dirCol = kColLightGreen;
    if (brightnessPct < 3.0f && abs(angleDeg) <= 5) {
        dirText = "TMA (0%)";
        dirCol = kColLightMuted;
    } else if (angleDeg < -5) {
        dirText = "<<< VLEVO";
        dirCol = kColLightCyan;
    } else if (angleDeg > 5) {
        dirText = "VPRAVO >>>";
        dirCol = kColLightYellow;
    }

    c->fillScreen(kColLightBg);

    struct SideCfg {
        char letter;
        const char* label;
        int meterX;
        int meterPctX;
        int dotX;
        int cardX;
        float pct;
        uint16_t rawAdc;
        bool dominant;
    };
    const int dotY = 38;
    const SideCfg sides[2] = {
        {'L', "FOTO L", 8,   15,  62,  4,   pctL, rawPhotoL, pctL > pctR + 3.0f},
        {'P', "FOTO P", 299, 305, 258, 240, pctR, rawPhotoR, pctR > pctL + 3.0f}
    };

    char buf[20];
    for (int i = 0; i < 2; i++) {
        canvasDrawVerticalMeter(c, sides[i].meterX, 10, 13, 52, sides[i].pct, lightPhotoGlowColor(sides[i].pct));
        snprintf(buf, sizeof(buf), "%d%%", (int)lroundf(sides[i].pct));
        canvasDrawText(c, sides[i].meterPctX, 66, buf, kColLightMuted, 1, 0);
        canvasDrawText(c, sides[i].dotX, 4, sides[i].label, kColLightCyan, 1, 0);
    }

    // Středová vyrovnávací čára a výrazný indikátor směru
    canvasDrawText(c, 160, 5, dirText, dirCol, 2, 0);
    c->fillRect(92, 36, 136, 4, kColLightTrackBg);
    c->drawRect(92, 36, 136, 4, lightRgb565(0x1f, 0x3f, 0x5c));
    c->drawLine(160, 30, 160, 46, kColLightTrackTick);
    int beadX = 160 + (int)lroundf(normAngle * 62.0f);
    c->fillCircle(beadX, 38, 5, dirCol);
    c->drawCircle(beadX, 38, 5, kColLightWhite);

    // Zvětšený otevřený spodní půlkruh využívající prostor po zrušené horní liště
    const int arcCx = 160;
    const int arcCy = 80;
    const int arcR = 92;
    float theta = (kLightPi * 0.5f) * (1.0f - normAngle);
    float sunX = (float)arcCx + (float)arcR * cosf(theta);
    float sunY = (float)arcCy + (float)arcR * sinf(theta);

    canvasDrawArcPoly(c, arcCx, arcCy, 56, kLightPi, 0.0f, 32, lightRgb565(0x0c, 0x1d, 0x30), 1);
    for (int deg = -90; deg <= 90; deg += 30) {
        float tAng = (kLightPi * 0.5f) * (1.0f - (float)deg / 90.0f);
        int innerR = (deg % 90 == 0 || deg == 0) ? (arcR + 4) : (arcR + 5);
        int outerR = (deg % 90 == 0 || deg == 0) ? (arcR + 10) : (arcR + 8);
        c->drawLine(
            arcCx + (int)lroundf((float)innerR * cosf(tAng)),
            arcCy + (int)lroundf((float)innerR * sinf(tAng)),
            arcCx + (int)lroundf((float)outerR * cosf(tAng)),
            arcCy + (int)lroundf((float)outerR * sinf(tAng)),
            (deg == 0) ? kColLightGreen : kColLightTrackTick
        );
    }
    canvasDrawText(c, 49, arcCy - 3, "-90", kColLightMuted, 1, 1);
    canvasDrawText(c, 271, arcCy - 3, "+90", kColLightMuted, 1, -1);

    canvasDrawArcPoly(c, arcCx, arcCy, arcR, kLightPi, 0.0f, 48, kColLightTrackGroove, 4);
    canvasDrawArcPoly(c, arcCx, arcCy, arcR, kLightPi, 0.0f, 48, kColLightTrackTick, 1);
    if (fabsf(normAngle) > 0.02f) {
        uint16_t sweepCol = (normAngle < 0.0f) ? kColLightCyan : kColLightYellow;
        canvasDrawArcPoly(c, arcCx, arcCy, arcR, kLightPi * 0.5f, theta, 28, sweepCol, 3);
    }

    for (int i = 0; i < 2; i++) {
        canvasDrawLightRay(c, sunX, sunY, sides[i].dotX, dotY, sides[i].pct, brightnessPct);
    }

    float vecDx = sunX - (float)arcCx;
    float vecDy = sunY - (float)arcCy;
    float vecLen = sqrtf(vecDx * vecDx + vecDy * vecDy);
    if (vecLen > 22.0f) {
        float rEnd = vecLen - 16.0f;
        c->drawLine(
            arcCx, arcCy,
            arcCx + (int)lroundf((vecDx / vecLen) * rEnd),
            arcCy + (int)lroundf((vecDy / vecLen) * rEnd),
            lightRgb565(0x2a, 0x53, 0x78)
        );
    }

    // Středový bod r=4 (otevřený oblouk bez rámečku)
    c->fillCircle(arcCx, arcCy, 4, kColLightCyan);
    c->drawCircle(arcCx, arcCy, 4, kColLightWhite);

    canvasDrawSun(c, sunX, sunY, brightnessPct, nowMs);
    for (int i = 0; i < 2; i++) {
        canvasDrawPhotoDot(c, sides[i].dotX, dotY, sides[i].letter, sides[i].pct, sides[i].dominant);
    }

    // Spodní 4 karty (y=198..237), FOTO L / FOTO P bez duplicitních %
    for (int i = 0; i < 2; i++) {
        snprintf(buf, sizeof(buf), "ADC: %u", (unsigned)sides[i].rawAdc);
        uint16_t border = sides[i].dominant ? kColLightCyan : kColLightCardBorder;
        canvasDrawLightCardBase(c, sides[i].cardX, 76, sides[i].label, kColLightCyan, border, buf, kColLightWhite);
        canvasDrawHorizontalBar(c, sides[i].cardX + 4, 227, 68, 6, sides[i].pct, lightPhotoGlowColor(sides[i].pct));
    }

    snprintf(buf, sizeof(buf), "%s%d deg", (angleDeg > 0) ? "+" : "", angleDeg);
    canvasDrawLightCardBase(c, 84, 74, "ODCHYLKA", kColLightMuted, kColLightCardBorder, buf, dirCol);
    c->fillRect(88, 227, 66, 6, kColLightBgDark);
    c->drawRect(88, 227, 66, 6, kColLightBarBorder);
    int offsetPx = (int)lroundf(normAngle * 31.0f);
    if (offsetPx < 0) {
        c->fillRect(121 + offsetPx, 228, -offsetPx, 4, kColLightCyan);
    } else if (offsetPx > 0) {
        c->fillRect(121, 228, offsetPx, 4, kColLightYellow);
    }
    c->drawLine(121, 225, 121, 234, kColLightWhite);

    snprintf(buf, sizeof(buf), "%d%%", (int)lroundf(brightnessPct));
    canvasDrawLightCardBase(c, 162, 74, "JAS OKOLI", kColLightMuted, kColLightCardBorder, buf, kColLightYellow);
    canvasDrawHorizontalBar(c, 166, 227, 66, 6, brightnessPct, kColLightYellow);

    pushCanvasToScreen();
}

void GraphicsManager::drawInfoOverlay(const char* title, uint8_t page,
                                      const InfoLine* lines, int totalRows, int scrollOffset) {
    if (!title) title = "NAPOVEDA";
    if (!lines || totalRows < 0) totalRows = 0;

    const int maxScroll = (totalRows > INFO_VISIBLE_ROWS) ? (totalRows - INFO_VISIBLE_ROWS) : 0;
    if (scrollOffset < 0) scrollOffset = 0;
    if (scrollOffset > maxScroll) scrollOffset = maxScroll;

    char badge[8];
    snprintf(badge, sizeof(badge), "[%u/2]", static_cast<unsigned>(page == 2 ? 2 : 1));
    const char* footer = (page == 2)
        ? (totalRows > INFO_VISIBLE_ROWS ? "[INFO] Zavrit | [UP/DOWN] Posun" : "[INFO] Zavrit napovedu")
        : (totalRows > INFO_VISIBLE_ROWS ? "[INFO] Obecne | [UP/DOWN] Posun" : "[INFO] Obecne info");
    const int footerX = (320 - static_cast<int>(strlen(footer)) * 6) / 2;

    GFXcanvas16* c = getCanvas();
    if (c == nullptr) {
        clearScreen(ST77XX_BLACK);
        drawRoundRect(4, 4, 312, 232, 8, ST77XX_YELLOW);
        drawTextPartial(14, 13, title, ST77XX_YELLOW, ST77XX_BLACK, 2);
        drawTextPartial(246, 13, badge, ST77XX_CYAN, ST77XX_BLACK, 2);
        drawLine(12, 36, 307, 36, ST77XX_YELLOW);
        const int limit = (totalRows - scrollOffset < INFO_VISIBLE_ROWS) ? (totalRows - scrollOffset) : INFO_VISIBLE_ROWS;
        for (int i = 0; i < limit; ++i) {
            const InfoLine& ln = lines[scrollOffset + i];
            String text = (ln.tag && ln.tag[0]) ? (String(ln.tag) + " " + String(ln.body ? ln.body : "")) : String(ln.body ? ln.body : "");
            drawTextPartial(14, 44 + i * 26, text, ST77XX_WHITE, ST77XX_BLACK, 2);
        }
        drawLine(12, 204, 307, 204, kColInfoDivider);
        drawTextPartial(footerX, 214, footer, ST77XX_GREEN, ST77XX_BLACK, 1);
        return;
    }

    c->fillScreen(ST77XX_BLACK);
    c->setFont(nullptr);
    c->setTextWrap(false);

    // Zluty ramecek v puvodnim stylu, roztazeny pro text velikosti 2
    c->drawRoundRect(4, 4, 312, 232, 8, ST77XX_YELLOW);

    // Hlavicka: nazev vlevo + indikator stranky [1/2] nebo [2/2] vpravo
    c->setTextSize(2);
    c->setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    c->setCursor(14, 13);
    c->print(title);

    c->setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    c->setCursor(246, 13);
    c->print(badge);

    c->drawFastHLine(12, 36, 296, ST77XX_YELLOW);

    // Telo s max 6 radky textu velikosti 2
    const int rowsToDraw = (totalRows - scrollOffset < INFO_VISIBLE_ROWS)
                               ? (totalRows - scrollOffset)
                               : INFO_VISIBLE_ROWS;
    for (int i = 0; i < rowsToDraw; ++i) {
        drawInfoTextRow(c, 14, 44 + i * 26, lines[scrollOffset + i]);
    }

    // Prava posuvna lista + sipky nahoru/dolu pokud se text nevejde na jednu obrazovku
    if (totalRows > INFO_VISIBLE_ROWS) {
        const uint16_t upColor = (scrollOffset > 0) ? ST77XX_CYAN : kColInfoArrowDim;
        const uint16_t dnColor = (scrollOffset < maxScroll) ? ST77XX_CYAN : kColInfoArrowDim;

        c->fillTriangle(303, 44, 298, 51, 308, 51, upColor);
        c->fillTriangle(303, 195, 298, 188, 308, 188, dnColor);

        constexpr int trackY = 56;
        constexpr int trackH = 127;
        c->fillRect(301, trackY, 5, trackH, kColInfoTrackBg);

        int thumbH = (trackH * INFO_VISIBLE_ROWS) / totalRows;
        if (thumbH < 18) thumbH = 18;
        const int thumbY = trackY + ((maxScroll > 0) ? ((trackH - thumbH) * scrollOffset) / maxScroll : 0);
        c->fillRect(301, thumbY, 5, thumbH, ST77XX_YELLOW);
    }

    // Spodni paticka
    c->drawFastHLine(12, 204, 296, kColInfoDivider);
    c->setTextSize(1);
    c->setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    c->setCursor(footerX, 214);
    c->print(footer);

    pushCanvasToScreen();
}
