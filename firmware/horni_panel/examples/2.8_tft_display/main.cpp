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

void setup() {
  Serial.begin(115200);
  Serial.println("Inicializace TFT LCD displeje pres Adafruit_ST7789...");

  // Inicializace hardwarové SPI sběrnice s našimi piny před spuštěním displeje
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);

  // Inicializace displeje jako 240x320 ST7789
  tft.init(240, 320);
  
  // Oprava invertovaných barev (vypnutí výchozí knihovní inverze)
  tft.invertDisplay(false);

  // Vyzkoušíme nejprve rotaci 1 (na šířku 320x240). 
  // Pokud bude text vzhůru nohama, změníme na 3. Pokud bude otočený o 90 stupňů, změníme na 0 nebo 2.
  tft.setRotation(1);

  // Smazání obrazovky na modrou barvu
  tft.fillScreen(ST77XX_BLUE);

  // Vykreslení rámečku po obvodu displeje (320x240)
  tft.drawRect(0, 0, 320, 240, ST77XX_WHITE);
  tft.drawRect(2, 2, 316, 236, ST77XX_GREEN);

  // 1. Žlutý obdélník s bílým okrajem
  tft.fillRect(20, 20, 100, 50, ST77XX_YELLOW);
  tft.drawRect(20, 20, 100, 50, ST77XX_WHITE);

  // 2. Červený kruh s bílým okrajem
  tft.fillCircle(220, 60, 30, ST77XX_RED);
  tft.drawCircle(220, 60, 30, ST77XX_WHITE);

  // 3. Zelená dělicí čára
  tft.drawLine(10, 120, 310, 120, ST77XX_GREEN);

  // 4. Text "Maturitni prace: ESP32-S3" - zkráceno a posunuto o 5px doleva pro zarovnání na jeden řádek
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLUE);
  tft.setTextSize(2);
  tft.setCursor(10, 140);
  tft.print("Maturitni prace: ESP32-S3");

  Serial.println("Inicializace a kresleni dokonceno.");
}

void loop() {
  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    
    // Výpis uptime na displej
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLUE);
    tft.setTextSize(1);
    tft.setCursor(15, 200);
    tft.print("Uptime: ");
    tft.print(millis() / 1000);
    tft.print(" s   ");
    
    // Výpis uptime na sériový port
    Serial.print("Program bezi - Uptime: ");
    Serial.print(millis() / 1000);
    Serial.println(" s");
  }
}
