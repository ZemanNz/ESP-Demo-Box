#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// Definice pinů pro ESP32-S3 a displej
#define TFT_CS   10
#define TFT_DC   13
#define TFT_RST  14
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_MISO -1 // MISO nepoužíváme

// Inicializace knihovny Adafruit_ST7789 s hardwarovou SPI
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Uživatelské barvy pro prémiový vzhled
#define COLOR_CARD        0x0841 // Velmi tmavá šedá s nádechem modré
#define COLOR_BORDER      0x2104 // Tmavá šedá pro ohraničení
#define COLOR_TEXT_MUTED  0x7BEF // Tlumená šedá pro popisky
#define COLOR_GRID        0x0183 // Tmavě modrá pro středovou osu grafu
#define COLOR_GRID_SUB    0x00A1 // Ještě tmavší modrá pro pomocné osy grafu

// Nastavení rozměrů grafu
#define GRAPH_X 172
#define GRAPH_Y 82
#define GRAPH_W 131
#define GRAPH_H 136
#define GRAPH_CENTER_Y (GRAPH_Y + GRAPH_H / 2)

int y_values[GRAPH_W]; // Historie hodnot pro graf

void setup() {
  Serial.begin(115200);
  Serial.println("Inicializace premium dashboardu...");

  // Inicializace hardwarové SPI sběrnice
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);

  // Inicializace displeje jako 240x320 ST7789
  tft.init(240, 320);
  tft.invertDisplay(false); // Vypnutí výchozí inverze knihovny pro správné barvy
  tft.setRotation(1);       // Nastavení režimu na šířku (320x240)

  // Černé pozadí celé obrazovky
  tft.fillScreen(ST77XX_BLACK);

  // Inicializace hodnot grafu na středovou hodnotu
  for (int i = 0; i < GRAPH_W; i++) {
    y_values[i] = GRAPH_CENTER_Y;
  }

  // --- Vykreslení statického rozhraní (karty a lišta) ---
  
  // 1. Horní panel (Header)
  tft.fillRoundRect(10, 8, 300, 30, 6, 0x10A2); // Tmavě modrý panel
  tft.drawRoundRect(10, 8, 300, 30, 6, 0x3186); // Okraj panelu
  
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 15);
  tft.print("ESP32-S3 DEMO BOX");

  // 2. Levá karta - SYSTEM DATA
  tft.fillRoundRect(10, 46, 145, 184, 8, COLOR_CARD);
  tft.drawRoundRect(10, 46, 145, 184, 8, COLOR_BORDER);
  
  tft.setTextColor(ST77XX_ORANGE);
  tft.setTextSize(1);
  tft.setCursor(20, 56);
  tft.print("SYSTEM STATUS");
  tft.drawFastHLine(15, 68, 135, COLOR_BORDER);

  // 3. Pravá karta - OSCILLOSCOPE
  tft.fillRoundRect(165, 46, 145, 184, 8, COLOR_CARD);
  tft.drawRoundRect(165, 46, 145, 184, 8, COLOR_BORDER);
  
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(175, 56);
  tft.print("OSCILLOSCOPE");
  tft.drawFastHLine(170, 68, 135, COLOR_BORDER);

  Serial.println("Staticke prvky dashboardu vykresleny.");
}

