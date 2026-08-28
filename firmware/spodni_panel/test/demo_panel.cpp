/**
 * @file demo_panel.cpp
 * @brief Spodní senzorový a akční panel – ESP-Demo-Box (Maturitní projekt)
 */

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// =============================================================================
//  PODMÍNĚNÉ INKLUDY A INSTANCE
// =============================================================================

#ifdef ENABLE_SMART_SERVO
  #include "SmartServoBus.hpp"
  using namespace lx16a;
  static SmartServoBus servoBus;
#endif

#if defined(ENABLE_SERVO_CLASSIC) || defined(ENABLE_SERVO_CONT)
  #include <ESP32Servo.h>
#endif

#ifdef ENABLE_SERVO_CLASSIC
  static Servo servoClassic;
  static int servoClassicAngle = 0;
  static int servoClassicDir = 10;
#endif

#ifdef ENABLE_SERVO_CONT
  static Servo servoCont;
#endif

#ifdef ENABLE_LED_STRIP
  #include <Adafruit_NeoPixel.h>
  static Adafruit_NeoPixel strip(WS2812B_NUM_LEDS, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);
  static uint8_t g_stripHue = 0;
#endif

#ifdef ENABLE_UART_TOP
  static HardwareSerial SerialTop(1);
#endif

#ifdef ENABLE_OLED_SSD1306
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  static Adafruit_SSD1306 display(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif

// =============================================================================
//  GLOBÁLNÍ STAV
// =============================================================================
static unsigned long g_lastLoop = 0;

#ifdef ENABLE_SMART_SERVO
  static uint8_t g_smartServoId = 0;
#endif

#ifdef ENABLE_ENCODER
  static volatile int g_encoderPos = 0;
  static int g_lastClkState = HIGH;
#endif

// =============================================================================
//  POMOCNÉ FUNKCE
// =============================================================================

#ifdef ENABLE_LED_STRIP
static uint32_t wheelColor(byte wheelPos) {
  wheelPos = 255 - wheelPos;
  if (wheelPos < 85) {
    return strip.Color(255 - wheelPos * 3, 0, wheelPos * 3);
  }
  if (wheelPos < 170) {
    wheelPos -= 85;
    return strip.Color(0, wheelPos * 3, 255 - wheelPos * 3);
  }
  wheelPos -= 170;
  return strip.Color(wheelPos * 3, 255 - wheelPos * 3, 0);
}
#endif

#ifdef ENABLE_I2C
static void scanI2C() {
  Serial.println(F("[I2C] Skenuji sběrnici..."));
  byte error, address;
  int nDevices = 0;
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("  -> Nalezeno I2C zařízení na adrese 0x%02X\n", address);
      nDevices++;
    } else if (error == 4) {
      Serial.printf("  -> Neznámá chyba na adrese 0x%02X\n", address);
    }
  }
  if (nDevices == 0) {
    Serial.println(F("  -> Žádná I2C zařízení nenalezena."));
  }
}
#endif

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);
  Serial.println(F("\n=== Spodní Panel (ESP32-WROOM) – Start ==="));
  Serial.flush();

  // ── Alokace PWM časovačů pro ESP32Servo ──────────────────────────────────
#if defined(ENABLE_SERVO_CLASSIC) || defined(ENABLE_SERVO_CONT)
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
#endif

  // ── I2C Sběrnice ─────────────────────────────────────────────────────────
#ifdef ENABLE_I2C
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Serial.printf("[I2C] Sběrnice inicializována (SDA=%d, SCL=%d)\n", PIN_I2C_SDA, PIN_I2C_SCL);
  Serial.flush();
  scanI2C();
#endif

  // ── 0.96" OLED SSD1306 (I2C 0x3C) ─────────────────────────────────────────
#ifdef ENABLE_OLED_SSD1306
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_SCREEN_ADDRESS)) {
    Serial.println(F("[OLED SSD1306] CHYBA – Inicializace selhala! Zkontroluj I2C adresi 0x3C a zapojení."));
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 10);
    display.println(F("ESP-Demo-Box"));
    display.setCursor(10, 30);
    display.println(F("Spodni Panel OK"));
    display.display();
    Serial.printf("[OLED SSD1306] OK – Inicializovan na I2C (SDA=%d, SCL=%d, 0x3C)\n", PIN_I2C_SDA, PIN_I2C_SCL);
  }
  Serial.flush();
#endif

  // ── UART Komunikace s Horním Panelem ─────────────────────────────────────
