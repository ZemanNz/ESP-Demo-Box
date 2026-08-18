#include "HardwareSetup.h"
#include "config.h"
#include <Wire.h>
#include <SPI.h>

#include "GraphicsManager.h"
// Objekt gfx je deklarován v GraphicsManager.h a vytvořen v GraphicsManager.cpp, 
// takže ho tu nemusíme vytvářet znovu.

// Globální instance UART1 pro komunikaci se spodním panelem
HardwareSerial SerialESP(1);

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
    // Nyní používáme bezpečně GraphicsManager pro kreslení chyb!
    gfx.clearScreen(ST77XX_RED);
    gfx.drawTextPartial(10, 40, "KRITICKA CHYBA:", ST77XX_WHITE, ST77XX_RED, 2);
    gfx.drawTextPartial(10, 100, errorMessage, ST77XX_WHITE, ST77XX_RED, 1);
#endif
}

// ---------------------------------------------------------
// Inicializace jednotlivých modulů
// ---------------------------------------------------------
bool setupDisplay() {
#ifdef ENABLE_TFT_ST7789
    // SPI Sběrnici musí zapnout HardwareSetup jako hlavní dirigent hardware
    SPI.begin(PIN_TFT_SCLK, PIN_TFT_MISO, PIN_TFT_MOSI, PIN_TFT_CS);
    
    // Potom zavoláme Manažera, aby oživil samotný displej
    if (!gfx.init()) {
        return false;
    }
    
    // Kreslení testovací zelené obrazovky už dělá Manažer
    gfx.drawTextPartial(20, 15, "BOOT: OK", ST77XX_GREEN, ST77XX_BLACK, 2);
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
    bool lsmOk = false;
    uint8_t lsmAddr = 0x6B;
    if (lsm6ds3.begin_I2C(0x6B)) {
        lsmOk = true;
        lsmAddr = 0x6B;
        Serial.println("[LSM6DS3]    OK – Wire 0x6B");
    } else if (lsm6ds3.begin_I2C(0x6A)) {
        lsmOk = true;
        lsmAddr = 0x6A;
        Serial.println("[LSM6DS3]    OK – Wire 0x6A");
    } else {
        Serial.println("[LSM6DS3]    VAROVANI: Senzor neodpovida!");
    }

    if (lsmOk) {
        lsm6ds3.setGyroRange(LSM6DS_GYRO_RANGE_250_DPS);
        lsm6ds3.setAccelRange(LSM6DS_ACCEL_RANGE_2_G);
        lsm6ds3.configInt1(false, false, false); // VYPNOUT Data-Ready, aby pin nekmital 100x za sekundu!

        // --- Nastavení HARDWAROVÉHO WAKE-UP PŘI NÁRAZU ---
        Wire.beginTransmission(lsmAddr); Wire.write(0x58); Wire.write(0x90); Wire.endTransmission(); // TAP_CFG: Interrupt enable
        Wire.beginTransmission(lsmAddr); Wire.write(0x5C); Wire.write(0x00); Wire.endTransmission(); // WAKE_UP_DUR: 0
        Wire.beginTransmission(lsmAddr); Wire.write(0x5B); Wire.write(0x02); Wire.endTransmission(); // WAKE_UP_THS: 0x02 (jemnejsi citlivost na klepnuti)
        Wire.beginTransmission(lsmAddr); Wire.write(0x5E); Wire.write(0x20); Wire.endTransmission(); // MD1_CFG: Vyvést Wake-up na pin INT1
        Serial.println("[LSM6DS3]    Wake-up detekce narazu na INT1 aktivni.");
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
#ifdef ENABLE_UART_ESP
    SerialESP.begin(UART_ESP_BAUD, SERIAL_8N1, UART_ESP_RX, UART_ESP_TX);
    Serial.println("[SETUP] SerialESP (UART1) spusten na 115200 baud.");
#endif
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
