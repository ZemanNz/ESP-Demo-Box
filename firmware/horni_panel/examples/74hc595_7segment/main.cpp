// Testovací kód pro sedmisegmentový displej s posuvnými registry 74HC595
// Využívá softwarové SPI (bit-banging) na pinech:
// - DIO (Data Input / DS): GPIO 7
// - SCLK (Shift Clock / SH_CP): GPIO 3
// - RCLK (Latch Clock / ST_CP / CS): GPIO 4
//
// Tento příklad je navržen pro výukové účely (maturitní projekt).

#include <Arduino.h>

// Definice pinů pro připojení displeje k ESP32-S3
#define PIN_DATA  7  // DIO / DS (Sériová data)
#define PIN_CLK   3  // SCLK / SH_CP (Hodinový signál)
#define PIN_CS    4  // RCLK / ST_CP (Latch / Chip Select)

// POČET ČÍSLIC NA TVÉM MODULU
// Nastav podle svého modulu (např. 2, 3 nebo 4 číslice)
const int NUM_DIGITS = 3;

// TYP DISPLEJE: SPOLEČNÁ ANODA (Common Anode) nebo SPOLEČNÁ KATODA (Common Cathode)
// Většina čínských modulů s 74HC595 používá Společnou Anodu (aktivní v nule).
// Pokud ti segmenty svítí obráceně (nesvítí to, co má, a svítí zbytek), změň na false.
const bool COMMON_ANODE = true; 

// Globální proměnná pro počítadlo (každých 500 ms se zvýší o 1)
unsigned long counter = 0;

// Funkce vracející segmentový vzor pro dané číslo (0-9)
// Mapování bitů: Bit 0 = segment A, Bit 1 = B, Bit 2 = C, ..., Bit 6 = G, Bit 7 = DP (tečka)
byte getSegmentPattern(int digit) {
  // Definice aktivních segmentů pro číslice 0 až 9 (pro Společnou Katodu - aktivní v 1)
  static const byte segmentMap[] = {
    0x3F, // 0 (A, B, C, D, E, F)
    0x06, // 1 (B, C)
    0x5B, // 2 (A, B, D, E, G)
    0x4F, // 3 (A, B, C, D, G)
    0x66, // 4 (B, C, F, G)
    0x6D, // 5 (A, C, D, F, G)
    0x7D, // 6 (A, C, D, E, F, G)
    0x07, // 7 (A, B, C)
    0x7F, // 8 (A, B, C, D, E, F, G)
    0x6F  // 9 (A, B, C, D, F, G)
  };

  // Pokud je požadováno prázdné místo (např. zhasnutí předních nul)
  if (digit < 0 || digit > 9) {
    return COMMON_ANODE ? 0xFF : 0x00; // Všechny segmenty vypnuté
  }

  byte pattern = segmentMap[digit];

  // Pokud je to Společná Anoda, invertujeme bity (0 = svítí, 1 = nesvítí)
  if (COMMON_ANODE) {
    pattern = ~pattern;
  }
  return pattern;
}

// Funkce pro odeslání hodnoty na displej
void displayValue(long value) {
  // Zahájení přenosu - Latch LOW
  digitalWrite(PIN_CS, LOW);

  long temp = value;

  // Postupně posíláme data pro jednotlivé číslice od poslední (pravé) k první (levé).
  // První odeslaná data doputují skrze kaskádu až do posledního posuvného registru.
  for (int i = 0; i < NUM_DIGITS; i++) {
    int digit = temp % 10;
    
    // Získání segmentového kódu pro danou cifru
    byte pattern = getSegmentPattern(digit);

    // Zhasínání úvodních nul (např. místo "0015" zobrazíme "  15")
    if (temp == 0 && i > 0 && value != 0) {
      pattern = getSegmentPattern(-1); // Zhasne celou pozici
    }
    // Výjimka pro nulu samotnou - tu chceme zobrazit na poslední pozici
    if (value == 0 && i == 0) {
      pattern = getSegmentPattern(0);
    }

    // Odeslání 8 bitů do posuvného registru (MSBFIRST)
    shiftOut(PIN_DATA, PIN_CLK, MSBFIRST, pattern);

    temp = temp / 10;
  }

  // Dokončení přenosu - Latch HIGH (data se zkopírují na výstupy registru a displej se rozsvítí)
  digitalWrite(PIN_CS, HIGH);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- 74HC595 Sedmisegmentový Displej - Start ---");

  // Nastavení pinů pro komunikaci jako výstupní
  pinMode(PIN_DATA, OUTPUT);
  pinMode(PIN_CLK, OUTPUT);
  pinMode(PIN_CS, OUTPUT);

  // Uvedení linek do výchozího stavu
  digitalWrite(PIN_DATA, LOW);
  digitalWrite(PIN_CLK, LOW);
  digitalWrite(PIN_CS, HIGH);

  // Zobrazení počáteční nuly
  displayValue(0);
  Serial.println("Inicializace dokončena.");
}

void loop() {
  // Inkrementace počítadla
  counter++;

  Serial.print("Aktualizace displeje: ");
  Serial.println(counter);

  // Zápis hodnoty počítadla na displej
  displayValue(counter);

  // Prodleva 500 ms (půl sekundy) podle zadání
  delay(500);
}
