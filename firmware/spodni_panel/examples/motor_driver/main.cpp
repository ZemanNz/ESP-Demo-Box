/**
 * @file main.cpp
 * @brief Příklad: Jednovodičové řízení výstupu motoru (PWM / MOSFET budič) pro ESP32
 * @details Tento kód demonstruje plynulou regulaci výkonu DC motoru nebo PWM zátěže
 *          pomocí pulzně-šířkové modulace (PWM) na GPIO 25.
 */

#include <Arduino.h>

// Definice GPIO pinu podle pinové mapy spodního panelu
#define PIN_MOTOR_CTRL  25  // Výstupní PWM pin pro řízení budiče motoru

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("\n=== ESP-Demo-Box: Spodní panel – Řízení Výkonu Motoru (PWM) ==="));

  // Nastavení pinu motoru jako výstupního
  pinMode(PIN_MOTOR_CTRL, OUTPUT);

  // Inicializace výstupu na 0 (motor zastaven)
  analogWrite(PIN_MOTOR_CTRL, 0);

  Serial.println(F("Inicializace dokončena. Spouštím testovací cyklus zrychlování a zpomalování...\n"));
}

void loop() {
  // 1. PLYNULÉ ZRYCHLOVÁNÍ (Ramp-Up: 0 až 255)
  Serial.println(F(">>> Motor: Plynulé zrychlování (0 % -> 100 %)..."));
  for (int duty = 0; duty <= 255; duty += 5) {
    analogWrite(PIN_MOTOR_CTRL, duty);
    int percent = map(duty, 0, 255, 0, 100);
    Serial.printf("    PWM Hodnota: %3d / 255 (%3d %%)\n", duty, percent);
    delay(50); // Rychlost nárůstu výkonu
  }

  // 2. PLNOU RYCHLOSTÍ CHVÍLI BĚŽÍ (2 sekundy)
  Serial.println(F(">>> Motor: Běh na 100 % výkonu (2 s)..."));
  analogWrite(PIN_MOTOR_CTRL, 255);
  delay(2000);

  // 3. PLYNULÉ ZPOMALOVÁNÍ (Ramp-Down: 255 až 0)
  Serial.println(F(">>> Motor: Plynulé zpomalování (100 % -> 0 %)..."));
  for (int duty = 255; duty >= 0; duty -= 5) {
    analogWrite(PIN_MOTOR_CTRL, duty);
    int percent = map(duty, 0, 255, 0, 100);
    Serial.printf("    PWM Hodnota: %3d / 255 (%3d %%)\n", duty, percent);
    delay(50);
  }

  // 4. ZASTAVENÍ A PAUZA (3 sekundy)
  Serial.println(F(">>> Motor: Zastaveno. Pauza před dalším cyklem...\n"));
  analogWrite(PIN_MOTOR_CTRL, 0);
  delay(3000);
}
