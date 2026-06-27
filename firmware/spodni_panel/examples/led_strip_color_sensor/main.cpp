#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <Adafruit_NeoPixel.h>

// Definice pinů pro LED pásek a počet diod
#define LED_PIN     19
#define NUM_LEDS    8

// Definice I2C pinů pro ESP32 (SDA na GPIO 21, SCL na GPIO 22)
#define I2C_SDA     21
#define I2C_SCL     22

// Inicializace LED pásku
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Inicializace barevného senzoru s integračním časem 50 ms a ziskem 4x
// TCS34725 využívá výchozí I2C adresu (0x29)
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

void setup() {
  Serial.begin(115200);
  Serial.println("ESP-Demo-Box: Barevny senzor TCS34725 + LED pasek");

  // Nastavení vlastních I2C pinů (21 = SDA, 22 = SCL)
  Wire.begin(I2C_SDA, I2C_SCL);

  // Inicializace LED pásku
  strip.begin();
  strip.show();            // Zhasnout všechny LED
  strip.setBrightness(50); // Nastavení jasu (0-255)

  // Inicializace TCS34725 barevného senzoru
  if (tcs.begin(TCS34725_ADDRESS, &Wire)) {
    Serial.println("Senzor TCS34725 nalezen!");
  } else {
    Serial.println("CHYBA: Senzor TCS34725 nenalezen! Zkontrolujte zapojeni.");
    // V případě chyby rozsvítíme LED pásek červeně pro vizuální signalizaci chyby
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(255, 0, 0));
    }
    strip.show();
    while (1) {
      delay(10);
    }
  }
}

void loop() {
  uint16_t r, g, b, c;
  
  // Načtení hrubých dat ze senzoru
  tcs.getRawData(&r, &g, &b, &c);

  uint8_t red_out = 0;
  uint8_t green_out = 0;
  uint8_t blue_out = 0;

  if (c > 0) {
    // Přepočet a normalizace barevných složek (r, g, b) na rozsah 0-255 s ohledem na celkový jas (c)
    // Bez normalizace by barva silně závisela na intenzitě osvětlení a vzdálenosti objektu
    uint32_t sum = c;
    
    // Použijeme float pro přesnější poměry, přičemž zohledníme i určitou kalibraci barev
    float r_ratio = (float)r / sum;
    float g_ratio = (float)g / sum;
    float b_ratio = (float)b / sum;

    // Vynásobením konstantou 255 (a případně drobným boostem pro sytost) získáme 0-255 složky
    // TCS34725 má vyšší citlivost na červenou a infračervenou složku,
    // proto poměry trochu upravíme pro věrnější barevné podání (jednoduchý white balance)
    int r_cal = r_ratio * 256 * 1.0;
    int g_cal = g_ratio * 256 * 0.9;
    int b_cal = b_ratio * 256 * 0.9;

    // Omezení hodnot do rozsahu 0-255
    red_out = (r_cal > 255) ? 255 : r_cal;
    green_out = (g_cal > 255) ? 255 : g_cal;
    blue_out = (b_cal > 255) ? 255 : b_cal;
  }

  // Výpis naměřených a přepočtených hodnot do sériové linky
  Serial.printf("Namereeno -> R: %d, G: %d, B: %d, C: %d | Vystup LED -> R: %d, G: %d, B: %d\n", 
                r, g, b, c, red_out, green_out, blue_out);

  // Nastavení zdetekované barvy na celý LED pásek
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(red_out, green_out, blue_out));
  }
  strip.show();

  delay(100); // Měření každých 100 ms
}
