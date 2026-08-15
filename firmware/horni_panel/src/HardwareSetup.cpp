#include "HardwareSetup.h"
#include "config.h"
#include <Wire.h>
#include <SPI.h>

// --- Displej ---
#ifdef ENABLE_TFT_ST7789
  #include <Adafruit_GFX.h>
  #include <Adafruit_ST7789.h>
  Adafruit_ST7789 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
#endif

// --- Senzory a periferie ---
#if defined(ENABLE_DHT) || defined(ENABLE_DHT22)
  #include <DHT.h>
  DHT dht(PIN_DHT, DHT_TYPE);
#endif

#ifdef ENABLE_LSM6DS3
  #include <Adafruit_LSM6DS3.h>
  Adafruit_LSM6DS3 lsm6ds3;
#endif

#ifdef ENABLE_VL53L0X
  #include <VL53L0X.h>
  VL53L0X vl53;
#endif

#ifdef ENABLE_TCS34725
  #include <Adafruit_TCS34725.h>
  Adafruit_TCS34725 tcs(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
#endif

#ifdef ENABLE_LCD1602
  #include <LiquidCrystal_I2C.h>
  LiquidCrystal_I2C lcd(0x27, 16, 2);
#endif

#ifdef ENABLE_WS2812B
  #include <Adafruit_NeoPixel.h>
  Adafruit_NeoPixel strip(WS2812B_NUM_LEDS, PIN_WS2812B, NEO_GRB + NEO_KHZ800);
#endif

// Pomocná funkce pro 74HC595 (jen pro vymazání při startu)
#ifdef ENABLE_74HC595
  static void seg_clear() {
    digitalWrite(PIN_SEG_LATCH, LOW);
    for (int i = 0; i < SEG_NUM_DIGITS; i++) {
      shiftOut(PIN_SEG_DATA, PIN_SEG_CLK, MSBFIRST, SEG_COMMON_ANODE ? 0xFF : 0x00);
    }
    digitalWrite(PIN_SEG_LATCH, HIGH);
  }
#endif

// ---------------------------------------------------------
// Pomocná funkce pro vykreslení Fail Logu na displej
// ---------------------------------------------------------
void drawErrorScreen(String errorMessage) {
    Serial.println("[FAIL LOG] " + errorMessage);
#ifdef ENABLE_TFT_ST7789
    tft.fillScreen(ST77XX_RED);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 40);
    tft.println("KRITICKA CHYBA:");
    tft.setTextSize(1);
    tft.setCursor(10, 100);
    tft.println(errorMessage);
#endif
}

// ---------------------------------------------------------
// Inicializace jednotlivých modulů
// ---------------------------------------------------------
bool setupDisplay() {
#ifdef ENABLE_TFT_ST7789
    Serial.println("[SETUP] Inicializace TFT displeje...");
    pinMode(PIN_TFT_LED, OUTPUT);
    digitalWrite(PIN_TFT_LED, HIGH);
    
    SPI.begin(PIN_TFT_SCLK, PIN_TFT_MISO, PIN_TFT_MOSI, PIN_TFT_CS);
    tft.init(TFT_W, TFT_H);
    tft.invertDisplay(false);
    tft.setRotation(1);
    
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_GREEN);
    tft.setTextSize(2);
    tft.setCursor(20, 15);
    tft.print("BOOT: OK");
#endif
    return true; 
}

