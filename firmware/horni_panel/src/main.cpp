#include <Arduino.h>

// Vestavěná RGB LED dioda (WS2812) na ESP32-S3 DevKitC-1 je připojená k GPIO 38
#define LED_PIN 38

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32-S3 RGB LED test start!");
  
  // Pro některé desky je potřeba nastavit pin jako výstup
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  static uint16_t hue = 0;
  
  // Převod HSV (odstín 0-359) na RGB s omezeným jasem na cca 50 % (max = 128)
  uint8_t r = 0, g = 0, b = 0;
  uint8_t sector = hue / 60;
  uint8_t rel = hue % 60;
  uint8_t min = 0;
  uint8_t max = 128; // Snížený jas, aby dioda neoslňovala
  uint8_t inc = min + (max - min) * rel / 60;
  uint8_t dec = max - (max - min) * rel / 60;
  
  switch (sector) {
    case 0: r = max; g = inc; b = min; break; // Červená -> Žlutá
    case 1: r = dec; g = max; b = min; break; // Žlutá -> Zelená
    case 2: r = min; g = max; b = inc; break; // Zelená -> Tyrkysová
    case 3: r = min; g = dec; b = max; break; // Tyrkysová -> Modrá
    case 4: r = inc; g = min; b = max; break; // Modrá -> Fialová
    case 5: r = max; g = min; b = dec; break; // Fialová -> Červená
  }
  
  // Použití vestavěné funkce neopixelWrite z ESP32 Arduino jádra
  neopixelWrite(LED_PIN, r, g, b);
  
  // Postupný posun odstínu (barvy)
  hue = (hue + 1) % 360;
  
  delay(15); // Rychlost prolínání barev
}