#ifdef ENABLE_UART_TOP
  SerialTop.begin(115200, SERIAL_8N1, PIN_UART_TOP_RX, PIN_UART_TOP_TX);
  SerialTop.println("HELLO_FROM_SPODNI_PANEL");
  Serial.printf("[UART_TOP] OK – TX=%d, RX=%d\n", PIN_UART_TOP_TX, PIN_UART_TOP_RX);
  Serial.flush();
#endif

  // ── Smart Servo LX-16A ───────────────────────────────────────────────────
#ifdef ENABLE_SMART_SERVO
  pinMode((gpio_num_t)PIN_SMART_SERVO, INPUT_PULLUP);
  servoBus.begin(2, UART_NUM_2, (gpio_num_t)PIN_SMART_SERVO);
  Serial.printf("[SMART_SERVO] Inicializuji LX-16A na GPIO %d...\n", PIN_SMART_SERVO);
  delay(1000); // Nutná pauza pro srovnání sběrnice a spuštění úlohy

  uint8_t currentId = servoBus.getId();
  if (currentId == 255) {
    Serial.println(F("[SMART_SERVO] CHYBA: Žádné servo nebylo nalezeno (broadcast 254 neodpovídá)!"));
    Serial.println(F("              Zkontroluj: 1) Napájení serva (potřebuje 6V-7.4V, nestačí 3.3V)"));
    Serial.println(F("                          2) Společné GND mezi ESP32 a napájením serva"));
    Serial.println(F("                          3) Signal vodič z rozbočovací desky na GPIO 14"));
  } else {
    Serial.printf("[SMART_SERVO] Úspěšně nalezeno servo s ID: %d\n", currentId);
    g_smartServoId = currentId;
    if (currentId != 0) {
      Serial.printf("[SMART_SERVO] Měním ID serva z %d na ID 0...\n", currentId);
      servoBus.setId(0);
      delay(500);
      uint8_t verifiedId = servoBus.getId();
      Serial.printf("[SMART_SERVO] Ověření: Nové ID serva je %d\n", verifiedId);
      if (verifiedId == 0) {
        g_smartServoId = 0;
      }
    }
  }
  Serial.flush();
#endif

  // ── Klasické Servo (PWM) ─────────────────────────────────────────────────
#ifdef ENABLE_SERVO_CLASSIC
  servoClassic.setPeriodHertz(50);
  servoClassic.attach(PIN_SERVO_CLASSIC, 500, 2500);
  servoClassic.write(90);
  Serial.printf("[SERVO_CLASSIC] Klasické servo připojeno na GPIO %d\n", PIN_SERVO_CLASSIC);
  Serial.flush();
#endif

  // ── Kontinuální Servo (PWM) ──────────────────────────────────────────────
#ifdef ENABLE_SERVO_CONT
  servoCont.setPeriodHertz(50);
  servoCont.attach(PIN_SERVO_CONT, 1000, 2000);
  servoCont.write(90); // Zastaveno
  Serial.printf("[SERVO_CONT] Kontinuální servo připojeno na GPIO %d\n", PIN_SERVO_CONT);
  Serial.flush();
#endif

  // ── Řízení motoru ────────────────────────────────────────────────────────
#ifdef ENABLE_MOTOR_CTRL
  pinMode(PIN_MOTOR_CTRL, OUTPUT);
  analogWrite(PIN_MOTOR_CTRL, 0);
  Serial.printf("[MOTOR_CTRL] Výstup motoru inicializován na GPIO %d\n", PIN_MOTOR_CTRL);
  Serial.flush();
#endif

  // ── WS2812B LED Pásek ────────────────────────────────────────────────────
#ifdef ENABLE_LED_STRIP
  strip.begin();
  strip.setBrightness(50);
  strip.show();
  Serial.printf("[LED_STRIP] WS2812B inicializován na GPIO %d (%d LED)\n", PIN_LED_STRIP, WS2812B_NUM_LEDS);
  Serial.flush();
#endif

  // ── Joystick ─────────────────────────────────────────────────────────────
#ifdef ENABLE_JOYSTICK
  pinMode(PIN_JOY_SW, INPUT_PULLUP);
  Serial.printf("[JOYSTICK] Inicializován (X=GPIO %d, Y=GPIO %d, SW=GPIO %d)\n", PIN_JOY_X, PIN_JOY_Y, PIN_JOY_SW);
  Serial.flush();
#endif

  // ── Potenciometr ─────────────────────────────────────────────────────────
#ifdef ENABLE_POTENTIOMETER
  pinMode(PIN_POTENTIOMETER, INPUT);
  Serial.printf("[POTENTIOMETER] Potenciometr inicializován na GPIO %d\n", PIN_POTENTIOMETER);
  Serial.flush();
#endif

  // ── Rotační Enkodér ──────────────────────────────────────────────────────
