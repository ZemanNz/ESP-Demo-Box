/**
 * @file main.cpp
 * @brief Příklad: Analogový potenciometr pro ESP32
 * @details Tento kód demonstruje čtení napětí z otočného potenciometru
 *          připojeného ke spodnímu panelu s ESP32-WROOM.
 */

#include <Arduino.h>

// Definice GPIO pinu podle pinové mapy spodního panelu
#define PIN_POTENTIOMETER  34  // Analogový vstup ADC1_CHANNEL_6 pro potenciometr

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("\n=== ESP-Demo-Box: Spodní panel – Analogový Potenciometr ==="));

  // GPIO 34 je čistě vstupní analogový pin
  pinMode(PIN_POTENTIOMETER, INPUT);

  Serial.println(F("Inicializace dokončena. Čtu polohu potenciometru...\n"));
}

void loop() {
  // Čtení analogové hodnoty (rozsah 0 až 4095)
  int rawValue = analogRead(PIN_POTENTIOMETER);

  // Přepočet surovinové ADC hodnoty na reálné napětí ve Voltech (0.00 V až 3.30 V)
  float voltage = rawValue * (3.3f / 4095.0f);

  // Přepočet na procenta (0 % až 100 %)
  float percentage = (rawValue / 4095.0f) * 100.0f;

  // Vykreslení jednoduchého ASCII ukazatele (progress bar)
  char bar[21];
  int filledLength = map(rawValue, 0, 4095, 0, 20);
  for (int i = 0; i < 20; i++) {
    bar[i] = (i < filledLength) ? '=' : ' ';
  }
  bar[20] = '\0';

  // Výpis do Sériového monitoru
  Serial.printf("[POTENTIOMETER] Surová ADC hodnota: %4d | Napětí: %.2f V | [%s] %.1f %%\n",
                rawValue, voltage, bar, percentage);

  // Prodleva 200 ms mezi výpisy
  delay(200);
}
