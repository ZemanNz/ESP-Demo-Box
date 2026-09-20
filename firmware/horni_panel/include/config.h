/**
 * @file config.h
 * @brief Hlavní konfigurační soubor pro Horní senzorový panel (ESP-Demo-Box - Maturitní projekt)
 *
 * Hardwarová platforma: ESP32-S3 DevKitC (44 pinů, Octal SPI Flash/PSRAM - OPI)
 * Framework:            Arduino / PlatformIO
 */

#pragma once

// =============================================================================
//  MODULÁRNÍ PŘEPÍNAČE (Odkomentuj pro aktivaci daného modulu)
// =============================================================================

#define ENABLE_DHT            // Senzor teploty a vlhkosti DHT11
#define ENABLE_ULTRASONIC     // Ultrazvukový senzor HC-SR04
#define ENABLE_IR_SENSORS     // IR digitální senzory (IR1, IR2)
#define ENABLE_PHOTORESISTORS // Analogové fotorezistory (2x)
#define ENABLE_BUTTONS        // Uživatelské tlačítko INFO
#define ENABLE_BUZZER         // Pasivní piezo bzučák
#define ENABLE_LEDS           // Indikační LED diody (3x)
#define ENABLE_WS2812B        // Adresovatelný LED pásek WS2812B (FastLED / NeoPixel)
#define ENABLE_MAX7219        // 3-místný sedmisegmentový displej (MAX7219)
//#define ENABLE_74HC595      // Původní sedmisegment přes 74HC595
#define ENABLE_TFT_ST7789     // TFT 2.8" displej (SPI)
#define ENABLE_LSM6DS3        // Gyroskop / akcelerometr LSM6DS3 (I2C_0)
#define ENABLE_LCD1602        // Znakový LCD displej 16x2 (I2C_0, 0x27)
#define ENABLE_VL53L0X        // Laserový senzor vzdálenosti VL53L0X (I2C_0, 0x29)
#define ENABLE_TCS34725       // Barevný RGB senzor TCS34725 (I2C_1, 0x29)
#define ENABLE_UART_ESP       // UART komunikace se spodním panelem (Serial1)
#define ENABLE_WIFI_WEB       // Wi-Fi a WebServer (SoftAP + Captive Portal + WebSocket)

// Kompatibilita pro novější názvy modulů
#define ENABLE_DHT11          ENABLE_DHT
#define ENABLE_PHOTORES       ENABLE_PHOTORESISTORS
#define ENABLE_BUTTON_INFO    ENABLE_BUTTONS
#define ENABLE_LED_STRIP      ENABLE_WS2812B
#define ENABLE_7SEG_DISPLAY   ENABLE_MAX7219
#define ENABLE_TFT_DISPLAY    ENABLE_TFT_ST7789
#define ENABLE_GYRO_LSM       ENABLE_LSM6DS3
#define ENABLE_LCD_1602       ENABLE_LCD1602
#define ENABLE_LASER_VL53     ENABLE_VL53L0X
#define ENABLE_COLOR_TCS      ENABLE_TCS34725
#define ENABLE_UART_BOTTOM    ENABLE_UART_ESP

// =============================================================================
//  SBĚRNICE A KOMUNIKACE
// =============================================================================

// --- UART linka se spodním panelem (křížem do spodního ESP) ---
#define UART_ESP_TX          17   // Výstup TX -> vede do RX spodního panelu
#define UART_ESP_RX          18   // Vstup RX  <- vede z TX spodního panelu
#define UART_ESP_BAUD    115200
#define PIN_UART_BOT_TX  UART_ESP_TX
#define PIN_UART_BOT_RX  UART_ESP_RX
#define UART_BOT_BAUD    UART_ESP_BAUD

// --- I2C_0 (Gyroskop LSM6DS3, LCD 1602, Laser VL53L0X) ---
#define I2C0_SDA              8
#define I2C0_SCL              9
#define PIN_I2C0_SDA     I2C0_SDA
#define PIN_I2C0_SCL     I2C0_SCL

// --- I2C_1 (Barevný senzor TCS34725 – oddělená sběrnice kvůli kolizi adresy 0x29) ---
#define I2C1_SDA              5
#define I2C1_SCL              6
#define PIN_I2C1_SDA     I2C1_SDA
#define PIN_I2C1_SCL     I2C1_SCL

// --- SPI Sběrnice pro 2.8" TFT displej (ST7789 / ILI9341) ---
#define PIN_TFT_CS           10
#define PIN_TFT_MOSI         11
#define PIN_TFT_SCLK         12
#define PIN_TFT_SCK          PIN_TFT_SCLK
#define PIN_TFT_DC           13
#define PIN_TFT_RST          14
#define PIN_TFT_MISO         -1
// Podsvícení TFT (LED) je hardwarově připojeno na +3.3V, čímž se uvolnil pin GPIO 21 pro ECHO!
#define PIN_TFT_LED          -1
#define TFT_W               240
#define TFT_H               320

