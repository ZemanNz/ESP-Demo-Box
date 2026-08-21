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
    #ifdef PIN_TFT_LED
        pinMode(PIN_TFT_LED, OUTPUT);
        digitalWrite(PIN_TFT_LED, HIGH);
    #endif

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
    if (!Wire.begin(I2C0_SDA, I2C0_SCL)) {
        drawErrorScreen("I2C_0 sbernice selhala!\n(SDA=" + String(I2C0_SDA) + ", SCL=" + String(I2C0_SCL) + ")");
        return false;
    }
    Serial.println("[I2C_0] OK – Sbernice spustena.");
#endif

#ifdef ENABLE_TCS34725
    if (!Wire1.begin(I2C1_SDA, I2C1_SCL)) {
        drawErrorScreen("I2C_1 sbernice selhala!\n(SDA=" + String(I2C1_SDA) + ", SCL=" + String(I2C1_SCL) + ")");
        return false;
    }
    Serial.println("[I2C_1] OK – Sbernice spustena.");
#endif

    // --- 2. Složité I2C Senzory (s kontrolou odpovědi) ---
#ifdef ENABLE_LSM6DS3
    bool lsmOk = false;
    uint8_t lsmAddr = 0x6B;
    if (lsm6ds3.begin_I2C(0x6B, &Wire)) {
        lsmOk = true;
        lsmAddr = 0x6B;
        Serial.println("[LSM6DS3]    OK – Wire 0x6B");
    } else if (lsm6ds3.begin_I2C(0x6A, &Wire)) {
        lsmOk = true;
        lsmAddr = 0x6A;
        Serial.println("[LSM6DS3]    OK – Wire 0x6A");
    } else {
        Serial.println("[LSM6DS3]    CHYBA: Senzor neodpovida na 0x6B ani 0x6A!");
        drawErrorScreen("LSM6DS3 Gyro/Akcel\nneodpovida na I2C (0x6B/0x6A)!");
        return false;
    }

    if (lsmOk) {
        lsm6ds3.setGyroRange(LSM6DS_GYRO_RANGE_250_DPS);
        lsm6ds3.setAccelRange(LSM6DS_ACCEL_RANGE_2_G);
        lsm6ds3.configInt1(false, false, false); // VYPNOUT Data-Ready

        // --- Nastavení HARDWAROVÉHO WAKE-UP PŘI NÁRAZU ---
        Wire.beginTransmission(lsmAddr); Wire.write(0x58); Wire.write(0x90); Wire.endTransmission();
        Wire.beginTransmission(lsmAddr); Wire.write(0x5C); Wire.write(0x00); Wire.endTransmission();
        Wire.beginTransmission(lsmAddr); Wire.write(0x5B); Wire.write(0x01); Wire.endTransmission();
        Wire.beginTransmission(lsmAddr); Wire.write(0x5E); Wire.write(0x20); Wire.endTransmission();
        
        #ifdef PIN_IMU_INT
            pinMode(PIN_IMU_INT, INPUT_PULLDOWN);
        #endif
        
        Serial.println("[LSM6DS3]    Probuzeni na INT1 aktivni.");
    }
#endif

#ifdef ENABLE_VL53L0X
    vl53.setTimeout(500);
    if (!vl53.init()) {
        drawErrorScreen("VL53L0X Laser dalkomer\nneodpovida na I2C (0x29)!");
        return false;
    }
    Serial.println("[VL53L0X]   OK – Laser dalkomer pripraven.");
#endif

#ifdef ENABLE_TCS34725
    if (!tcs.begin(TCS34725_ADDRESS, &Wire1)) {
        drawErrorScreen("TCS34725 Barevny senzor\nneodpovida na I2C_1!");
        return false;
    }
    Serial.println("[TCS34725]  OK – Barevny senzor pripraven.");
#endif

#ifdef ENABLE_LCD1602
    Wire.beginTransmission(0x27);
    if (Wire.endTransmission() != 0) {
        drawErrorScreen("LCD 1602 displej (0x27)\nneodpovida na I2C!");
        return false;
    }
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("ESP-Demo-Box OK");
    Serial.println("[LCD1602]   OK – Displej 16x2 inicializovan.");
#endif

    // --- 3. Jednoduché senzory s testem odezvy ---
#if defined(ENABLE_DHT) || defined(ENABLE_DHT22)
    dht.begin();
    delay(100);
    float testTemp = dht.readTemperature();
    float testHum = dht.readHumidity();
    if (isnan(testTemp) && isnan(testHum)) {
        // Zkusíme ještě jedno rychlé přečtení po krátké prodlevě
        delay(250);
        testTemp = dht.readTemperature();
        testHum = dht.readHumidity();
        if (isnan(testTemp) && isnan(testHum)) {
            drawErrorScreen("DHT Teplomer/Vlhkomer\nneodpovida na GPIO " + String(PIN_DHT) + "!");
            return false;
        }
    }
    Serial.println("[DHT]       OK – Teplota a vlhkost funkcni.");
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


bool setupUART() {
    Serial.println("[SETUP] Inicializace UART komunikace a Handshake...");
#ifdef ENABLE_UART_ESP
    SerialESP.begin(UART_ESP_BAUD, SERIAL_8N1, UART_ESP_RX, UART_ESP_TX);
    Serial.println("[SETUP] SerialESP (UART1) spusten na 115200 baud.");

    // --- SYNCHRONIZAČNÍ BOOT HANDSHAKE SE SPODNÍM PANELEM ---
    Serial.println("[HANDSHAKE] Cekam na potvrzeni pripravenosti spodniho panelu...");
    SerialESP.flush();
    unsigned long startTime = millis();

    while (millis() - startTime < 3500) { // Timeout 3.5 sekundy
        if (SerialESP.available()) {
            String msg = SerialESP.readStringUntil('\n');
            msg.trim();

            // 1. Spodní panel je v pořádku
            if (msg == "BOOT:READY") {
                SerialESP.println("BOOT:START"); // Povolíme spodnímu panelu start
                Serial.println("[HANDSHAKE] Spodni panel OK! Spoustim system...");
                return true;
            }

            // 2. Spodní panel hlásí chybu inicializace hardwaru
            if (msg.startsWith("BOOT:ERR:")) {
                String errorReason = msg.substring(9);
                drawErrorScreen("CHYBA DOLNIHO PANELU:\n" + errorReason);
                return false;
            }
        }
        delay(10);
    }

    // 3. Spodní panel vůbec neodpověděl v časovém limitu
    drawErrorScreen("DOLNI PANEL NEODPOVIDA!\nZkontroluj UART / napajeni.");
    return false;
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
    
    
    // 4. UART & HANDSHAKE
    if (!setupUART()) return false;

    Serial.println("-------------------------------------");
    Serial.println(">> Vsechen hardware je PRIPRAVEN <<");
    Serial.println("=====================================\n");
    
    return true;
}
