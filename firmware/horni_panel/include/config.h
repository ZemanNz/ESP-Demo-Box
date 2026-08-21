/**
 * @file config.h
 * @brief Hlavní konfigurační soubor pro Horní senzorový panel (Maturitní projekt)
 *
 * Odkomentuj ENABLE_* co chceš kompilovat. Zakomentovaný modul se ignoruje.
 *
 * Hardwarová platforma: ESP32-S3 DevKitC (44 pinů)
 * Framework:            Arduino / PlatformIO
 */

#pragma once

// =============================================================================
//  SEZNAM MODULŮ – ODKOMENTUJ CO CHCEŠ POUŽÍVAT
// =============================================================================

//#define ENABLE_DHT            // Teplotní a vlhkostní senzor (DHT11)
//#define ENABLE_ULTRASONIC     // Ultrazvukový senzor HC-SR04
//#define ENABLE_IR_SENSORS     // IR digitální senzory (2x)
//#define ENABLE_PHOTORESISTORS // Analogové fotorezistory (2x)
#define ENABLE_BUTTONS        // Tlačítka (2x)
//#define ENABLE_BUZZER         // Pasivní bzučák
//#define ENABLE_LEDS           // Jednoduché LED diody (3x)
//#define ENABLE_WS2812B        // Adresovatelný LED pásek WS2812B (FastLED)
//#define ENABLE_74HC595        // Sedmisegmentový displej přes 74HC595 (bit-banging)
#define ENABLE_TFT_ST7789     // TFT 2.8" displej ST7789 (SPI, Adafruit_ST7789)
#define ENABLE_LSM6DS3        // Gyroskop/akcelerometr LSM6DS3 (I2C_0)
//#define ENABLE_LCD1602        // LCD displej 16x2 (I2C_0, adresa 0x27)
//#define ENABLE_VL53L0X        // Laserový senzor vzdálenosti VL53L0X (I2C_0, 0x29)
//#define ENABLE_TCS34725       // Barevný senzor TCS34725 (I2C_1/Wire1, 0x29)
#define ENABLE_UART_ESP       // UART komunikace s druhým ESP32
#define ENABLE_WIFI_WEB       // Wi-Fi a WebServer (SoftAP + Captive Portal + WebSocket)

// =============================================================================
//  MAPA PINŮ
// =============================================================================

// --- Analogové vstupy ---
#define PIN_PHOTO1            1   // Fotorezistor 1 (ADC)
#define PIN_PHOTO2            2   // Fotorezistor 2 (ADC)

// --- Digitální senzory ---
#define PIN_DHT               3   // DHT data pin (GPIO 3)
#define DHT_TYPE              DHT11 // Typ senzoru (DHT11)
#define PIN_DHT22             PIN_DHT // Zpětná kompatibilita

#define PIN_ULTRASONIC_TRIG   4   // HC-SR04 Trigger (GPIO 4)
#define PIN_ULTRASONIC_ECHO  18   // HC-SR04 Echo (přesunuto z GPIO 20 – 20 je vnitřní USB D+ pin!)
#define PIN_IR1              38   // IR Senzor 1 (přesunuto z GPIO 37 – 37 je Octal Flash pin!)
#define PIN_IMU_INT          45   // LSM6DS3 INT1 – Přerušení na bezpečném pinu GPIO 45 (GPIO 33-37 jsou Octal Flash/PSRAM!)
#define PIN_BTN1             47   // Tlačítko 1 (INPUT_PULLUP)
#define PIN_BTN2             48   // Tlačítko 2 / Reset (INPUT_PULLUP)

// --- Výstupy ---
#define PIN_BUZZER            7   // Pasivní bzučák (tone())
#define PIN_LED1             39   // LED 1
#define PIN_LED2             40   // LED 2
#define PIN_LED3             41   // LED 3
#define PIN_WS2812B          42   // WS2812B DATA
#define WS2812B_NUM_LEDS      8   // Počet LEDek na pásku

// --- 74HC595 sedmisegmentový displej ---
#define PIN_SEG_DATA         15   // DS – sériová data
#define PIN_SEG_CLK          16   // SH_CP – clock
#define PIN_SEG_LATCH        17   // ST_CP – latch (přesunuto z GPIO 19 – 19 je USB D- pin)
#define SEG_NUM_DIGITS        3   // Počet číslic
#define SEG_COMMON_ANODE   true   // true = Společná Anoda (aktivní nula)

// --- TFT ST7789 2.8" (SPI) ---
#define PIN_TFT_CS           10
#define PIN_TFT_MOSI         11
#define PIN_TFT_SCLK         12
#define PIN_TFT_DC           13
#define PIN_TFT_RST          14
#define PIN_TFT_MISO         -1
#define PIN_TFT_LED          21
#define TFT_W               240
#define TFT_H               320

// --- UART1 ---
#define UART_ESP_TX          17
#define UART_ESP_RX          18
#define UART_ESP_BAUD    115200

// --- I2C_0 (Wire) ---
#define I2C0_SDA              8
#define I2C0_SCL              9

// --- I2C_1 (Wire1) ---
#define I2C1_SDA              5
#define I2C1_SCL              6

// =============================================================================
//  LADĚNÍ
// =============================================================================
#define SERIAL_BAUD      115200
#define LOOP_INTERVAL_MS   1000   // Interval výpisu v loop() [ms]
