#include "HardwareSetup.h"
#include "config.h"
#include <Wire.h>
#include <SPI.h>

#include "GraphicsManager.h"
// Objekt gfx je deklarován v GraphicsManager.h a vytvořen v GraphicsManager.cpp

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
  Adafruit_NeoPixel builtinPixel(1, 38, NEO_GRB + NEO_KHZ800);
#endif

// Pomocná funkce pro 74HC595 (vymazání při startu)
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
// Vykreslení řádku POST diagnostické tabulky na TFT displej
// ---------------------------------------------------------
static void drawPostRow(int rowIdx, const char* name, bool ok, const char* errDetail = nullptr) {
    int y = 42 + rowIdx * 19;
#ifdef ENABLE_TFT_ST7789
    // 1. Smazání plochy řádku
    gfx.fillRect(0, y, 320, 18, ST77XX_BLACK);

    // 2. Název komponenty vlevo
    gfx.drawTextPartial(16, y + 4, name, ST77XX_WHITE, ST77XX_BLACK, 1);

    // 3. Šipka uprostřed
    gfx.drawTextPartial(168, y + 4, "->", ST77XX_CYAN, ST77XX_BLACK, 1);

    // 4. Stav vpravo: zelené [ BOOT OK ] nebo červené [ FAILED ]
    if (ok) {
        gfx.drawTextPartial(200, y + 4, "[ BOOT OK ]", ST77XX_GREEN, ST77XX_BLACK, 1);
    } else {
        gfx.drawTextPartial(200, y + 4, "[ FAILED ]", ST77XX_RED, ST77XX_BLACK, 1);
    }
#endif

    if (ok) {
        Serial.printf("[POST] %-20s -> OK\n", name);
    } else {
        Serial.printf("[POST] %-20s -> FAILED! (%s)\n", name, errDetail ? errDetail : "Chyba");
    }
    delay(40); // Krátká vizuální pauza pro efekt postupného testování
}

// ---------------------------------------------------------
// Inicializace displeje ST7789
// ---------------------------------------------------------
bool setupDisplay() {
#ifdef ENABLE_TFT_ST7789
    #ifdef PIN_TFT_LED
        pinMode(PIN_TFT_LED, OUTPUT);
        digitalWrite(PIN_TFT_LED, HIGH);
    #endif

    // Inicializace SPI sběrnice
    SPI.begin(PIN_TFT_SCLK, PIN_TFT_MISO, PIN_TFT_MOSI, PIN_TFT_CS);
    
    // Inicializace samotného ST7789 řadiče
    if (!gfx.init()) {
        Serial.println("[TFT] CHYBA: Inicializace displeje ST7789 selhala!");
        return false;
    }
    
    // Úvodní vyčištění a záhlaví POST tabulky
    gfx.clearScreen(ST77XX_BLACK);
    gfx.fillRect(0, 0, 320, 34, 0x1125); // Tmavě modro-šedá lišta
    gfx.drawTextPartial(16, 12, "ESP-DEMO-BOX: KONTROLA HARDWARU", ST77XX_WHITE, 0x1125, 1);
    gfx.drawLine(0, 34, 320, 34, ST77XX_CYAN);

    drawPostRow(0, "1. TFT ST7789 SPI", true);
#endif
    return true; 
}

// ---------------------------------------------------------
// Inicializace ostatních GPIO periferií (LED, Bzučák, Piny)
// ---------------------------------------------------------
static void setupGpioPeripherals() {
#ifdef ENABLE_ULTRASONIC
    pinMode(PIN_ULTRASONIC_TRIG, OUTPUT);
    pinMode(PIN_ULTRASONIC_ECHO, INPUT);
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
#endif

#ifdef ENABLE_IR_SENSORS
    analogSetPinAttenuation(PIN_IR1, ADC_11db); // IR1 analog vzdálenost (GPIO 20)
    pinMode(PIN_IR1, INPUT);
    pinMode(PIN_IR2, INPUT);                    // IR2 digitální překážka (GPIO 38)
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
    digitalWrite(PIN_BUZZER, BUZZER_SILENT_LEVEL); // Klidový stav = TICHO
#endif

#ifdef ENABLE_LEDS
    pinMode(PIN_LED1, OUTPUT);
    pinMode(PIN_LED2, OUTPUT);
    pinMode(PIN_LED3, OUTPUT);

    // Krátký boot test 3 LED nad senzory (D1 -> D4 -> D5)
    digitalWrite(PIN_LED1, HIGH); delay(80); digitalWrite(PIN_LED1, LOW);
    digitalWrite(PIN_LED2, HIGH); delay(80); digitalWrite(PIN_LED2, LOW);
    digitalWrite(PIN_LED3, HIGH); delay(80); digitalWrite(PIN_LED3, LOW);
#endif

#if defined(PIN_TCS_LED) && (PIN_TCS_LED >= 0)
    // LEDC PWM regulace pro přisvětlovací LED senzoru barev
    ledcSetup(LEDC_CHAN_TCS_LED, LEDC_FREQ_TCS_LED, LEDC_RES_TCS_LED);
    ledcAttachPin(PIN_TCS_LED, LEDC_CHAN_TCS_LED);
    ledcWrite(LEDC_CHAN_TCS_LED, 60);
#endif

#ifdef ENABLE_WS2812B
    strip.begin();
    strip.setBrightness(60);
    strip.show(); // Vypne všechny LED

    builtinPixel.begin();
    builtinPixel.clear();
    builtinPixel.show();
#endif

#ifdef ENABLE_74HC595
    pinMode(PIN_SEG_DATA,  OUTPUT);
    pinMode(PIN_SEG_CLK,   OUTPUT);
    pinMode(PIN_SEG_LATCH, OUTPUT);
    seg_clear();
#endif
}

