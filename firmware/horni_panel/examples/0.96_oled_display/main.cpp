// OLED displej 0.96" přes I2C s řadičem SSD1306 a Rotačním Enkodérem (např. KY-040)
// Integrovaný příklad s počítadlem, interaktivní grafikou a ošetřeným přerušením (debounce)
// Určeno pro ESP32-S3 (deska horního panelu)

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Definice rozlišení OLED displeje
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Reset pin displeje (pokud ho displej nemá, nastavíme na -1)
#define OLED_RESET -1
// I2C adresa displeje
#define SCREEN_ADDRESS 0x3C

// --- I2C PINY PRO ESP32-S3 ---
#define I2C_SDA 8
#define I2C_SCL 9

// --- PINY PRO ROTAČNÍ ENKODÉR ---
#define ENCODER_CLK 18  // Hodinový signál (CLK / Output A)
#define ENCODER_DT  17  // Datový signál (DT / Output B)
#define ENCODER_SW  16  // Tlačítko enkodéru (SW / Switch)

// Inicializace objektu displeje
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- GLOBÁLNÍ PROMĚNNÉ (Sdílené s přerušením - volatile) ---
volatile int counter = 50;           // Hodnota počítadla (0 až 100)
volatile bool isHighlighted = false; // Příznak stisknutí tlačítka (zvýraznění)

// Sledování času pro hardwarový debounce enkodéru v mikrosekundách
volatile unsigned long lastClkInterruptTime = 0;

// Pomocná proměnná pro animaci záření při zvýraznění
float glowOffset = 0;

// --- OBSLUHA PŘERUŠENÍ (ISR) ---

// Obsluha přerušení pro otočení enkodéru
void IRAM_ATTR handleEncoder() {
  unsigned long currentTime = micros();
  
  // Softwarový debouncing v mikrosekundách (3000 us = 3 ms)
  // Zákmity (kontakt bounce) mechanických spínačů enkodéru obvykle odezní do 1-2 ms.
  // Tento filtr spolehlivě zabrání vícenásobným krokům nebo samovolnému skákání zpět.
  if (currentTime - lastClkInterruptTime > 3000) {
    int clkState = digitalRead(ENCODER_CLK);
    int dtState = digitalRead(ENCODER_DT);
    
    // Potvrdíme, že CLK je opravdu v log. 0 (klesající hrana)
    if (clkState == LOW) {
      if (dtState == HIGH) {
        // Otáčení po směru hodinových ručiček (CW) -> přičítáme
        // Upozornění: Pokud se vám hodnota přičítá při točení na druhou stranu,
        // stačí prohodit zapojení CLK a DT drátů, nebo v kódu prohodit definice pinů 17 a 18.
        if (counter < 100) {
          counter++;
        }
      } else {
        // Otáčení proti směru hodinových ručiček (CCW) -> odečítáme
        if (counter > 0) {
          counter--;
        }
      }
      lastClkInterruptTime = currentTime;
    }
  }
}

// Obsluha přerušení pro stisk tlačítka (debouncing 250 ms)
void IRAM_ATTR handleButton() {
  static unsigned long lastDebounceTime = 0;
  unsigned long currentTime = millis();
  
  if (currentTime - lastDebounceTime > 250) {
    isHighlighted = !isHighlighted;
    lastDebounceTime = currentTime;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Interactive Encoder & OLED Demo start!");

  // Nastavení pinů pro rotační enkodér s interními PULL-UPy
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);

  // Připojení přerušení (detekce klesající hrany FALLING)
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), handleEncoder, FALLING);
  attachInterrupt(digitalPinToInterrupt(ENCODER_SW), handleButton, FALLING);

  // Inicializace I2C
  if (!Wire.begin(I2C_SDA, I2C_SCL)) {
    Serial.println("Chyba: Inicializace I2C selhala!");
    while (true);
  }

  // Inicializace OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("Chyba: Alokace SSD1306 selhala!");
    while (true);
  }

  display.clearDisplay();
  display.display();
}

void loop() {
  // Vyčistit obrazovku
  display.clearDisplay();

  // 1. ZOBRAZENÍ ZÁHLAVÍ
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(18, 2);
  display.print("ROTACNI ENKODER");
  
  // Dělící linka pod záhlavím
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  // 2. VÝPOČET POZICE PRO VYCENTROVÁNÍ ČÍSLA
  int numWidth = 18;
  int currentVal = counter; // Bezpečné lokální zkopírování hodnoty z volatile proměnné
  
  if (currentVal >= 100) {
    numWidth = 54;
  } else if (currentVal >= 10) {
    numWidth = 36;
  }
  
  int xPos = (128 - numWidth) / 2;
  int yPos = 20;

  // 3. VYKRESLENÍ ČÍSLA A EFEKTU ROZZÁŘENÍ (Tlačítko stisknuto)
  if (isHighlighted) {
    // Vykreslení animovaného "záření" (pulzující rámečky okolo)
    glowOffset += 0.2;
    if (glowOffset >= 6.0) glowOffset = 0;
    
    for (int i = 0; i < 3; i++) {
      int dist = (int)(glowOffset) + (i * 4);
      display.drawRoundRect(xPos - 6 - dist, yPos - 4 - dist, numWidth + 12 + (dist * 2), 24 + 8 + (dist * 2), 4, SSD1306_WHITE);
    }
    
    // Invertovaný styl (bílé pozadí, černé písmo)
    display.fillRoundRect(xPos - 4, yPos - 2, numWidth + 8, 28, 3, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    // Standardní styl
    display.setTextColor(SSD1306_WHITE);
  }

  // Vytištění aktuální hodnoty počítadla
  display.setTextSize(3);
  display.setCursor(xPos, yPos);
  display.print(currentVal);

  // 4. VYKRESLENÍ GRAFICKÉHO UKAZATELE (Progress bar pod číslem)
  display.drawRect(14, 54, 100, 8, SSD1306_WHITE);
  display.fillRect(14, 54, currentVal, 8, SSD1306_WHITE);

  // 5. ODESLÁNÍ BUFFERU DO DISPLEJE
  display.display();

  // Krátká prodleva pro plynulost animace záření
  delay(30);
}
