/**
 * @file demo_panel.cpp
 * @brief Horní senzorový panel – Maturitní projekt
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "config.h"

// =============================================================================
//  PODMÍNĚNÉ INKLUDY
// =============================================================================

#if defined(ENABLE_DHT) || defined(ENABLE_DHT22)
  #include <DHT.h>
  #ifndef DHT_TYPE
    #define DHT_TYPE DHT11
  #endif
  #ifndef PIN_DHT
    #define PIN_DHT PIN_DHT22
  #endif
  DHT dht(PIN_DHT, DHT_TYPE);
#endif

#ifdef ENABLE_WS2812B
  #include <Adafruit_NeoPixel.h>
  Adafruit_NeoPixel strip(WS2812B_NUM_LEDS, PIN_WS2812B, NEO_GRB + NEO_KHZ800);
#endif

#ifdef ENABLE_TFT_ST7789
  #include <Adafruit_GFX.h>
  #include <Adafruit_ST7789.h>
  Adafruit_ST7789 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
#endif

#ifdef ENABLE_LSM6DS3
  #include <Adafruit_LSM6DS3.h>
  Adafruit_LSM6DS3 lsm6ds3;
  static float angleX = 0, angleY = 0, angleZ = 0;
  static float gyroBiasX = 0, gyroBiasY = 0, gyroBiasZ = 0;
  static unsigned long lsmLastUs = 0;
#endif

#ifdef ENABLE_LCD1602
  #include <LiquidCrystal_I2C.h>
  LiquidCrystal_I2C lcd(0x27, 16, 2);
#endif

#ifdef ENABLE_VL53L0X
  #include <VL53L0X.h>
  VL53L0X vl53;
#endif

#ifdef ENABLE_TCS34725
  #include <Adafruit_TCS34725.h>
  Adafruit_TCS34725 tcs(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
#endif

#ifdef ENABLE_UART_ESP
  HardwareSerial SerialESP(1);
#endif

// =============================================================================
//  GLOBÁLNÍ STAV
// =============================================================================
static unsigned long g_lastLoop = 0;
static bool          g_ledState = false;
static uint8_t       g_hue      = 0;
static uint16_t      g_segCount = 0;
static uint8_t       g_tftPage  = 0;

// =============================================================================
//  74HC595 SEDMISEGMENTOVÝ DISPLEJ
// =============================================================================
#ifdef ENABLE_74HC595
static byte seg_getPattern(int digit) {
  static const byte map[] = {
    0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F
  };
  if (digit < 0 || digit > 9)
    return SEG_COMMON_ANODE ? 0xFF : 0x00;
  byte p = map[digit];
  return SEG_COMMON_ANODE ? ~p : p;
}

static void seg_display(long value) {
  digitalWrite(PIN_SEG_LATCH, LOW);
  long tmp = value;
  for (int i = 0; i < SEG_NUM_DIGITS; i++) {
    int d = tmp % 10;
    byte p = seg_getPattern(d);
    if (tmp == 0 && i > 0 && value != 0) p = seg_getPattern(-1);
    if (value == 0 && i == 0)            p = seg_getPattern(0);
    shiftOut(PIN_SEG_DATA, PIN_SEG_CLK, MSBFIRST, p);
    tmp /= 10;
  }
  digitalWrite(PIN_SEG_LATCH, HIGH);
}
#endif

// =============================================================================
//  ULTRAZVUK
// =============================================================================
#ifdef ENABLE_ULTRASONIC
static float ultrasonicCm() {
  digitalWrite(PIN_ULTRASONIC_TRIG, LOW);  delayMicroseconds(4);
  digitalWrite(PIN_ULTRASONIC_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
  long d = pulseIn(PIN_ULTRASONIC_ECHO, HIGH, 40000UL);
  return (d == 0) ? -1.0f : d * 0.0343f / 2.0f;
}
#endif

// =============================================================================
//  LSM6DS3 KALIBRACE
// =============================================================================
#ifdef ENABLE_LSM6DS3
static void lsm_calibrate() {
  Serial.println(F("[LSM6DS3] Kalibrace – nehybejte senzorem 2 s..."));
  float sx = 0, sy = 0, sz = 0;
  for (int i = 0; i < 200; i++) {
    sensors_event_t a, g, t;
    lsm6ds3.getEvent(&a, &g, &t);
    sx += g.gyro.x; sy += g.gyro.y; sz += g.gyro.z;
    delay(10);
  }
  gyroBiasX = sx / 200; gyroBiasY = sy / 200; gyroBiasZ = sz / 200;
  Serial.printf("[LSM6DS3] Bias: X=%.4f Y=%.4f Z=%.4f\n",
                gyroBiasX, gyroBiasY, gyroBiasZ);
}
#endif

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);
  Serial.println(F("\n=== Horní Senzorový Panel – Start ==="));
  Serial.flush();

  // ── I2C ──────────────────────────────────────────────────────────────────
#if defined(ENABLE_LSM6DS3) || defined(ENABLE_LCD1602) || defined(ENABLE_VL53L0X)
  Wire.begin(I2C0_SDA, I2C0_SCL);
  Serial.printf("[I2C_0] Wire  SDA=%d SCL=%d\n", I2C0_SDA, I2C0_SCL); Serial.flush();
#endif

#ifdef ENABLE_TCS34725
  Wire1.begin(I2C1_SDA, I2C1_SCL);
  Serial.printf("[I2C_1] Wire1 SDA=%d SCL=%d\n", I2C1_SDA, I2C1_SCL); Serial.flush();
#endif

  // ── DHT ──────────────────────────────────────────────────────────────────
#if defined(ENABLE_DHT) || defined(ENABLE_DHT22)
  dht.begin();
  Serial.printf("[DHT]        OK – Inicializován senzor (Typ: DHT11) na GPIO %d\n", PIN_DHT);
  Serial.flush();
#endif

  // ── Ultrazvuk ────────────────────────────────────────────────────────────
#ifdef ENABLE_ULTRASONIC
  pinMode(PIN_ULTRASONIC_TRIG, OUTPUT);
  pinMode(PIN_ULTRASONIC_ECHO, INPUT);
  digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
  Serial.printf("[HC-SR04]    OK – TRIG=%d ECHO=%d\n",
                PIN_ULTRASONIC_TRIG, PIN_ULTRASONIC_ECHO); Serial.flush();
#endif

  // ── IR senzor 1 ──────────────────────────────────────────────────────────
#ifdef ENABLE_IR_SENSORS
  pinMode(PIN_IR1, INPUT);
  Serial.printf("[IR1]        OK – IR senzor 1 inicializován na GPIO %d\n", PIN_IR1);
  Serial.flush();
#endif

  // ── Fotorezistory ────────────────────────────────────────────────────────
#ifdef ENABLE_PHOTORESISTORS
  pinMode(PIN_PHOTO1, INPUT);
  pinMode(PIN_PHOTO2, INPUT);
  Serial.printf("[PHOTO]      OK – P1=%d P2=%d\n", PIN_PHOTO1, PIN_PHOTO2); Serial.flush();
#endif

  // ── Tlačítka ─────────────────────────────────────────────────────────────
#ifdef ENABLE_BUTTONS
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  Serial.printf("[BUTTONS]    OK – BTN1=%d BTN2=%d\n", PIN_BTN1, PIN_BTN2); Serial.flush();
#endif

  // ── Bzučák ───────────────────────────────────────────────────────────────
#ifdef ENABLE_BUZZER
  pinMode(PIN_BUZZER, OUTPUT);
  tone(PIN_BUZZER, 1000, 120); delay(180);
  tone(PIN_BUZZER, 2000, 80);  delay(130);
  Serial.printf("[BUZZER]     OK – GPIO %d\n", PIN_BUZZER); Serial.flush();
#endif

  // ── LED diody ────────────────────────────────────────────────────────────
#ifdef ENABLE_LEDS
  pinMode(PIN_LED1, OUTPUT); digitalWrite(PIN_LED1, LOW);
  pinMode(PIN_LED2, OUTPUT); digitalWrite(PIN_LED2, LOW);
  pinMode(PIN_LED3, OUTPUT); digitalWrite(PIN_LED3, LOW);
  Serial.printf("[LEDs]       OK – %d %d %d\n", PIN_LED1, PIN_LED2, PIN_LED3); Serial.flush();
#endif

  // ── WS2812B ──────────────────────────────────────────────────────────────
#ifdef ENABLE_WS2812B
  strip.begin();
  strip.setBrightness(60);
  strip.show();
  Serial.printf("[WS2812B]    OK – NeoPixel GPIO %d, %d LED\n", PIN_WS2812B, WS2812B_NUM_LEDS);
  Serial.flush();
#endif

  // ── 74HC595 sedmisegmentový displej ──────────────────────────────────────
#ifdef ENABLE_74HC595
  pinMode(PIN_SEG_DATA,  OUTPUT); digitalWrite(PIN_SEG_DATA,  LOW);
  pinMode(PIN_SEG_CLK,   OUTPUT); digitalWrite(PIN_SEG_CLK,   LOW);
  pinMode(PIN_SEG_LATCH, OUTPUT); digitalWrite(PIN_SEG_LATCH, HIGH);
  seg_display(0);
  Serial.printf("[74HC595]    OK – DATA=%d CLK=%d LATCH=%d\n",
                PIN_SEG_DATA, PIN_SEG_CLK, PIN_SEG_LATCH); Serial.flush();
#endif

  // ── TFT ST7789 (SPI) ─────────────────────────────────────────────────────
#ifdef ENABLE_TFT_ST7789
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
  tft.print("ESP32-S3 DEMO BOX");
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(20, 40);
  tft.print("Maturitni projekt 2025");
  Serial.println(F("[TFT ST7789] OK – SPI inicializovan")); Serial.flush();
#endif

  // ── LSM6DS3 gyroskop (I2C_0 – Wire) ─────────────────────────────────────
#ifdef ENABLE_LSM6DS3
  bool lsmOk = false;
  if      (lsm6ds3.begin_I2C(0x6A)) { lsmOk = true; Serial.println(F("[LSM6DS3]    OK – Wire 0x6A")); }
  else if (lsm6ds3.begin_I2C(0x6B)) { lsmOk = true; Serial.println(F("[LSM6DS3]    OK – Wire 0x6B")); }
  else                               { Serial.println(F("[LSM6DS3]    CHYBA – senzor nenalezen!")); }
  Serial.flush();
  if (lsmOk) {
    lsm6ds3.setGyroRange(LSM6DS_GYRO_RANGE_250_DPS);
    lsm6ds3.setAccelRange(LSM6DS_ACCEL_RANGE_2_G);
    lsm_calibrate();
    lsmLastUs = micros();
  }
#endif

  // ── LCD 1602 (I2C_0 – Wire, 0x27) ───────────────────────────────────────
#ifdef ENABLE_LCD1602
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("Demo Panel");
  lcd.setCursor(0, 1); lcd.print("Maturita 2025");
  Serial.println(F("[LCD1602]    OK – Wire 0x27")); Serial.flush();
#endif

  // ── VL53L0X laserový senzor (I2C_0 – Wire, 0x29) ────────────────────────
#ifdef ENABLE_VL53L0X
  vl53.setBus(&Wire);
  if (!vl53.init()) {
    Serial.println(F("[VL53L0X]    CHYBA – senzor nenalezen!"));
  } else {
    vl53.setTimeout(500);
    vl53.startContinuous(100);
    Serial.println(F("[VL53L0X]    OK – Wire 0x29"));
  }
  Serial.flush();
#endif

  // ── TCS34725 barevný senzor (I2C_1 – Wire1, 0x29) ───────────────────────
#ifdef ENABLE_TCS34725
  if (!tcs.begin(0x29, &Wire1)) {
    Serial.println(F("[TCS34725]   CHYBA – senzor nenalezen na Wire1!"));
  } else {
    Serial.println(F("[TCS34725]   OK – Wire1 0x29"));
  }
  Serial.flush();
#endif

  // ── UART1 ────────────────────────────────────────────────────────────────
#ifdef ENABLE_UART_ESP
  SerialESP.begin(UART_ESP_BAUD, SERIAL_8N1, UART_ESP_RX, UART_ESP_TX);
  SerialESP.println("HELLO_FROM_HORNI_PANEL");
  Serial.printf("[UART_ESP]   OK – TX=%d RX=%d\n", UART_ESP_TX, UART_ESP_RX); Serial.flush();
#endif

  Serial.println(F("\n[SETUP] Hotovo. Spoustim loop...\n")); Serial.flush();
  g_lastLoop = millis();
}

// =============================================================================
//  LOOP – neblokující, interval LOOP_INTERVAL_MS
// =============================================================================
void loop() {
  unsigned long now = millis();

  // ── Mimo interval ────────────────────────────────────────────────────────
  if (now - g_lastLoop < LOOP_INTERVAL_MS) {
    delay(10); // Nakrmí Task Watchdog (WDT)

#ifdef ENABLE_BUTTONS
    if (digitalRead(PIN_BTN1) == LOW) {
      Serial.println(F("[BTN1] Stisknuto!"));
#ifdef ENABLE_BUZZER
      tone(PIN_BUZZER, 800, 80);
#endif
      delay(200);
    }
    if (digitalRead(PIN_BTN2) == LOW) {
      Serial.println(F("[BTN2] Reset!"));
      delay(300);
      ESP.restart();
    }
#endif

#ifdef ENABLE_UART_ESP
    while (SerialESP.available()) {
      String m = SerialESP.readStringUntil('\n');
      m.trim();
      if (m.length()) { Serial.print(F("[UART_ESP] < ")); Serial.println(m); }
    }
#endif

#ifdef ENABLE_LSM6DS3
    {
      unsigned long nowUs = micros();
      float dt = (float)(nowUs - lsmLastUs) / 1e6f;
      lsmLastUs = nowUs;
      if (dt > 0.1f) dt = 0.01f;

      sensors_event_t a, g, t;
      lsm6ds3.getEvent(&a, &g, &t);
      float gx = g.gyro.x - gyroBiasX;
      float gy = g.gyro.y - gyroBiasY;
      float gz = g.gyro.z - gyroBiasZ;
      const float DZ = 0.03f;
      if (fabsf(gx) < DZ) gx = 0; if (fabsf(gy) < DZ) gy = 0; if (fabsf(gz) < DZ) gz = 0;
      const float R2D = 57.2957795f, alpha = 0.98f;
      float aRoll  = atan2f(a.acceleration.y, a.acceleration.z)  * R2D;
      float aPitch = atan2f(-a.acceleration.x,
                     sqrtf(a.acceleration.y*a.acceleration.y +
                           a.acceleration.z*a.acceleration.z)) * R2D;
      angleX = alpha*(angleX + gx*R2D*dt) + (1-alpha)*aRoll;
      angleY = alpha*(angleY + gy*R2D*dt) + (1-alpha)*aPitch;
      angleZ += gz * R2D * dt;
    }
#endif

    return;
  }
  g_lastLoop = now;

  Serial.println(F("-----------------------------------------"));
  Serial.printf("[%lu ms]\n", now);

  // ── DHT ──────────────────────────────────────────────────────────────────
#if defined(ENABLE_DHT) || defined(ENABLE_DHT22)
  {
    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();
    if (isnan(temp) || isnan(hum)) {
      Serial.println(F("[DHT11] Senzor neodpovídá nebo chybný kód (skontroluj VCC, GND, GPIO 3)."));
    } else {
      Serial.printf("[DHT11] Teplota: %.1f °C | Vlhkost: %.1f %%\n", temp, hum);
#ifdef ENABLE_LCD1602
      char b[17]; snprintf(b, 17, "T%.1fC H%.1f%%", temp, hum);
      lcd.setCursor(0, 0); lcd.print(b);
#endif
    }
  }
#endif

  // ── Ultrazvuk ────────────────────────────────────────────────────────────
#ifdef ENABLE_ULTRASONIC
  {
    float d = ultrasonicCm();
    if (d < 0) Serial.println(F("[HC-SR04] Mimo dosah"));
    else        Serial.printf("[HC-SR04] %.1f cm\n", d);
  }
#endif

  // ── IR ───────────────────────────────────────────────────────────────────
#ifdef ENABLE_IR_SENSORS
  int ir1State = digitalRead(PIN_IR1);
  Serial.printf("[IR1] Stav (GPIO %d): %s\n", PIN_IR1, ir1State == LOW ? "DETEKCE (PŘEKÁŽKA)" : "volno");
#endif

  // ── Fotorezistory ────────────────────────────────────────────────────────
#ifdef ENABLE_PHOTORESISTORS
  Serial.printf("[PHOTO] P1=%d P2=%d\n", analogRead(PIN_PHOTO1), analogRead(PIN_PHOTO2));
#endif

  // ── LED blikání ──────────────────────────────────────────────────────────
#ifdef ENABLE_LEDS
  g_ledState = !g_ledState;
  digitalWrite(PIN_LED1,  g_ledState ? HIGH : LOW);
  digitalWrite(PIN_LED2,  g_ledState ? LOW  : HIGH);
  digitalWrite(PIN_LED3,  g_ledState ? HIGH : LOW);
  Serial.printf("[LEDs] %s\n", g_ledState ? "LED1+3 ON" : "LED2 ON");
#endif

  // ── TCS34725 barevný senzor ──────────────────────────────────────────────
#ifdef ENABLE_TCS34725
  float redF = 0, greenF = 0, blueF = 0;
  tcs.getRGB(&redF, &greenF, &blueF);
  uint8_t sensorR = (uint8_t)redF;
  uint8_t sensorG = (uint8_t)greenF;
  uint8_t sensorB = (uint8_t)blueF;
  Serial.printf("[TCS34725] Naměřená barva -> R:%d G:%d B:%d\n", sensorR, sensorG, sensorB);
#endif

  // ── WS2812B LED pásek ────────────────────────────────────────────────────
#ifdef ENABLE_WS2812B
  #ifdef ENABLE_TCS34725
    // Pokud je aktivní barevný senzor, pásek svítí barvou načtenou ze senzoru
    for (int i = 0; i < WS2812B_NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(sensorR, sensorG, sensorB));
    }
    strip.show();
    Serial.printf("[WS2812B] Pásek nastaven na barvu senzoru (R:%d G:%d B:%d)\n", sensorR, sensorG, sensorB);
  #else
    // Pokud barevný senzor vypnutý, běží duhová animace
    for (int i = 0; i < WS2812B_NUM_LEDS; i++) {
      uint8_t pos = g_hue + (i * 256 / WS2812B_NUM_LEDS);
      uint32_t color;
      if(pos < 85) {
        color = strip.Color(pos * 3, 255 - pos * 3, 0);
      } else if(pos < 170) {
        pos -= 85;
        color = strip.Color(255 - pos * 3, 0, pos * 3);
      } else {
        pos -= 170;
        color = strip.Color(0, pos * 3, 255 - pos * 3);
      }
      strip.setPixelColor(i, color);
    }
    strip.show();
    g_hue += 20;
    Serial.println(F("[WS2812B] Obnovení barvy LED pásku"));
  #endif
#endif

  // ── Bzučák ───────────────────────────────────────────────────────────────
#ifdef ENABLE_BUZZER
  { static uint8_t bc=0; if(++bc>=5){tone(PIN_BUZZER,2000,50);bc=0;} }
#endif

  // ── 74HC595 ──────────────────────────────────────────────────────────────
#ifdef ENABLE_74HC595
  seg_display(g_segCount);
  Serial.printf("[74HC595] %d\n", g_segCount);
  g_segCount = (g_segCount + 1) % 1000;
#endif

  // ── TFT ST7789 ───────────────────────────────────────────────────────────
#ifdef ENABLE_TFT_ST7789
  {
    tft.fillRect(0, 60, 320, 180, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 70);
    switch (g_tftPage % 3) {
      case 0:
        tft.setTextColor(ST77XX_CYAN);
        tft.println("=== Senzory ===");
#if defined(ENABLE_DHT) || defined(ENABLE_DHT22)
        { float t=dht.readTemperature(), h=dht.readHumidity();
          tft.setCursor(10,100); tft.printf("T: %.1f C", t);
          tft.setCursor(10,125); tft.printf("H: %.1f %%", h); }
#endif
        break;
      case 1:
        tft.setTextColor(ST77XX_YELLOW);
        tft.println("=== Vzdalenost ===");
#ifdef ENABLE_ULTRASONIC
        { float d=ultrasonicCm();
          tft.setCursor(10,100);
          if(d<0) tft.println("Mimo dosah"); else tft.printf("%.1f cm", d); }
#endif
        break;
      case 2:
        tft.setTextColor(ST77XX_MAGENTA);
        tft.println("=== Uptime ===");
        tft.setCursor(10,100);
        tft.printf("%lu s", millis()/1000UL);
        break;
    }
    g_tftPage++;
    Serial.printf("[TFT] stranka %d\n", g_tftPage%3);
  }
#endif

  // ── LSM6DS3 ──────────────────────────────────────────────────────────────
#ifdef ENABLE_LSM6DS3
  Serial.printf("[LSM6DS3] Roll:%.1f Pitch:%.1f Yaw:%.1f\n",
                angleX, angleY, angleZ);
#endif

  // ── VL53L0X ──────────────────────────────────────────────────────────────
#ifdef ENABLE_VL53L0X
  { uint16_t mm = vl53.readRangeContinuousMillimeters();
    if (vl53.timeoutOccurred()) Serial.println(F("[VL53L0X] Timeout!"));
    else Serial.printf("[VL53L0X] %d mm\n", mm); }
#endif

  // ── UART ─────────────────────────────────────────────────────────────────
#ifdef ENABLE_UART_ESP
  { static uint32_t n=0; SerialESP.printf("HORNI:%lu\n",n);
    Serial.printf("[UART_ESP] > HORNI:%lu\n",n++); }
#endif

  Serial.println();
}
