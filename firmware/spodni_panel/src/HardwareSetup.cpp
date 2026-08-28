#include "HardwareSetup.h"
#include "config.h"
#include <Wire.h>

// =============================================================================
//  GLOBÁLNÍ INSTANCE HARDWARU A CHYBOVÝ STAV
// =============================================================================

String lastHardwareError = "";

// Sériová linka pro komunikaci s horním panelem (UART1)
#ifdef ENABLE_UART_TOP
HardwareSerial SerialTop(1);
#endif

// 0.96" OLED SSD1306 displej (I2C)
#ifdef ENABLE_OLED_SSD1306
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 display(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif

// WS2812B RGB LED pásek
#ifdef ENABLE_LED_STRIP
#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel strip(WS2812B_NUM_LEDS, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);
#endif

// Jednodrátové Smart Servo LX-16A
#ifdef ENABLE_SMART_SERVO
#include "SmartServoBus.hpp"
using namespace lx16a;
SmartServoBus servoBus;
#endif

// Klasické a Kontinuální Servo (ESP32 PWM)
#if defined(ENABLE_SERVO_CLASSIC) || defined(ENABLE_SERVO_CONT)
#include <ESP32Servo.h>
#endif

#ifdef ENABLE_SERVO_CLASSIC
Servo servoClassic;
#endif

#ifdef ENABLE_SERVO_CONT
Servo servoCont;
#endif

// =============================================================================
//  INICIALIZACE JEDNOTLIVÝCH MODULŮ
// =============================================================================

bool setupI2C() {
#if defined(ENABLE_I2C) || defined(ENABLE_OLED_SSD1306)
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Serial.printf("[I2C] Sbernice inicializovana (SDA=%d, SCL=%d)\n", PIN_I2C_SDA, PIN_I2C_SCL);
#endif
    return true;
}

bool setupDisplay() {
#ifdef ENABLE_OLED_SSD1306
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_SCREEN_ADDRESS)) {
        lastHardwareError = "OLED SSD1306 neodpovida!";
        Serial.println(F("[OLED] CHYBA – Inicializace SSD1306 selhala!"));
        return false;
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 10);
    display.println(F("ESP-Demo-Box"));
    display.setCursor(10, 30);
    display.println(F("Spodni Panel OK"));
    display.display();
    Serial.printf("[OLED] OK – SSD1306 inicializovan (0x%02X)\n", OLED_SCREEN_ADDRESS);
#endif
    return true;
}

bool setupServosAndMotors() {
    Serial.println("[SETUP] Inicializace serv a motoru...");

    // Alokace PWM časovačů pro ESP32Servo
#if defined(ENABLE_SERVO_CLASSIC) || defined(ENABLE_SERVO_CONT)
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
#endif

    // Klasické servo (0-180°)
#ifdef ENABLE_SERVO_CLASSIC
    servoClassic.setPeriodHertz(50);
    servoClassic.attach(PIN_SERVO_CLASSIC, 500, 2500);
    servoClassic.write(90);
    Serial.printf("[SERVO_CLASSIC] OK – Pripojeno na GPIO %d\n", PIN_SERVO_CLASSIC);
#endif

    // Kontinuální servo (360°)
#ifdef ENABLE_SERVO_CONT
    servoCont.setPeriodHertz(50);
    servoCont.attach(PIN_SERVO_CONT, 1000, 2000);
    servoCont.write(90); // 90 = zastaveno
    Serial.printf("[SERVO_CONT] OK – Pripojeno na GPIO %d\n", PIN_SERVO_CONT);
#endif

    // Řízení DC motoru (PWM)
#ifdef ENABLE_MOTOR_CTRL
    pinMode(PIN_MOTOR_CTRL, OUTPUT);
    analogWrite(PIN_MOTOR_CTRL, 0);
    Serial.printf("[MOTOR_CTRL] OK – Vystup motoru na GPIO %d\n", PIN_MOTOR_CTRL);
#endif

    // Smart Servo LX-16A (Jednodrátový UART)
#ifdef ENABLE_SMART_SERVO
    pinMode((gpio_num_t)PIN_SMART_SERVO, INPUT_PULLUP);
    servoBus.begin(2, UART_NUM_2, (gpio_num_t)PIN_SMART_SERVO);
    Serial.printf("[SMART_SERVO] Testuji LX-16A na GPIO %d...\n", PIN_SMART_SERVO);
    delay(500);
    uint8_t id = servoBus.getId();
    if (id == 255) {
        lastHardwareError = "Smart Servo LX-16A neodpovida!";
        Serial.println(F("[SMART_SERVO] CHYBA: LX-16A neodpovídá na ping!"));
        return false;
    }
    Serial.printf("[SMART_SERVO] OK – LX-16A nalezeno s ID %d\n", id);
#endif

    return true;
}