#ifdef ENABLE_ENCODER
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  g_lastClkState = digitalRead(PIN_ENC_CLK);
  Serial.printf("[ENCODER] Rotační enkodér inicializován na CLK=%d, DT=%d\n", PIN_ENC_CLK, PIN_ENC_DT);
  Serial.flush();
#endif

  // ── Digitální Tlačítka (1-5) ─────────────────────────────────────────────
#ifdef ENABLE_BUTTONS
  pinMode(PIN_BTN_1, INPUT_PULLUP);
  pinMode(PIN_BTN_2, INPUT_PULLUP);
  pinMode(PIN_BTN_3, INPUT_PULLUP);
  pinMode(PIN_BTN_4, INPUT_PULLUP);
  pinMode(PIN_BTN_5, INPUT_PULLUP);
  Serial.printf("[BUTTONS] Tlačítka 1-5 inicializována na GPIO %d, %d, %d, %d, %d\n",
                PIN_BTN_1, PIN_BTN_2, PIN_BTN_3, PIN_BTN_4, PIN_BTN_5);
  Serial.flush();
#endif

  // ── Páčkové Přepínače (1-2) ──────────────────────────────────────────────
#ifdef ENABLE_SWITCHES
  pinMode(PIN_SWITCH_1, INPUT);
  pinMode(PIN_SWITCH_2, INPUT);
  Serial.printf("[SWITCHES] Přepínače 1-2 inicializovány na GPIO %d, %d\n", PIN_SWITCH_1, PIN_SWITCH_2);
  Serial.flush();
#endif

  Serial.println(F("\n[SETUP] Hotovo. Spouštím hlavní smyčku...\n"));
  Serial.flush();
  g_lastLoop = millis();
}

// =============================================================================
//  LOOP – neblokující, interval LOOP_INTERVAL_MS
// =============================================================================
void loop() {
  unsigned long now = millis();

  // ── Čtení rychlých událostí (mimo interval) ──────────────────────────────
#ifdef ENABLE_ENCODER
  int clkState = digitalRead(PIN_ENC_CLK);
  if (clkState != g_lastClkState && clkState == LOW) {
    if (digitalRead(PIN_ENC_DT) != clkState) {
      g_encoderPos++;
    } else {
      g_encoderPos--;
    }
    Serial.printf("[ENCODER] Poloha: %d\n", g_encoderPos);
  }
  g_lastClkState = clkState;
#endif

#ifdef ENABLE_UART_TOP
  while (SerialTop.available()) {
    String msg = SerialTop.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) {
      Serial.print(F("[UART_TOP] Příchozí zpráva: "));
      Serial.println(msg);
    }
  }
#endif

  // ── Mimo hlavní časovací interval ────────────────────────────────────────
  if (now - g_lastLoop < LOOP_INTERVAL_MS) {
    delay(10); // Nakrmí Task Watchdog (WDT)
    return;
  }
  g_lastLoop = now;

  Serial.println(F("-----------------------------------------"));
  Serial.printf("[%lu ms]\n", now);

  // ── Smart Servo LX-16A ───────────────────────────────────────────────────
#ifdef ENABLE_SMART_SERVO
  {
    static float targetDeg = 0.0f;
    Angle targetAngle = Angle::deg(targetDeg);

    Serial.printf("[SMART_SERVO] Příkaz k přesunu ID %d na: %.1f deg\n", g_smartServoId, targetDeg);
    servoBus.set(g_smartServoId, targetAngle);

    Angle currentAngle = servoBus.pos(g_smartServoId);
    if (!currentAngle.isNaN()) {
      Serial.printf("[SMART_SERVO] Aktuální pozice: %.1f deg\n", currentAngle.deg());
    } else {
      Serial.printf("[SMART_SERVO] Pozice pro ID %d neodpovídá (offline/timeout)\n", g_smartServoId);
    }

    targetDeg += 60.0f;
    if (targetDeg > 240.0f) {
      targetDeg = 0.0f;
    }
  }
#endif

  // ── Klasické Servo ───────────────────────────────────────────────────────
#ifdef ENABLE_SERVO_CLASSIC
  servoClassicAngle += servoClassicDir;
  if (servoClassicAngle >= 180 || servoClassicAngle <= 0) {
    servoClassicDir = -servoClassicDir;
  }
  servoClassic.write(servoClassicAngle);
  Serial.printf("[SERVO_CLASSIC] Úhel: %d deg\n", servoClassicAngle);
#endif

  // ── Kontinuální Servo ────────────────────────────────────────────────────
