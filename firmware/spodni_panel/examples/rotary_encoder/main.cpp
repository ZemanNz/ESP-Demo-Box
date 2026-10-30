/**
 * @file main.cpp
 * @brief Příklad: Rotační enkodér (KY-040) s obsluhou přerušení a stavovým automatem (State Machine)
 * @details Tento kód demonstruje spolehlivé čtení mechanického rotačního enkodéru.
 *          Používá tabulkový stavový automat Grayova kódu, který zcela eliminuje
 *          mechanické zákmity kontaktů (bouncing) a počítá přesně 1 krok na 1 fyzické cvaknutí.
 */

#include <Arduino.h>

// Definice GPIO pinů podle pinové mapy spodního panelu
#define PIN_ENC_CLK  32  // Hodinový signál (CLK / Output A)
#define PIN_ENC_DT    4  // Datový signál (DT / Output B)
#define PIN_JOY_SW    2  // Tlačítko joysticku / enkodéru pro reset (SW)

// Výsledky stavového automatu
#define DIR_NONE 0x00
#define DIR_CW   0x10
#define DIR_CCW  0x20

// ---------------------------------------------------------------------------
// Stavová tabulka kvadraturního signálu (Buxton State Machine)
// ---------------------------------------------------------------------------
const uint8_t PROGMEM ttable[7][4] = {
    // 00: START
    {0x0, 0x2, 0x4, 0x0},
    // 01: CW_FINAL
    {0x3, 0x0, 0x1, 0x10},
    // 02: CW_BEGIN
    {0x3, 0x2, 0x0, 0x0},
    // 03: CW_NEXT
    {0x3, 0x2, 0x1, 0x0},
    // 04: CCW_BEGIN
    {0x6, 0x0, 0x4, 0x0},
    // 05: CCW_FINAL
    {0x6, 0x5, 0x0, 0x20},
    // 06: CCW_NEXT
    {0x6, 0x5, 0x4, 0x0},
};

// Globální proměnné pro přerušení (volatile)
volatile int32_t encoderCount = 0;
volatile uint8_t encoderState = 0;

// Obsluha přerušení spouštěná při libovolné změně (CHANGE) na CLK i DT
void IRAM_ATTR handleEncoderISR() {
    uint8_t pinState = (digitalRead(PIN_ENC_CLK) << 1) | digitalRead(PIN_ENC_DT);
    encoderState = pgm_read_byte(&ttable[encoderState & 0x0F][pinState]);
    uint8_t result = encoderState & 0x30;

    if (result == DIR_CW) {
        encoderCount++;
    } else if (result == DIR_CCW) {
        encoderCount--;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println(F("\n=== ESP-Demo-Box: Spodní panel – Rotační Enkodér (State Machine) ==="));

    // Nastavení pinů s interním PULL-UPem
    pinMode(PIN_ENC_CLK, INPUT_PULLUP);
    pinMode(PIN_ENC_DT, INPUT_PULLUP);
    pinMode(PIN_JOY_SW, INPUT_PULLUP);

    // DŮLEŽITÉ: Sledujeme oba piny na každou změnu stavu (CHANGE)
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), handleEncoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_DT), handleEncoderISR, CHANGE);

    Serial.println(F("Enkodér inicializován. Otáčej knoflíkem...\n"));
}

void loop() {
    static int32_t lastReportedCount = 0;
    int32_t currentCount;

    noInterrupts();
    currentCount = encoderCount;
    interrupts();

    // Vypíšeme novou polohu pouze při fyzické změně o krok
    if (currentCount != lastReportedCount) {
        int32_t delta = currentCount - lastReportedCount;
        lastReportedCount = currentCount;
        Serial.printf("[ENCODER] Pozice: %4d | Zmena: %+2d (%s)\n",
                      currentCount, delta, (delta > 0) ? "DOPRAVA ->" : "<- DOLEVA");
    }

    // Reset tlačítkem
    if (digitalRead(PIN_JOY_SW) == LOW) {
        delay(20);
        if (digitalRead(PIN_JOY_SW) == LOW) {
            noInterrupts();
            encoderCount = 0;
            encoderState = 0;
            interrupts();
            lastReportedCount = 0;
            Serial.println(F(">>> [ENCODER] Tlačítko stisknuto: Pozice resetována na 0 <<<"));
            while (digitalRead(PIN_JOY_SW) == LOW) {
                delay(10);
            }
        }
    }

    delay(10);
}