// ---------------------------------------------------------
// Hlavní sdružující funkce (Master Setup & POST diagnostika)
// ---------------------------------------------------------
bool initializeAllHardware() {
    Serial.println("\n=====================================");
    Serial.println("   STARTUJE HARDWARE HORNIHO PANELU  ");
    Serial.println("=====================================");

    // 1. DISPLEJ MUSÍ BÝT PRVNÍ!
    if (!setupDisplay()) {
        return false;
    }

    bool allPassed = true;

    // 2. I2C_0 Sběrnice
    bool i2c0Ok = Wire.begin(I2C0_SDA, I2C0_SCL);
    if (i2c0Ok) {
        Wire.setClock(400000); // 400 kHz Fast I2C
    } else {
        allPassed = false;
    }
    drawPostRow(1, "2. I2C_0 Sbernice", i2c0Ok, i2c0Ok ? nullptr : "SDA/SCL selhalo");

    // 3. Laserový dálkoměr VL53L0X
    bool laserOk = false;
    if (i2c0Ok) {
        vl53.setBus(&Wire);
        vl53.setTimeout(400);
        if (vl53.init()) {
            laserOk = true;
            vl53.startContinuous(100);
        }
    }
    if (!laserOk) allPassed = false;
    drawPostRow(2, "3. Laser VL53L0X", laserOk, laserOk ? nullptr : "0x29 neodpovida");

    // 4. Gyroskop a Akcelerometr LSM6DS3
    bool gyroOk = false;
    uint8_t lsmAddr = 0x6B;
    if (i2c0Ok) {
        if (lsm6ds3.begin_I2C(0x6B, &Wire)) {
            gyroOk = true;
            lsmAddr = 0x6B;
        } else if (lsm6ds3.begin_I2C(0x6A, &Wire)) {
            gyroOk = true;
            lsmAddr = 0x6A;
        }

        if (gyroOk) {
            lsm6ds3.setGyroRange(LSM6DS_GYRO_RANGE_500_DPS);
            lsm6ds3.setAccelRange(LSM6DS_ACCEL_RANGE_4_G);
            lsm6ds3.setGyroDataRate(LSM6DS_RATE_104_HZ);
            lsm6ds3.setAccelDataRate(LSM6DS_RATE_104_HZ);
            lsm6ds3.configInt1(false, false, false);

            Wire.beginTransmission(lsmAddr); Wire.write(0x58); Wire.write(0x90); Wire.endTransmission();
            Wire.beginTransmission(lsmAddr); Wire.write(0x5C); Wire.write(IMU_WAKEUP_DUR); Wire.endTransmission();
            Wire.beginTransmission(lsmAddr); Wire.write(0x5B); Wire.write(IMU_WAKEUP_THS); Wire.endTransmission();
            Wire.beginTransmission(lsmAddr); Wire.write(0x5E); Wire.write(0x20); Wire.endTransmission();
            
            #ifdef PIN_IMU_INT
            pinMode(PIN_IMU_INT, INPUT_PULLDOWN);
            attachInterrupt(digitalPinToInterrupt(PIN_IMU_INT), imuWakeupIsr, RISING);
            #endif
        }
    }
    if (!gyroOk) allPassed = false;
    drawPostRow(3, "4. Gyro LSM6DS3", gyroOk, gyroOk ? nullptr : "0x6B/6A neodpovida");

    // Inicializace LCD 1602 na I2C_0 (pokud je připojen)
#ifdef ENABLE_LCD1602
    if (i2c0Ok) {
        Wire.beginTransmission(0x27);
        if (Wire.endTransmission() == 0) {
            lcd.init();
            lcd.backlight();
            lcd.setCursor(0, 0);
            lcd.print("ESP-Demo-Box OK");
        }
    }
#endif

    // 5. Barevný RGB senzor TCS34725 (na I2C_1)
    bool tcsOk = false;
    bool i2c1Ok = Wire1.begin(I2C1_SDA, I2C1_SCL);
    if (i2c1Ok) {
        tcsOk = tcs.begin(TCS34725_ADDRESS, &Wire1);
    }
    if (!tcsOk) allPassed = false;
    drawPostRow(4, "5. Barva TCS34725", tcsOk, tcsOk ? nullptr : "I2C_1 neodpovida");

    // 6. Teploměr a Vlhkoměr DHT11
    bool dhtOk = false;
#if defined(ENABLE_DHT) || defined(ENABLE_DHT22)
    dht.begin();
    delay(80);
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) || !isnan(h)) {
        dhtOk = true;
    } else {
        delay(150);
        t = dht.readTemperature();
        h = dht.readHumidity();
        if (!isnan(t) || !isnan(h)) dhtOk = true;
    }
