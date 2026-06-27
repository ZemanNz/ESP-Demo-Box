#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN     19
#define NUM_LEDS    8

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Pomocná funkce pro generování barev duhy (rainbow wheel)
// Vstupní hodnota 0 až 255 vrací barvu (přechod Červená -> Zelená -> Modrá -> Červená)
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP-Demo-Box: Spodní panel - LED pásek příklad spuštěn");
  
  strip.begin();
  strip.show();            // Vypnout všechny LED na začátku
  strip.setBrightness(50); // Nastavení jasu (0-255), 50 je bezpečné a úsporné
}

void loop() {
  static uint16_t j = 0;
  
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    // Vytvoření efektu tekoucí duhy (RGB barvy do kruhu)
    strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
  }
  
  strip.show();
  delay(20); // Rychlost přelévání barev (čím nižší delay, tím rychlejší pohyb)
  
  j++;
  if (j >= 256 * 5) {
    j = 0; // Reset cyklu po dokončení 5 plných otáček
  }
}
