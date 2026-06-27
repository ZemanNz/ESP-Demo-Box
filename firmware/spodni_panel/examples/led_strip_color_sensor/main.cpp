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
  strip.setBrightness(255); // Nastavení jasu na maximum (0-255)

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
    // Přepočet a normalizace barevných složek (r, g, b) s ohledem na celkový jas (c)
    uint32_t sum = c;
    
    // Pro lepší vyvážení bílé (white balance) upravíme poměry.
    // TCS34725 je velmi citlivý na červenou/infračervenou složku,
    // proto mírně posílíme zelenou a modrou.
    float r_ratio = ((float)r / sum) * 1.0f;
    float g_ratio = ((float)g / sum) * 1.15f;
    float b_ratio = ((float)b / sum) * 1.25f;

    // Výpočet rozdílů pro zvýraznění barev (zvýšení saturace)
    float max_ratio = max(r_ratio, max(g_ratio, b_ratio));
    float min_ratio = min(r_ratio, min(g_ratio, b_ratio));
    float delta = max_ratio - min_ratio;

    float r_boosted = r_ratio;
    float g_boosted = g_ratio;
    float b_boosted = b_ratio;

    if (delta > 0.02f) { // pokud se nejedná o čistě šedou/černobílou barvu
      float avg = (r_ratio + g_ratio + b_ratio) / 3.0f;
      float saturation_factor = 1.8f; // Zesílení rozdílů od průměru pro živější barvy
      
      r_boosted = avg + (r_ratio - avg) * saturation_factor;
      g_boosted = avg + (g_ratio - avg) * saturation_factor;
      b_boosted = avg + (b_ratio - avg) * saturation_factor;

      // Omezení záporných hodnot
      if (r_boosted < 0) r_boosted = 0;
      if (g_boosted < 0) g_boosted = 0;
      if (b_boosted < 0) b_boosted = 0;
    }

    // Normalizace na maximální možný jas (roztáhneme největší složku na 255)
    float highest = max(r_boosted, max(g_boosted, b_boosted));
    if (highest > 0) {
      red_out = (r_boosted / highest) * 255;
      green_out = (g_boosted / highest) * 255;
      blue_out = (b_boosted / highest) * 255;
    }
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
