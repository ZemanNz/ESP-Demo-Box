/**
 * @file main.cpp
 * @brief Příklad: Analogový joystick (X, Y) a integrované tlačítko (SW) pro ESP32
 * @details Tento kód demonstruje čtení dvousměrného analogového joysticku (např. KY-023 / HW-504)
 *          připojeného ke spodnímu panelu s ESP32-WROOM.
 */

#include <Arduino.h>

// Definice GPIO pinů podle pinové mapy spodního panelu
#define PIN_JOY_X   36  // VP (Sensor_VP) - Analogový vstup osy X
#define PIN_JOY_Y   39  // VN (Sensor_VN) - Analogový vstup osy Y
#define PIN_JOY_SW  18  // Digitální vstup s interním PULL-UPem pro tlačítko joysticku

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("\n=== ESP-Demo-Box: Spodní panel – Analogový Joystick ==="));

  // Nastavení pinu tlačítka jako vstup s vnitřním pull-up rezistorem
  // Tlačítko joysticku spíná výstup na GND (při stisku hodnota LOW)
  pinMode(PIN_JOY_SW, INPUT_PULLUP);

  // Poznotečka k analogovým pinům:
  // GPIO 36 (VP) a GPIO 39 (VN) jsou čistě vstupní piny převodníku ADC1.
  pinMode(PIN_JOY_X, INPUT);
  pinMode(PIN_JOY_Y, INPUT);

  Serial.println(F("Inicializace dokončena. Čtu hodnoty joysticku...\n"));
}

void loop() {
  // Čtení analogových hodnot z ADC převodníku (rozsah 0 až 4095 pro 0V až 3.3V)
  int valX = analogRead(PIN_JOY_X);
  int valY = analogRead(PIN_JOY_Y);

  // Čtení stavu tlačítka (LOW = stisknuto, HIGH = uvolněno)
  bool isPressed = (digitalRead(PIN_JOY_SW) == LOW);

  // Přepočet analogových hodnot na procentuální vychýlení ze středu (-100 % až +100 %)
  // Střed joysticku je přibližně v hodnotě 2048
  int percentX = map(valX, 0, 4095, -100, 100);
  int percentY = map(valY, 0, 4095, -100, 100);

  // Výpis naměřených dat do Sériového monitoru
  Serial.printf("[JOYSTICK] X: %4d (%+4d %%) | Y: %4d (%+4d %%) | Tlačítko: %s\n",
                valX, percentX,
                valY, percentY,
                isPressed ? "STISKNUTO [X]" : "Uvolněno [ ]");

  // Krátká prodleva mezi výpisy (200 ms)
  delay(200);
}
