#include <Arduino.h>
#include <Adafruit_GFX.h>    // Hlavní grafická knihovna Adafruit
#include <Adafruit_ST7789.h> // Knihovna přímo pro řadič ST7789 (2.8" IPS displej)
#include <SPI.h>

// ---------------------------------------------------------
// Piny displeje podle README (ESP32-S3)
// ---------------------------------------------------------
#define TFT_CS    10
#define TFT_DC    13
#define TFT_RST   14
#define TFT_MOSI  11
#define TFT_SCK   12
#define TFT_BL    21  // Podsvícení displeje

// Inicializace displeje. Používáme hardwarové SPI (rychlejší).
// Na ESP32-S3 se dají piny pro SPI přemapovat, což uděláme v setup()
Adafruit_ST7789 tft = Adafruit_ST7789(TCS_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST); // pro soft SPI nebo předáme rovnou existující SPI

// Lepší je použít globální SPI a přiřadit piny
SPIClass *vspi = NULL;
Adafruit_ST7789 tft_hw = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);

// ---------------------------------------------------------
// Definice stavového automatu (State Machine)
// ---------------------------------------------------------
enum AppState {
  STATE_MAIN_MENU,
  STATE_SENSORS_DASHBOARD,
  STATE_MOTOR_TEST,
  STATE_GAME_SNAKE
};

// Aktuální stav, do kterého zařízení nabootuje
AppState currentState = STATE_MAIN_MENU;

// Proměnné pro neblokující časování (bez delay!)
unsigned long previousMillis = 0;
const long interval = 5000; // Přepnutí každých 5 sekund (5000 ms)

// Proměnná pro detekci, že se stav právě změnil (abychom displej nevykreslovali pořád dokola)
bool stateChanged = true;

// ---------------------------------------------------------
// Pomocné funkce pro vykreslení jednotlivých stavů
// ---------------------------------------------------------

void drawMainMenu() {
  tft_hw.fillScreen(ST77XX_BLACK); // Smazat obrazovku (černá)
  
  // Hlavička
  tft_hw.fillRect(0, 0, 320, 40, ST77XX_BLUE);
  tft_hw.setTextColor(ST77XX_WHITE);
  tft_hw.setTextSize(3);
  tft_hw.setCursor(50, 10);
  tft_hw.print("HLAVNI MENU");

  // Vykreslení dlaždic
  tft_hw.drawRect(20, 60, 130, 80, ST77XX_WHITE);
  tft_hw.setCursor(35, 90);
  tft_hw.setTextSize(2);
  tft_hw.print("Senzory");

  tft_hw.drawRect(170, 60, 130, 80, ST77XX_WHITE);
  tft_hw.setCursor(195, 90);
  tft_hw.print("Motory");

  tft_hw.drawRect(20, 150, 130, 80, ST77XX_WHITE);
  tft_hw.setCursor(55, 180);
  tft_hw.print("Hra");

  tft_hw.drawRect(170, 150, 130, 80, ST77XX_WHITE);
  tft_hw.setCursor(185, 180);
  tft_hw.print("Klepeto");
}

void drawSensorsDashboard() {
  tft_hw.fillScreen(ST77XX_BLACK);
  
  tft_hw.fillRect(0, 0, 320, 40, ST77XX_ORANGE);
  tft_hw.setTextColor(ST77XX_BLACK);
  tft_hw.setTextSize(3);
  tft_hw.setCursor(30, 10);
  tft_hw.print("SENZORY (ZIVE)");

  tft_hw.setTextColor(ST77XX_GREEN);
  tft_hw.setTextSize(2);
  tft_hw.setCursor(20, 70);
  tft_hw.print("Teplota:    24.5 C");
  
  tft_hw.setTextColor(ST77XX_CYAN);
  tft_hw.setCursor(20, 110);
  tft_hw.print("Vlhkost:    45 %");

  tft_hw.setTextColor(ST77XX_RED);
  tft_hw.setCursor(20, 150);
  tft_hw.print("Laser:      152 mm");

  tft_hw.setTextColor(ST77XX_YELLOW);
  tft_hw.setCursor(20, 190);
  tft_hw.print("Barva:      Zluta");
}

