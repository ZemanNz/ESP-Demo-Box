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

#define ENABLE_OLED_SSD1306    // 0.96" OLED displej (I2C 0x3C, Adafruit_SSD1306)
#define ENABLE_UART_TOP        // UART komunikace s Horním panelem (Serial1 křížem do horního ESP)
#define ENABLE_JOYSTICK        // Analogový joystick (X, Y) a tlačítko (SW)
#define ENABLE_POTENTIOMETER   // Analogový potenciometr
#define ENABLE_ENCODER         // Rotační enkodér (CLK, DT)
#define ENABLE_BUTTONS         // Digitální tlačítka (1 až 5 / směrový kříž)
#define ENABLE_SWITCHES        // Páčkové přepínače (1 a 2)
#define ENABLE_SERVO_CLASSIC   // Klasické PWM servo (0-180°)
#define ENABLE_SERVO_CONT      // Kontinuální PWM servo (360°)
#define ENABLE_SMART_SERVO     // Jednodrátové Smart Servo LX-16A (UART)
#define ENABLE_LIMIT_SWITCHES  // Koncové spínače lineárního posuvu (MIN a MAX)
#define ENABLE_LED_STRIP       // Adresovatelný LED pásek WS2812B
//#define ENABLE_I2C           // Samostatné skenování I2C sběrnice

// Kompatibilita pro novější názvy modulů
#define ENABLE_SERVO_SMART     ENABLE_SMART_SERVO

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

// Limity úhlu pro Smart Servo LX-16A (stupně)
#define SMART_SERVO_MIN_DEG   0
#define SMART_SERVO_MAX_DEG  70

// AutoStop ochrana při mechanickém odporu (přetížení / sevření kostky):
// Rozdíl mezi požadovaným a reálným úhlem v centistupních (1° = 100 centistupňů).
// 1000 = 10.0° rozdíl pro detekci odporu
#define SMART_SERVO_AUTOSTOP_DIFF_CENTIDEG  1000
// Počet po sobě jdoucích měření s tímto rozdílem před zastavením serva (2x = 60 ms)
#define SMART_SERVO_AUTOSTOP_READINGS       2

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

#define PIN_JOY_SW           2   // Tlačítko joysticku (spíná GND -> bezpečný boot!) -> pinMode INPUT_PULLUP
#define PIN_BTN_1           12   // Tlačítko 1 / SW1 (Vlevo) - spíná GND (bezpečné pro Flash 3.3V) -> pinMode INPUT_PULLUP
#define PIN_BTN_2           23   // Tlačítko 2 / SW2 (Střed / OK) - spíná GND -> pinMode INPUT_PULLUP
#define PIN_BTN_3            5   // Tlačítko 3 / SW3 (Vpravo) - spíná GND -> pinMode INPUT_PULLUP
#define PIN_BTN_4           15   // Tlačítko 4 / SW4 (Nahoru) - spíná GND -> pinMode INPUT_PULLUP
#define PIN_BTN_5           26   // Tlačítko 5 / SW5 (Dolů) - spíná GND -> pinMode INPUT_PULLUP

#define PIN_ENC_CLK         32   // Rotační enkodér CLK -> pinMode INPUT_PULLUP
#define PIN_ENC_DT           4   // Rotační enkodér DT  -> pinMode INPUT_PULLUP
// Poznámka: Tlačítko enkodéru (SW) zůstalo nezapojené (NC)

// Aliasy pro označení SW1 až SW5
#define PIN_BTN_SW1         PIN_BTN_1
#define PIN_BTN_SW2         PIN_BTN_2
#define PIN_BTN_SW3         PIN_BTN_3
#define PIN_BTN_SW4         PIN_BTN_4
#define PIN_BTN_SW5         PIN_BTN_5

// =============================================================================
//  KONCOVÉ SPÍNAČE LINEÁRNÍHO POSUVU (Active LOW – Spínají proti GND)
// =============================================================================
// Koncový spínač MIN (Levý doraz – uvolněný pin po MOSFETu motoru):
// V kódu v setup(): pinMode(PIN_LIMIT_MIN, INPUT_PULLUP);
// Sepnuto (doraz) = LOW (0 V) | V klidu = HIGH (3.3 V)
#define PIN_LIMIT_MIN       25   // Interní pull-up čipu -> pinMode INPUT_PULLUP

// Koncový spínač MAX (Pravý doraz – čistě vstupní pin GPIO 35):
// POZOR: GPIO 35 nemá vnitřní pull-up, má na desce externí rezistor 10k k 3.3V!
// V kódu v setup(): pinMode(PIN_LIMIT_MAX, INPUT); (NEPOUŽÍVAT INPUT_PULLUP!)
// Sepnuto (doraz) = LOW (0 V) | V klidu = HIGH (3.3 V)
#define PIN_LIMIT_MAX       35   // Vyžaduje externí 10k k 3.3V -> pinMode INPUT

// Aliasy pro alternativní značení dorazů
#define PIN_ENDSTOP_MIN     PIN_LIMIT_MIN
#define PIN_ENDSTOP_MAX     PIN_LIMIT_MAX

// =============================================================================
//  DIGITÁLNÍ VSTUPY – PÁČKOVÉ PŘEPÍNAČE (Active HIGH)
// =============================================================================
// DŮLEŽITÉ: Tyto piny mají na desce externí 10k pull-down rezistory k zemi.
// V kódu v setup(): pinMode(PIN_..., INPUT); (NEPOUŽÍVAT INPUT_PULLUP!)
// Sepnuto (ON + svítí LED) = HIGH (3.3 V) | Vypnuto (OFF) = LOW (0 V)

#define PIN_SWITCH_1        18   // Páčkový přepínač 1 (běžné GPIO, bezpečný boot) -> pinMode INPUT
#define PIN_SWITCH_2        19   // Páčkový přepínač 2 (běžné GPIO, bezpečný boot) -> pinMode INPUT

// =============================================================================
//  VÝSTUPY (PWM Serva a LED pásek)
// =============================================================================
// V kódu v setup(): pinMode(PIN_..., OUTPUT);
#define PIN_SERVO_CLASSIC   13   // Stupňové klasické servo (0-180°, PWM)
#define PIN_SERVO_CONT      27   // Kontinuální servo lineárního posuvu (PWM)
#define PIN_LED_STRIP       33   // Datový pin pro adresovatelný LED pásek WS2812B
#define WS2812B_NUM_LEDS     8   // Počet LED diod na spodním pásku

// Aliasy pro alternativní názvy výstupů
#define PIN_SERVO_STU       PIN_SERVO_CLASSIC
#define PIN_SERVO_KON       PIN_SERVO_CONT

// =============================================================================
//  PARAMETRY LADĚNÍ A SYSTÉMU
// =============================================================================
#define SERIAL_BAUD      115200   // Rychlost USB UART0 (ladění do PC)
#define UART_TOP_BAUD    115200   // Rychlost UART pro spojení s horním panelem
#define LOOP_INTERVAL_MS   1000   // Interval výpisu v loop() [ms]