#endif
    if (!dhtOk) allPassed = false;
    drawPostRow(5, "6. Klima DHT11", dhtOk, dhtOk ? nullptr : "GPIO 16 neodpovida");

    // Inicializace zbývajících GPIO pinů (LEDky, bzučák, pásky)
    setupGpioPeripherals();

    // 7. UART Linka a 8. Spodní panel HW
    Serial.println("[SETUP] Inicializace UART linky a synchronizace se spodnim panelem...");
    SerialESP.begin(UART_ESP_BAUD, SERIAL_8N1, UART_ESP_RX, UART_ESP_TX);
    SerialESP.flush();

    bool uartLinkOk = false;
    bool bottomHwOk = false;
    String bottomErrReason = "";

    unsigned long uartStart = millis();
    while (millis() - uartStart < 2500) { // Timeout 2.5 s
        if (SerialESP.available()) {
            String msg = SerialESP.readStringUntil('\n');
            msg.trim();
            if (msg.length() > 0) {
                uartLinkOk = true; // Zpráva dorazila -> kabelové spojení funguje!
                if (msg == "BOOT:READY") {
                    bottomHwOk = true;
                    SerialESP.println("BOOT:START");
                    break;
                } else if (msg.startsWith("BOOT:ERR:")) {
                    bottomHwOk = false;
                    bottomErrReason = msg.substring(9);
                    break;
                }
            }
        }
        delay(10);
    }

    if (!uartLinkOk) {
        allPassed = false;
        drawPostRow(6, "7. UART Linka", false, "Odpojeny kabel");
        drawPostRow(7, "8. Spodni panel HW", false, "Nedostupny");
    } else {
        drawPostRow(6, "7. UART Linka", true);
        if (!bottomHwOk) allPassed = false;
        drawPostRow(7, "8. Spodni panel HW", bottomHwOk, bottomHwOk ? nullptr : bottomErrReason.c_str());
    }

    // =========================================================
    // VYHODNOCENÍ VÝSLEDKU POST TESTU
    // =========================================================
    if (allPassed) {
        // Vše v pořádku -> Zelený pruh, podržíme půl vteřiny a startujeme!
        gfx.fillRect(0, 198, 320, 42, 0x0320); // Tmavě zelený obdélník
        gfx.drawTextPartial(24, 214, ">> VSECHNY TESTY USPESNE (BOOT OK) <<", ST77XX_GREEN, 0x0320, 1);
        Serial.println("[POST] Vsechny moduly OK! Prechazim do hlavniho menu za 600 ms...");
        delay(600); // Půl sekundy pro přečtení uživatelem
        return true;
    } else {
        // Některý modul selhal -> Červený pruh, výstražný tón a čekání na stisk horního tlačítka
        gfx.fillRect(0, 196, 320, 44, ST77XX_RED);
        gfx.drawTextPartial(12, 202, "! CHYBA: ZKONTROLUJ ODPOJENY HARDWARE !", ST77XX_WHITE, ST77XX_RED, 1);
        gfx.drawTextPartial(12, 220, "Stiskni tlacitko TOP pro preskoceni...", ST77XX_YELLOW, ST77XX_RED, 1);

        Serial.println("[POST] CHYBA HARDWARU! Cekam na stisk horniho tlacitka pro preskoceni...");

        // Výstražné pípnutí bzučáku
        #ifdef PIN_BUZZER
        digitalWrite(PIN_BUZZER, BUZZER_ACTIVE_LEVEL);
        delay(180);
        digitalWrite(PIN_BUZZER, BUZZER_SILENT_LEVEL);
        #endif

        // Čekáme na stisk horního tlačítka (PIN_BTN1 = GPIO 47, sepnuto = LOW)
        pinMode(PIN_BTN1, INPUT_PULLUP);
        while (digitalRead(PIN_BTN1) == HIGH) {
            delay(20);
        }
        // Počkáme na uvolnění tlačítka
        while (digitalRead(PIN_BTN1) == LOW) {
            delay(20);
        }
        delay(150);

        // Odeslat spodnímu panelu povolení ke startu i přes chybu (pokud čekal v chybové smyčce)
        SerialESP.println("BOOT:START");
        Serial.println("[POST] Tlacitko TOP stisknuto -> Preskakuji do systemu...");
        return true;
    }
}
