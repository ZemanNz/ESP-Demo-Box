/**
 * @file config.h
 * @brief Hlavní konfigurační soubor pro Spodní desku / panel (ESP-Demo-Box - Maturitní projekt)
 *
 * Odkomentuj ENABLE_* co chceš kompilovat a testovat. Zakomentovaný modul se ignoruje.
 *
 * Hardwarová platforma: ESP32-WROOM-32 (38 pinů)
 * Framework:            Arduino / PlatformIO
 */

#pragma once

// =============================================================================
//  SEZNAM MODULŮ – ODKOMENTUJ CO CHCEŠ POUŽÍVAT
// =============================================================================

//#define ENABLE_SMART_SERVO    // Jednodrátové Smart Servo LX-16A (UART)
//#define ENABLE_SERVO_CLASSIC  // Klasické PWM servo (0-180°)
//#define ENABLE_SERVO_CONT     // Kontinuální PWM servo
//#define ENABLE_MOTOR_CTRL     // Řízení motoru (PWM výstup)
//#define ENABLE_LED_STRIP      // Adresovatelný LED pásek WS2812B
//#define ENABLE_JOYSTICK       // Analogový joystick (X, Y) a tlačítko (SW)
//#define ENABLE_POTENTIOMETER  // Analogový potenciometr
//#define ENABLE_ENCODER        // Rotační enkodér (CLK, DT)
//#define ENABLE_BUTTONS        // Digitální tlačítka (1 až 5)
//#define ENABLE_SWITCHES       // Páčkové přepínače (1 a 2)
//#define ENABLE_I2C            // I2C sběrnice a skenování zařízení
//#define ENABLE_UART_TOP       // UART komunikace s Horním panelem

// =============================================================================
//  MAPA PINŮ - SPODNÍ DESKA (ESP32-WROOM)
// =============================================================================

// --- Sběrnice a Komunikace ---
#define PIN_I2C_SDA         21
#define PIN_I2C_SCL         22
#define PIN_UART_TOP_TX     17
#define PIN_UART_TOP_RX     16
#define PIN_SMART_SERVO     14   // Jednodrátový UART (LX-16A)

// --- Analogové vstupy ---
#define PIN_JOY_X           36   // VP / Sensor_VP
#define PIN_JOY_Y           39   // VN / Sensor_VN
#define PIN_POTENTIOMETER   34

// --- Výstupy (PWM & Motory & LED) ---
#define PIN_SERVO_CLASSIC   13
#define PIN_SERVO_CONT      27
#define PIN_MOTOR_CTRL      25
#define PIN_LED_STRIP       33
#define WS2812B_NUM_LEDS     8   // Počet LEDek na pásku

// --- Digitální vstupy (PULLUP - stačí spínat proti GND) ---
#define PIN_ENC_CLK         32
#define PIN_ENC_DT          4
#define PIN_JOY_SW          18
#define PIN_BTN_1           19
#define PIN_BTN_2           23
#define PIN_BTN_3            5
#define PIN_BTN_4           15
#define PIN_BTN_5           26   // Interní pull-up, nepotřebuje externí rezistor

// --- Přepínače (Active HIGH + 10k PULLDOWN proti GND) ---
#define PIN_SWITCH_1         2
#define PIN_SWITCH_2        12

// =============================================================================
//  PARAMETRY LADĚNÍ A SYSTÉMU
// =============================================================================
#define SERIAL_BAUD      115200
#define LOOP_INTERVAL_MS   1000   // Interval výpisu v loop() [ms]