bool setupSensors() {
    Serial.println("[SETUP] Inicializace senzoru a periferii na hornim panelu...");
    
    // --- 1. I2C Sběrnice ---
#if defined(ENABLE_LSM6DS3) || defined(ENABLE_LCD1602) || defined(ENABLE_VL53L0X)
    Wire.begin(I2C0_SDA, I2C0_SCL);
#endif
#ifdef ENABLE_TCS34725
    Wire1.begin(I2C1_SDA, I2C1_SCL);
#endif

    // --- 2. Složité I2C Senzory (s možností selhání) ---
#ifdef ENABLE_LSM6DS3
    if (!lsm6ds3.begin_I2C(0x6A) && !lsm6ds3.begin_I2C(0x6B)) {
        drawErrorScreen("LSM6DS3 Gyroskop neodpovida!");
        return false;
    }
#endif

#ifdef ENABLE_VL53L0X
    vl53.setTimeout(500);
    if (!vl53.init()) {
        drawErrorScreen("VL53L0X Laser neodpovida!");
        return false;
    }
#endif

#ifdef ENABLE_TCS34725
    if (!tcs.begin(TCS34725_ADDRESS, &Wire1)) {
        drawErrorScreen("TCS34725 Barvy neodpovidaji!");
        return false;
    }
#endif

#ifdef ENABLE_LCD1602
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Booting...");
#endif

    // --- 3. Jednoduché senzory a výstupy (Nemusí se složitě inicializovat, jen pinMode) ---
#if defined(ENABLE_DHT) || defined(ENABLE_DHT22)
    dht.begin(); // Vrací void, chybu zjistíme až při čtení
#endif

#ifdef ENABLE_ULTRASONIC
    pinMode(PIN_ULTRASONIC_TRIG, OUTPUT);
    pinMode(PIN_ULTRASONIC_ECHO, INPUT);
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
#endif

#ifdef ENABLE_IR_SENSORS
    pinMode(PIN_IR1, INPUT);
#endif

#ifdef ENABLE_PHOTORESISTORS
    pinMode(PIN_PHOTO1, INPUT);
    pinMode(PIN_PHOTO2, INPUT);
#endif

#ifdef ENABLE_BUTTONS
    pinMode(PIN_BTN1, INPUT_PULLUP);
    pinMode(PIN_BTN2, INPUT_PULLUP);
#endif

#ifdef ENABLE_BUZZER
    pinMode(PIN_BUZZER, OUTPUT);
    // Krátké pípnutí pro signalizaci startu
    tone(PIN_BUZZER, 2000, 100);
#endif

#ifdef ENABLE_LEDS
    pinMode(PIN_LED1, OUTPUT); digitalWrite(PIN_LED1, LOW);
    pinMode(PIN_LED2, OUTPUT); digitalWrite(PIN_LED2, LOW);
    pinMode(PIN_LED3, OUTPUT); digitalWrite(PIN_LED3, LOW);
#endif

#ifdef ENABLE_WS2812B
    strip.begin();
    strip.setBrightness(60);
    strip.show(); // Vypne všechny LED
#endif

#ifdef ENABLE_74HC595
    pinMode(PIN_SEG_DATA,  OUTPUT);
    pinMode(PIN_SEG_CLK,   OUTPUT);
    pinMode(PIN_SEG_LATCH, OUTPUT);
    seg_clear(); // Vyčistí staré znaky
#endif

    return true;
}

bool setupWiFi() {
    Serial.println("[SETUP] Inicializace Wi-Fi...");
    return true;
}

bool setupUART() {
    Serial.println("[SETUP] Inicializace UART komunikace...");
    return true;
}

// ---------------------------------------------------------
// Hlavní sdružující funkce (Master Setup)
// ---------------------------------------------------------
bool initializeAllHardware() {
    Serial.println("\n=====================================");
    Serial.println("   STARTUJE HARDWARE HORNIHO PANELU  ");
    Serial.println("=====================================");

    // 1. DISPLEJ MUSÍ BÝT PRVNÍ!
    if (!setupDisplay()) return false;
    
    // 2. SENZORY
    if (!setupSensors()) return false;
    
    // 3. WI-FI
    if (!setupWiFi()) return false;
    
    // 4. UART & HANDSHAKE
    if (!setupUART()) return false;

    Serial.println("-------------------------------------");
    Serial.println(">> Vsechen hardware je PRIPRAVEN <<");
    Serial.println("=====================================\n");
    
    return true;
}
