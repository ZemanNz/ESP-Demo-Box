/**
 * @file main.cpp
 * @brief Příklad: 0.96" OLED displej (I2C 128x64 px, SSD1306) pro ESP32
 * @details Tento kód demonstruje inicializaci,ykreslování textu, geometrií a animovaného ukazatele
 *          na OLED displeji připojeném ke spodnímu panelu s ESP32-WROOM.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Definice parametrů OLED displeje
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS 0x3C  // Standardní I2C adresa větších i menších 0.96" OLED modulů

// I2C piny spodního panelu
#define PIN_I2C_SDA     21
#define PIN_I2C_SCL     22

// Inicializace objektu displeje
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("\n=== ESP-Demo-Box: Spodní panel – 0.96\" OLED Displej (SSD1306) ==="));

  // Inicializace I2C sběrnice
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  // Inicializace OLED displeje s generováním 3.3V z VCC (SSD1306_SWITCHCAPVCC)
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("CHYBA: Alokace SSD1306 selhala! Zkontroluj I2C adresu (0x3C/0x3D) a zapojení."));
    while (true) delay(100);
  }

  // Úvodní obrazovka
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 10);
  display.println(F("ESP-DEMO-BOX"));
  display.setCursor(10, 30);
  display.println(F("SPODNI PANEL OK"));
  display.drawRect(5, 5, 118, 54, SSD1306_WHITE);
  display.display();
  delay(2000);

  Serial.println(F("Displej úspěšně inicializován. Spouštím hlavní grafickou smyčku...\n"));
}

void loop() {
  static uint8_t progress = 0;
  unsigned long seconds = millis() / 1000UL;

  // Vyčištění obrazovkového bufferu
  display.clearDisplay();

  // 1. ZÁHLAVÍ
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.println(F("OLED SSD1306 DEMO"));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  // 2. TEXTOVÉ INFORMACE
  display.setCursor(0, 16);
  display.printf("Uptime : %lu s", seconds);

  display.setCursor(0, 28);
  display.printf("I2C SDA: GPIO %d", PIN_I2C_SDA);

  display.setCursor(0, 40);
  display.printf("I2C SCL: GPIO %d", PIN_I2C_SCL);

  // 3. ANIMOVANÝ PROGRESS BAR A RÁMEČEK (Dole)
  display.drawRect(0, 52, 128, 12, SSD1306_WHITE);
  int fillWidth = map(progress, 0, 100, 0, 124);
  display.fillRect(2, 54, fillWidth, 8, SSD1306_WHITE);

  // Aktualizace fyzického displeje z bufferu
  display.display();

  // Inkrementace progresu
  progress = (progress + 5) % 105;

  delay(200);
}
