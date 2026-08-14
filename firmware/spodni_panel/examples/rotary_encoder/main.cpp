/**
 * @file main.cpp
 * @brief Příklad: Rotační enkodér (KY-040) s obsluhou přerušení (ISR) a hardwarovým debouncingem
 * @details Tento kód demonstruje čtení inkrementálního rotačního enkodéru s tlačítkem
 *          připojeného ke spodnímu panelu s ESP32-WROOM.
 */

#include <Arduino.h>

// Definice GPIO pinů podle pinové mapy spodního panelu
#define PIN_ENC_CLK  32  // Hodinový signál (CLK / Output A)
#define PIN_ENC_DT    4  // Datový signál (DT / Output B)
#define PIN_JOY_SW   18  // Tlačítko enkodéru / joysticku (SW / Switch)

// Globální proměnné přístupné z přerušení (volatile)
volatile int encoderCount = 0;
volatile bool buttonPressed = false;
volatile unsigned long lastClkTime = 0;

// Obsluha přerušení pro otáčení enkodéru
void IRAM_ATTR handleEncoderISR() {
  unsigned long nowUs = micros();
  // Softwarový filtr zákmitů (debouncing 2000 us = 2 ms)
  if (nowUs - lastClkTime > 2000) {
    int clkState = digitalRead(PIN_ENC_CLK);
    int dtState  = digitalRead(PIN_ENC_DT);

    if (clkState == LOW) {
      if (dtState == HIGH) {
        encoderCount++; // Otáčení po směru hodinových ručiček (CW)
      } else {
        encoderCount--; // Otáčení proti směru hodinových ručiček (CCW)
      }
      lastClkTime = nowUs;
    }
  }
}

// Obsluha přerušení pro stisk tlačítka
void IRAM_ATTR handleButtonISR() {
  static unsigned long lastBtnTime = 0;
  unsigned long nowMs = millis();
  if (nowMs - lastBtnTime > 250) {
    buttonPressed = true;
    lastBtnTime = nowMs;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("\n=== ESP-Demo-Box: Spodní panel – Rotační Enkodér (KY-040) ==="));

  // Nastavení pinů s interním PULL-UPem
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_JOY_SW, INPUT_PULLUP);

  // Připojení přerušení pro detekci klesající hrany (FALLING)
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), handleEncoderISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_JOY_SW), handleButtonISR, FALLING);

  Serial.println(F("Enkodér inicializován s přerušením (ISR). Otáčej knoflíkem...\n"));
}

void loop() {
  static int lastReportedCount = 9999;

  // Pokud došlo k pohybu enkodéru, vypíšeme novou polohu
  if (encoderCount != lastReportedCount) {
    lastReportedCount = encoderCount;
    Serial.printf("[ENCODER] Pozice: %d\n", lastReportedCount);
  }

  // Pokud bylo stisknuto tlačítko
  if (buttonPressed) {
    buttonPressed = false;
    Serial.printf("[ENCODER] Tlačítko STISKNUTO -> Resetuji počítadlo na 0!\n");
    encoderCount = 0;
    lastReportedCount = 0;
  }

  delay(20);
}