#ifdef ENABLE_SERVO_CONT
  {
    static int speedState = 0;
    int speeds[] = {90, 120, 90, 60}; // Stop, Vpřed, Stop, Vzad
    int s = speeds[speedState % 4];
    servoCont.write(s);
    Serial.printf("[SERVO_CONT] Nastavená rychlost/hodnota: %d\n", s);
    speedState++;
  }
#endif

  // ── Řízení motoru ────────────────────────────────────────────────────────
#ifdef ENABLE_MOTOR_CTRL
  {
    static int motorSpeed = 0;
    motorSpeed = (motorSpeed + 64) % 256;
    analogWrite(PIN_MOTOR_CTRL, motorSpeed);
    Serial.printf("[MOTOR_CTRL] PWM Výkon motoru: %d / 255\n", motorSpeed);
  }
#endif

  // ── LED Pásek WS2812B ────────────────────────────────────────────────────
#ifdef ENABLE_LED_STRIP
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, wheelColor(((i * 256 / strip.numPixels()) + g_stripHue) & 255));
  }
  strip.show();
  g_stripHue += 20;
  Serial.println(F("[LED_STRIP] Obnoven efekt duhy na LED pásku."));
#endif

  // ── Joystick ─────────────────────────────────────────────────────────────
#ifdef ENABLE_JOYSTICK
  {
    int joyX = analogRead(PIN_JOY_X);
    int joyY = analogRead(PIN_JOY_Y);
    bool joySw = (digitalRead(PIN_JOY_SW) == LOW);
    Serial.printf("[JOYSTICK] X: %d | Y: %d | Tlačítko (SW): %s\n", joyX, joyY, joySw ? "STISKNUTO" : "Uvolněno");
  }
#endif

  // ── Potenciometr ─────────────────────────────────────────────────────────
#ifdef ENABLE_POTENTIOMETER
  {
    int potVal = analogRead(PIN_POTENTIOMETER);
    float volt = potVal * (3.3f / 4095.0f);
    Serial.printf("[POTENTIOMETER] Hodnota: %d | Napětí: %.2f V\n", potVal, volt);
  }
#endif

  // ── Digitální Tlačítka ───────────────────────────────────────────────────
#ifdef ENABLE_BUTTONS
  {
    bool b1 = (digitalRead(PIN_BTN_1) == LOW);
    bool b2 = (digitalRead(PIN_BTN_2) == LOW);
    bool b3 = (digitalRead(PIN_BTN_3) == LOW);
    bool b4 = (digitalRead(PIN_BTN_4) == LOW);
    bool b5 = (digitalRead(PIN_BTN_5) == LOW);
    Serial.printf("[BUTTONS] B1:%d | B2:%d | B3:%d | B4:%d | B5:%d\n", b1, b2, b3, b4, b5);
  }
#endif

  // ── Páčkové Přepínače ────────────────────────────────────────────────────
#ifdef ENABLE_SWITCHES
  {
    bool sw1 = (digitalRead(PIN_SWITCH_1) == HIGH);
    bool sw2 = (digitalRead(PIN_SWITCH_2) == HIGH);
    Serial.printf("[SWITCHES] Přepínač 1: %s | Přepínač 2: %s\n",
                  sw1 ? "ZAPNUTO (HIGH)" : "VYPNUTO (LOW)",
                  sw2 ? "ZAPNUTO (HIGH)" : "VYPNUTO (LOW)");
  }
#endif

  // ── UART Odeslání do Horního Panelu ──────────────────────────────────────
#ifdef ENABLE_UART_TOP
  {
    static uint32_t counter = 0;
    SerialTop.printf("SPODNI:%lu\n", counter);
    Serial.printf("[UART_TOP] > Odesláno: SPODNI:%lu\n", counter++);
  }
#endif

  // ── 0.96" OLED SSD1306 ────────────────────────────────────────────────────
#ifdef ENABLE_OLED_SSD1306
  {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("=== SPODNI PANEL ==="));
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    display.setCursor(0, 16);
    display.printf("Uptime: %lu s", now / 1000UL);

#ifdef ENABLE_JOYSTICK
    display.setCursor(0, 28);
    display.printf("JOY X:%4d Y:%4d", analogRead(PIN_JOY_X), analogRead(PIN_JOY_Y));
#endif

#ifdef ENABLE_POTENTIOMETER
    display.setCursor(0, 40);
    display.printf("POT: %4d", analogRead(PIN_POTENTIOMETER));
#endif

#ifdef ENABLE_ENCODER
    display.setCursor(0, 52);
    display.printf("ENC Pos: %d", g_encoderPos);
#endif

    display.display();
    Serial.println(F("[OLED SSD1306] Obrazovka obnovena."));
  }
#endif

  Serial.println();
}