// --- 3-místný sedmisegmentový displej (MAX7219 / 74HC595) ---
#define PIN_SEG_DATA         15   // Data / DIN
#define PIN_SEG_CLK          16   // Clock / CLK
#define PIN_SEG_LATCH        46   // CS / LOAD (přesunuto ze 17 na bezpečné GPIO 46)
#define PIN_7SEG_SDI         PIN_SEG_DATA
#define PIN_7SEG_SCLK        PIN_SEG_CLK
#define PIN_7SEG_LOAD        PIN_SEG_LATCH
#define SEG_NUM_DIGITS        3
#define SEG_COMMON_ANODE   true

// =============================================================================
//  ANALOGOVÉ VSTUPY (ADC1 převodník ESP32-S3)
// =============================================================================
// V kódu v setup(): pinMode(PIN_..., INPUT); čtení přes analogRead(pin)
#define PIN_PHOTO1            1   // Fotorezistor 1 (dělič s 10k k GND)
#define PIN_PHOTO2            2   // Fotorezistor 2 (dělič s 10k k GND)
#define PIN_PHOTO_1          PIN_PHOTO1
#define PIN_PHOTO_2          PIN_PHOTO2

// =============================================================================
//  DIGITÁLNÍ VSTUPY – TLAČÍTKA A PŘERUŠENÍ
// =============================================================================
// Tlačítko INFO (Spíná přímo proti GND v režimu Active LOW)
// DŮLEŽITÉ: Vyžaduje vnitřní pull-up: pinMode(PIN_BTN1, INPUT_PULLUP);
// Stisknuto = LOW (0 V) | Uvolněno = HIGH (3.3 V)
#define PIN_BTN1             47   // Tlačítko INFO
#define PIN_BTN_INFO         PIN_BTN1

// Tlačítko Reset je hardwarově připojeno přímo na systémový pin EN/RST procesoru
#define PIN_BTN2             -1

// Hardwarové přerušení z gyroskopu LSM6DS3 (INT1)
// Bezpečný pin GPIO 45 (GPIO 33-37 jsou interně Octal PSRAM / Flash!)
#define PIN_IMU_INT          45
#define PIN_GYRO_INT         PIN_IMU_INT

// =============================================================================
//  DIGITÁLNÍ SENZORY (NEPOUŽÍVAT INPUT_PULLUP)
// =============================================================================
// V kódu v setup(): pinMode(PIN_..., INPUT);

// Senzor teploty a vlhkosti DHT11 (modul má externí pull-up rezistor k +3.3V)
#define PIN_DHT               3
#define DHT_TYPE              DHT11
#define PIN_DHT22             PIN_DHT

// Ultrazvukový senzor HC-SR04
#define PIN_ULTRASONIC_TRIG   4   // Spouštěcí impuls (Výstup -> pinMode OUTPUT)
// Echo přichází přes napěťový dělič na bezpečných 3.3V. PULL-UP ZDE NEZAPÍNAT!
// Zapojeno na GPIO 21 (GPIO 33-37 NESMÍ být použito kvůli kolizi s Octal PSRAM!)
#define PIN_ULTRASONIC_ECHO  21   // Měření odezvy (Vstup -> pinMode INPUT)
#define PIN_US_TRIG          PIN_ULTRASONIC_TRIG
#define PIN_US_ECHO          PIN_ULTRASONIC_ECHO

// Infračervené senzory překážky
#define PIN_IR1              48   // IR Senzor 1 (přesunuto z RST na uvolněné GPIO 48)
#define PIN_IR2              38   // IR Senzor 2 (GPIO 38)
#define PIN_IR_1             PIN_IR1
#define PIN_IR_2             PIN_IR2

// =============================================================================
//  VÝSTUPY (Bzučák, LED diody a pásek)
// =============================================================================
// V kódu v setup(): pinMode(PIN_..., OUTPUT);
#define PIN_BUZZER            7   // Pasivní piezo bzučák (tone())
#define PIN_LED1             39   // LED D1 přes 220R k zemi
#define PIN_LED2             40   // LED D4 přes 220R k zemi
#define PIN_LED3             41   // LED D5 přes 220R k zemi
#define PIN_LED_1            PIN_LED1
#define PIN_LED_2            PIN_LED2
#define PIN_LED_3            PIN_LED3
#define PIN_WS2812B          42   // Datový pin pro adresovatelný LED pásek WS2812B
#define PIN_LED_STRIP        PIN_WS2812B
#define WS2812B_NUM_LEDS      8   // Počet LED diod na horním pásku

// =============================================================================
//  I2C ADRESY
// =============================================================================
#define ADDR_LCD_1602      0x27   // Znakový LCD displej 16x2 (PCF8574)
#define ADDR_GYRO_LSM      0x6A   // LSM6DS3 gyroskop (případně 0x6B)
#define ADDR_LASER_VL53    0x29   // VL53L0X laser dálkoměr (sběrnice I2C_0)
#define ADDR_COLOR_TCS     0x29   // TCS34725 barevný senzor (sběrnice I2C_1)

// =============================================================================
//  LADĚNÍ A SYSTÉM
// =============================================================================
#define SERIAL_BAUD      115200   // Rychlost USB ladění do PC (UART0)
#define LOOP_INTERVAL_MS   1000   // Interval výpisu v loop() [ms]