bool setupInputs() {
    Serial.println("[SETUP] Inicializace senzoru a tlacitek...");

    // Joystick
#ifdef ENABLE_JOYSTICK
    pinMode(PIN_JOY_SW, INPUT_PULLUP);
    pinMode(PIN_JOY_X, INPUT);
    pinMode(PIN_JOY_Y, INPUT);
#endif

    // Potenciometr
#ifdef ENABLE_POTENTIOMETER
    pinMode(PIN_POTENTIOMETER, INPUT);
#endif

    // Rotační enkodér
#ifdef ENABLE_ENCODER
    pinMode(PIN_ENC_CLK, INPUT_PULLUP);
    pinMode(PIN_ENC_DT, INPUT_PULLUP);
#endif

    // 5x Tlačítka
#ifdef ENABLE_BUTTONS
    pinMode(PIN_BTN_1, INPUT_PULLUP);
    pinMode(PIN_BTN_2, INPUT_PULLUP);
    pinMode(PIN_BTN_3, INPUT_PULLUP);
    pinMode(PIN_BTN_4, INPUT_PULLUP);
    pinMode(PIN_BTN_5, INPUT_PULLUP);
#endif

    // 2x Páčkové přepínače
#ifdef ENABLE_SWITCHES
    pinMode(PIN_SWITCH_1, INPUT);
    pinMode(PIN_SWITCH_2, INPUT);
#endif

    Serial.println("[INPUTS] OK – Vsechny vstupni piny nastaveny.");
    return true;
}

bool setupLedStrip() {
#ifdef ENABLE_LED_STRIP
    strip.begin();
    strip.setBrightness(50);
    strip.show();
    Serial.printf("[LED_STRIP] OK – WS2812B inicializovan na GPIO %d (%d LED)\n", PIN_LED_STRIP, WS2812B_NUM_LEDS);
#endif
    return true;
}

bool setupUART() {
#ifdef ENABLE_UART_TOP
    SerialTop.begin(115200, SERIAL_8N1, PIN_UART_TOP_RX, PIN_UART_TOP_TX);
    Serial.printf("[UART_TOP] OK – Linka na horni panel spustena (TX=%d, RX=%d, 115200 baud)\n",
                  PIN_UART_TOP_TX, PIN_UART_TOP_RX);
#endif
    return true;
}

// =============================================================================
//  HLAVNÍ SDRUŽUJÍCÍ FUNKCE (MASTER SETUP)
// =============================================================================
bool initializeAllHardware() {
    Serial.println("\n=====================================");
    Serial.println("  STARTUJE HARDWARE SPODNIHO PANELU  ");
    Serial.println("=====================================");

    lastHardwareError = "";

    if (!setupI2C())             return false;
    if (!setupDisplay())         return false;
    if (!setupServosAndMotors()) return false;
    if (!setupInputs())          return false;
    if (!setupLedStrip())        return false;
    if (!setupUART())            return false;

    Serial.println("-------------------------------------");
    Serial.println(">> Spodni panel: Hardware PRIPRAVEN <<");
    Serial.println("=====================================\n");
    return true;
}

// =============================================================================
//  BOOT HANDSHAKE A HLÁŠENÍ CHYB
// =============================================================================
bool waitForHandshakeWithTop(bool hardwareOk, const String& errorMsg) {
#ifdef ENABLE_UART_TOP
    Serial.println("[HANDSHAKE] Zahajuji synchronizaci s hornim panelem...");

    // Pokud hardware selhal, posíláme chybovou hlášku
    if (!hardwareOk) {
        Serial.printf("[HANDSHAKE] Hlasim chybu hornimu panelu: %s\n", errorMsg.c_str());
        
#ifdef ENABLE_OLED_SSD1306
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println(F("KRITICKA CHYBA!"));
        display.setCursor(0, 16);
        display.println(errorMsg);
        display.display();
#endif

        // Neustále vysíláme chybu, aby ji horní panel mohl zachytit kdykoliv dokončí svůj pomalejší boot
        while (true) {
            SerialTop.printf("BOOT:ERR:%s\n", errorMsg.c_str());
            delay(200);
        }
        return false;
    }

    // Hardware je OK -> posíláme BOOT:READY a čekáme na potvrzení BOOT:START
    SerialTop.flush();
    unsigned long lastSend = 0;

    while (true) {
        unsigned long now = millis();
        if (now - lastSend >= 150) {
            lastSend = now;
            SerialTop.println("BOOT:READY");
        }

        if (SerialTop.available()) {
            String resp = SerialTop.readStringUntil('\n');
            resp.trim();
            if (resp == "BOOT:START") {
                Serial.println("[HANDSHAKE] Horni panel potvrdil start (BOOT:START)! Prechazim do provozu.");
                return true;
            }
        }
        delay(10);
    }
#else
    return true;
#endif
}