void loop() {
  static uint32_t lastUpdate = 0;
  static uint32_t lastFastUpdate = 0;
  static float angle = 0;
  
  uint32_t currentMillis = millis();

  // Rychlá smyčka pro animaci grafu (každých 30 ms = ~33 fps)
  if (currentMillis - lastFastUpdate >= 30) {
    lastFastUpdate = currentMillis;

    // A. Smazání starého grafu překreslením barvou pozadí karty
    for (int i = 0; i < GRAPH_W - 1; i++) {
      tft.drawLine(GRAPH_X + i, y_values[i], GRAPH_X + i + 1, y_values[i + 1], COLOR_CARD);
    }
    // Smazání staré tečky na konci
    tft.fillCircle(GRAPH_X + GRAPH_W - 2, y_values[GRAPH_W - 2], 2, COLOR_CARD);

    // B. Vykreslení/obnova referenční mřížky
    tft.drawFastHLine(GRAPH_X, GRAPH_CENTER_Y, GRAPH_W, COLOR_GRID); // Středová osa
    tft.drawFastHLine(GRAPH_X, GRAPH_CENTER_Y - 30, GRAPH_W, COLOR_GRID_SUB); // Horní mez
    tft.drawFastHLine(GRAPH_X, GRAPH_CENTER_Y + 30, GRAPH_W, COLOR_GRID_SUB); // Dolní mez

    // C. Posun hodnot v poli doleva
    for (int i = 0; i < GRAPH_W - 1; i++) {
      y_values[i] = y_values[i + 1];
    }

    // D. Výpočet nové hodnoty (kombinace dvou sinusovek pro zajímavější vlnu)
    angle += 0.08f;
    float val = sin(angle) * 28.0f + sin(angle * 2.3f) * 10.0f;
    y_values[GRAPH_W - 1] = GRAPH_CENTER_Y + (int)val;

    // E. Vykreslení nového grafu (neonově zelený průběh)
    for (int i = 0; i < GRAPH_W - 1; i++) {
      tft.drawLine(GRAPH_X + i, y_values[i], GRAPH_X + i + 1, y_values[i + 1], ST77XX_GREEN);
    }
    // Vykreslení jasně červené tečky na konci průběhu (efekt laserového paprsku)
    tft.fillCircle(GRAPH_X + GRAPH_W - 2, y_values[GRAPH_W - 2], 2, ST77XX_RED);

    // F. Animace LED indikátoru v horní liště (blikání na 1 Hz)
    bool ledOn = (currentMillis / 500) % 2 == 0;
    tft.fillCircle(290, 23, 4, ledOn ? ST77XX_GREEN : 0x03E0);

    // G. Animace progress baru v levé kartě (cykluje každých 10 sekund)
    int progress_w = map((currentMillis % 10000), 0, 10000, 0, 125);
    tft.fillRect(20, 208, 125, 6, COLOR_BORDER); // Pozadí progress baru
    tft.fillRect(20, 208, progress_w, 6, ST77XX_CYAN); // Výplň progress baru
  }

  // Pomalá smyčka pro update číselných hodnot na displeji (každých 1000 ms)
  if (currentMillis - lastUpdate >= 1000) {
    lastUpdate = currentMillis;

    // 1. Výpis Uptime
    tft.fillRect(20, 78, 125, 30, COLOR_CARD); // Vymazání staré hodnoty
    tft.setTextColor(COLOR_TEXT_MUTED);
    tft.setTextSize(1);
    tft.setCursor(20, 78);
    tft.print("Uptime:");
    
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 88);
    tft.print(currentMillis / 1000);
    tft.print(" s");

    // 2. Výpis Heap Free (volná paměť RAM v kilobajtech)
    tft.fillRect(20, 120, 125, 30, COLOR_CARD); // Vymazání staré hodnoty
    tft.setTextColor(COLOR_TEXT_MUTED);
    tft.setTextSize(1);
    tft.setCursor(20, 120);
    tft.print("Free RAM:");
    
    tft.setTextColor(ST77XX_YELLOW);
    tft.setTextSize(2);
    tft.setCursor(20, 130);
    tft.print(ESP.getFreeHeap() / 1024);
    tft.print(" KB");

    // 3. Výpis simulované teploty procesoru (jemné kolísání)
    tft.fillRect(20, 162, 125, 30, COLOR_CARD); // Vymazání staré hodnoty
    tft.setTextColor(COLOR_TEXT_MUTED);
    tft.setTextSize(1);
    tft.setCursor(20, 162);
    tft.print("CPU Temp:");
    
    tft.setTextColor(ST77XX_ORANGE);
    tft.setTextSize(2);
    tft.setCursor(20, 172);
    // Simulovaná teplota 41.5 až 42.5 °C
    float simTemp = 41.5f + (float)(rand() % 10) / 10.0f;
    tft.print(simTemp, 1);
    tft.print(" C");

    // Výpis na sériový port pro kontrolu
    Serial.print("Program bezi - Uptime: ");
    Serial.print(currentMillis / 1000);
    Serial.print(" s | Volna RAM: ");
    Serial.print(ESP.getFreeHeap() / 1024);
    Serial.println(" KB");
  }
}
