/**
 * @file config.h
 * @brief Hlavní konfigurační soubor pro Spodní panel (ESP-Demo-Box - Maturitní projekt)
 *
 * Hardwarová platforma: ESP32-WROOM-32 (38 pinů)
 * Framework:            Arduino / PlatformIO
 */

#pragma once

// =============================================================================
//  MODULÁRNÍ PŘEPÍNAČE (Odkomentuj pro aktivaci daného modulu)
// =============================================================================

#define ENABLE_OLED_SSD1306   // 0.96" OLED displej (I2C 0x3C, Adafruit_SSD1306)
#define ENABLE_UART_TOP       // UART komunikace s Horním panelem (Serial2 / UART1)
#define ENABLE_JOYSTICK       // Analogový joystick (X, Y) a tlačítko (SW)
#define ENABLE_POTENTIOMETER  // Analogový potenciometr
#define ENABLE_ENCODER        // Rotační enkodér (CLK, DT)
#define ENABLE_BUTTONS        // Digitální tlačítka (1 až 5 / směrový kříž)
#define ENABLE_SWITCHES       // Páčkové přepínače (1 a 2)
#define ENABLE_SERVO_CLASSIC  // Klasické PWM servo (0-180°)
#define ENABLE_SERVO_CONT     // Kontinuální PWM servo (360°)
#define ENABLE_SMART_SERVO    // Jednodrátové Smart Servo LX-16A (UART)
#define ENABLE_MOTOR_CTRL     // Řízení motoru (PWM / Výkonový MOSFET)
#define ENABLE_LED_STRIP      // Adresovatelný LED pásek WS2812B
//#define ENABLE_I2C          // Samostatné skenování I2C sběrnice

// Kompatibilita pro novější názvy modulů
#define ENABLE_SERVO_SMART    ENABLE_SMART_SERVO
#define ENABLE_MOTOR_MOSFET   ENABLE_MOTOR_CTRL

// =============================================================================
//  SBĚRNICE A KOMUNIKACE
// =============================================================================

// I2C sběrnice pro OLED displej SSD1306
#define PIN_I2C_SDA         21
#define PIN_I2C_SCL         22

// UART linka pro komunikaci s horním panelem (křížem do horního ESP)
#define PIN_UART_TOP_TX     17   // Výstup TX -> vede do RX horního panelu
#define PIN_UART_TOP_RX     16   // Vstup RX  <- vede z TX horního panelu
#define PIN_SMART_SERVO     14   // Jednodrátový UART pro LX-16A Smart Servo
#define PIN_SERVO_SMA       PIN_SMART_SERVO

// =============================================================================
//  OLED DISPLEJ 0.96" (I2C SSD1306)
// =============================================================================
#define OLED_SCREEN_WIDTH  128
#define OLED_SCREEN_HEIGHT  64
#define OLED_RESET          -1   // Reset sdílený s procesorem
#define OLED_SCREEN_ADDRESS 0x3C // I2C adresa displeje
#define OLED_SCREEN_ADDR    OLED_SCREEN_ADDRESS

// =============================================================================
//  ANALOGOVÉ VSTUPY (ADC1 převodník – stabilní i při zapnuté Wi-Fi)
// =============================================================================
// V kódu v setup(): pinMode(PIN_..., INPUT); čtení přes analogRead(PIN_...)
#define PIN_JOY_X           36   // Osa X joysticku (VP / Sensor_VP, 0 až 3.3 V)
#define PIN_JOY_Y           39   // Osa Y joysticku (VN / Sensor_VN, 0 až 3.3 V)
#define PIN_POTENTIOMETER   34   // Otočný potenciometr (0 až 3.3 V)

// Aliasy pro alternativní značení os
#define PIN_JOY_VRX         PIN_JOY_X
#define PIN_JOY_VRY         PIN_JOY_Y