void drawMotorTest() {
  tft_hw.fillScreen(ST77XX_BLACK);
  
  tft_hw.fillRect(0, 0, 320, 40, ST77XX_RED);
  tft_hw.setTextColor(ST77XX_WHITE);
  tft_hw.setTextSize(3);
  tft_hw.setCursor(30, 10);
  tft_hw.print("TEST MOTORU");

  tft_hw.setTextSize(2);
  tft_hw.setCursor(20, 80);
  tft_hw.print("Servo 1:    90°");
  tft_hw.drawRect(20, 110, 280, 20, ST77XX_WHITE);
  tft_hw.fillRect(20, 110, 140, 20, ST77XX_GREEN); // Grafický ukazatel na 50%

  tft_hw.setCursor(20, 160);
  tft_hw.print("Motor 1:    75 % PWM");
  tft_hw.drawRect(20, 190, 280, 20, ST77XX_WHITE);
  tft_hw.fillRect(20, 190, 210, 20, ST77XX_MAGENTA); // Grafický ukazatel na 75%
}

void drawGameSnake() {
  tft_hw.fillScreen(ST77XX_BLACK);
  
  tft_hw.fillRect(0, 0, 320, 30, ST77XX_GREEN);
  tft_hw.setTextColor(ST77XX_BLACK);
  tft_hw.setTextSize(2);
  tft_hw.setCursor(10, 8);
  tft_hw.print("HRA: HAD | Skore: 15");

  // Rámeček hrací plochy
  tft_hw.drawRect(5, 35, 310, 200, ST77XX_WHITE);

  // Zjednodušené vykreslení Hada
  tft_hw.fillRect(100, 100, 15, 15, ST77XX_GREEN); // Hlava
  tft_hw.fillRect(85, 100, 15, 15, ST77XX_GREEN);  // Tělo
  tft_hw.fillRect(70, 100, 15, 15, ST77XX_GREEN);  // Tělo
  tft_hw.fillRect(70, 85, 15, 15, ST77XX_GREEN);   // Ocas

  // Jablko
  tft_hw.fillRect(200, 150, 15, 15, ST77XX_RED);
}

// ---------------------------------------------------------
// Hlavní program
// ---------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println("Startuji State Machine Displeje...");

  // Zapnutí podsvícení
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Inicializace SPI s našimi piny na S3
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS); // MISO nepotřebujeme, nastavíme na -1

  // Inicializace displeje 
  // (240x320 rozlišení, mód závisí na přesném typu displeje)
  tft_hw.init(240, 320);
  
  // Otočení obrazu na šířku (landscape)
  tft_hw.setRotation(1); 
  
  tft_hw.fillScreen(ST77XX_BLACK);
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. ČÁST: Změna stavu každých 5 sekund
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Přepnutí na další stav
    switch (currentState) {
      case STATE_MAIN_MENU:
        currentState = STATE_SENSORS_DASHBOARD;
        Serial.println("Prechod do stavu: SENZORY");
        break;
      case STATE_SENSORS_DASHBOARD:
        currentState = STATE_MOTOR_TEST;
        Serial.println("Prechod do stavu: TEST MOTORU");
        break;
      case STATE_MOTOR_TEST:
        currentState = STATE_GAME_SNAKE;
        Serial.println("Prechod do stavu: HRA HAD");
        break;
      case STATE_GAME_SNAKE:
        currentState = STATE_MAIN_MENU;
        Serial.println("Prechod do stavu: HLAVNI MENU");
        break;
    }
    
    // Nahlásíme, že se má displej překreslit
    stateChanged = true; 
  }

  // 2. ČÁST: Překreslení obrazovky POUZE KDYŽ SE STAV ZMĚNÍ
  // Tím zabráníme tomu, aby obrazovka neustále problikávala.
  if (stateChanged) {
    
    switch (currentState) {
      case STATE_MAIN_MENU:
        drawMainMenu();
        break;
      case STATE_SENSORS_DASHBOARD:
        drawSensorsDashboard();
        break;
      case STATE_MOTOR_TEST:
        drawMotorTest();
        break;
      case STATE_GAME_SNAKE:
        drawGameSnake();
        break;
    }
    
    // Máme nakresleno, příští smyčku loop() už nic kreslit nebudeme
    stateChanged = false;
  }
  
  // Zde může běžet další kód (čtení UARTU, teploměru...), 
  // protože smyčka loop() běží na plný výkon bez delay() záseků!
}
