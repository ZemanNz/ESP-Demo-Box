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
  // Hardwarové přerušení pro detekci drbnutí z gyroskopu LSM6DS3 (INT1)
  volatile bool g_imuHwInterruptFired = false;
  void IRAM_ATTR imuWakeupIsr() {
      g_imuHwInterruptFired = true;
  }
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
    Wire.setClock(400000); // 400 kHz Fast I2C pro plynulé vyčítání gyroskopu a periferií
    Serial.println("[I2C_0] OK – Sbernice spustena (400 kHz).");
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
        lsm6ds3.setGyroRange(LSM6DS_GYRO_RANGE_500_DPS); // 500 DPS pro přesný a rychlý pohyb bez saturace
        lsm6ds3.setAccelRange(LSM6DS_ACCEL_RANGE_4_G);
        lsm6ds3.setGyroDataRate(LSM6DS_RATE_104_HZ);
        lsm6ds3.setAccelDataRate(LSM6DS_RATE_104_HZ);
        lsm6ds3.configInt1(false, false, false); // VYPNOUT Data-Ready

        // --- Nastavení HARDWAROVÉHO WAKE-UP PŘI NÁRAZU (z config.h) ---
        Wire.beginTransmission(lsmAddr); Wire.write(0x58); Wire.write(0x90); Wire.endTransmission(); // TAP_CFG: INTERRUPTS_ENABLE + SLOPE_FDS
        Wire.beginTransmission(lsmAddr); Wire.write(0x5C); Wire.write(IMU_WAKEUP_DUR); Wire.endTransmission(); // WAKE_UP_DUR: 0 ODR
        Wire.beginTransmission(lsmAddr); Wire.write(0x5B); Wire.write(IMU_WAKEUP_THS); Wire.endTransmission(); // WAKE_UP_THS: 0x02 = extra citlivé na prst
        Wire.beginTransmission(lsmAddr); Wire.write(0x5E); Wire.write(0x20); Wire.endTransmission(); // MD1_CFG: INT1_WU (probuzení na pin INT1)
        
        #ifdef PIN_IMU_INT
            pinMode(PIN_IMU_INT, INPUT_PULLDOWN);
            attachInterrupt(digitalPinToInterrupt(PIN_IMU_INT), imuWakeupIsr, RISING);
        #endif
        
        Serial.printf("[LSM6DS3]    Probuzeni na INT1 (GPIO %d) aktivni s prahem 0x%02X (~125 mg).\n",
                      PIN_IMU_INT, IMU_WAKEUP_THS);
    }
#endif

#ifdef ENABLE_VL53L0X
    vl53.setBus(&Wire);
    vl53.setTimeout(500);
    if (!vl53.init()) {
        drawErrorScreen("VL53L0X Laser dalkomer\nneodpovida na I2C (0x29)!");
        return false;
    }
    vl53.startContinuous(100); // Spuštění kontinuálního měření každých 100 ms
    Serial.println("[VL53L0X]   OK – Laser dalkomer pripraven (continuous 100ms).");
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
    analogSetPinAttenuation(PIN_IR1, ADC_11db); // IR1: analogové měření vzdálenosti (GPIO 20)
    pinMode(PIN_IR1, INPUT);
    pinMode(PIN_IR2, INPUT);                    // IR2: digitální překážka (GPIO 38)
#endif

#ifdef ENABLE_PHOTORESISTORS
    analogSetPinAttenuation(PIN_PHOTO1, ADC_11db);
    analogSetPinAttenuation(PIN_PHOTO2, ADC_11db);
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
    pinMode(PIN_LED1, OUTPUT);
    pinMode(PIN_LED2, OUTPUT);
    pinMode(PIN_LED3, OUTPUT);

    // Krátký boot test 3 LED nad senzory (běžící světlo D1 -> D4 -> D5)
    digitalWrite(PIN_LED1, HIGH); delay(100); digitalWrite(PIN_LED1, LOW);
    digitalWrite(PIN_LED2, HIGH); delay(100); digitalWrite(PIN_LED2, LOW);
    digitalWrite(PIN_LED3, HIGH); delay(100); digitalWrite(PIN_LED3, LOW);
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

    // 3. Spodní panel neodpověděl v časovém limitu -> neblokujeme systém, linka naskočí za běhu
    Serial.println("[HANDSHAKE] Spodni panel zatim neodpovedel (UART se synchronizuje automaticky za behu).");
    SerialESP.println("BOOT:START");
    return true;
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