// =============================================================================
//  DIGITÁLNÍ VSTUPY – TLAČÍTKA A ENKODÉR (Active LOW – Spínají proti GND)
// =============================================================================
// DŮLEŽITÉ: Všechny tyto piny vyžadují aktivaci vnitřního pull-upu:
// V kódu v setup(): pinMode(PIN_..., INPUT_PULLUP);
// Stisknuto = LOW (0 V) | Uvolněno = HIGH (3.3 V)

#define PIN_JOY_SW           2   // Tlačítko joysticku (spíná GND -> bezpečný boot!)
#define PIN_BTN_1           12   // Tlačítko 1 / SW1 (Vlevo) - spíná GND (bezpečné pro Flash 3.3V)
#define PIN_BTN_2           23   // Tlačítko 2 / SW2 (Střed / OK) - spíná GND
#define PIN_BTN_3            5   // Tlačítko 3 / SW3 (Vpravo) - spíná GND
#define PIN_BTN_4           15   // Tlačítko 4 / SW4 (Nahoru) - spíná GND
#define PIN_BTN_5           26   // Tlačítko 5 / SW5 (Dolů) - spíná GND

#define PIN_ENC_CLK         32   // Rotační enkodér CLK -> INPUT_PULLUP
#define PIN_ENC_DT           4   // Rotační enkodér DT  -> INPUT_PULLUP
// Poznámka: Tlačítko enkodéru (SW) zůstalo nezapojené (NC)

// Aliasy pro označení SW1 až SW5
#define PIN_BTN_SW1         PIN_BTN_1
#define PIN_BTN_SW2         PIN_BTN_2
#define PIN_BTN_SW3         PIN_BTN_3
#define PIN_BTN_SW4         PIN_BTN_4
#define PIN_BTN_SW5         PIN_BTN_5

// =============================================================================
//  DIGITÁLNÍ VSTUPY – PÁČKOVÉ PŘEPÍNAČE (Active HIGH)
// =============================================================================
// DŮLEŽITÉ: Tyto piny mají na desce externí 10k pull-down rezistory k zemi.
// V kódu v setup(): pinMode(PIN_..., INPUT); (NEPOUŽÍVAT INPUT_PULLUP!)
// Sepnuto (ON + svítí LED) = HIGH (3.3 V) | Vypnuto (OFF) = LOW (0 V)

#define PIN_SWITCH_1        18   // Páčkový přepínač 1 (běžné GPIO, bezpečný boot)
#define PIN_SWITCH_2        19   // Páčkový přepínač 2 (běžné GPIO, bezpečný boot)

// =============================================================================
//  VÝSTUPY (PWM, Motory a LED pásek)
// =============================================================================
// V kódu v setup(): pinMode(PIN_..., OUTPUT);
#define PIN_SERVO_CLASSIC   13   // Stupňové klasické servo (0-180°, PWM)
#define PIN_SERVO_CONT      27   // Kontinuální servo 360° (PWM)
#define PIN_MOTOR_CTRL      25   // Řízení MOSFET tranzistoru motoru (PWM / Digitál)
#define PIN_LED_STRIP       33   // Datový pin pro adresovatelný LED pásek WS2812B
#define WS2812B_NUM_LEDS     8   // Počet LED diod na spodním pásku

// Aliasy pro novější názvy výstupů
#define PIN_SERVO_STU       PIN_SERVO_CLASSIC
#define PIN_SERVO_KON       PIN_SERVO_CONT
#define PIN_MOTOR_MOSFET    PIN_MOTOR_CTRL

// =============================================================================
//  PARAMETRY LADĚNÍ A SYSTÉMU
// =============================================================================
#define SERIAL_BAUD      115200   // Rychlost USB UART0 (ladění do PC)
#define UART_TOP_BAUD    115200   // Rychlost UART pro spojení s horním panelem
#define LOOP_INTERVAL_MS   1000   // Interval výpisu v loop() [ms]
