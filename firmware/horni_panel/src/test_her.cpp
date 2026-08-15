#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "hry/game_engine.h"
#include "hry/snake.h"
#include "hry/flappy_bird.h"

// Definice pinů displeje (stejně jako v příkladu 2.8_tft_display)
#define TFT_CS   10
#define TFT_DC   13
#define TFT_RST  14
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_MISO -1

// Globální objekt displeje
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Instatnce her
SnakeGame snake;
FlappyGame flappy;

// Režim testu: 0 = Snake, 1 = Flappy Bird
int herniRezim = 0;
unsigned long casPosledniZmeny = 0;
unsigned long casPoslednihoKroku = 0;

void setup() {
    Serial.begin(115200);
    Serial.println("Testovani her: Snake a Flappy Bird...");

    // Inicializace SPI a displeje přesně podle příkladu
    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
    tft.init(240, 320);
    tft.invertDisplay(false);
    tft.setRotation(1); // Orientace 320x240 na šířku
    tft.fillScreen(ST77XX_BLACK);

    // Předání odkazu na inicializovaný displej do GameEnginu
    GameEngine::setDisplay(&tft);

    // Reset her
    snake.reset();
    flappy.reset();
    casPosledniZmeny = millis();
}

void loop() {
    unsigned long aktualniCas = millis();

    // Přepínání her každých 12 sekund, nebo při Game Over po 3 sekundách
    if (herniRezim == 0) {
        // --- TEST SNAKE ---
        if (aktualniCas - casPoslednihoKroku >= 150) {
            casPoslednihoKroku = aktualniCas;
            
            snake.update();
            snake.draw();
        }

        // Přepnutí na Flappy Bird po 12s nebo při Game Over
        if (aktualniCas - casPosledniZmeny > 12000 || (snake.isGameOver() && aktualniCas - casPosledniZmeny > 3000)) {
            herniRezim = 1;
            flappy.reset();
            casPosledniZmeny = aktualniCas;
        }
    } 
    else {
        // --- TEST FLAPPY BIRD ---
        if (aktualniCas - casPoslednihoKroku >= 40) {
            casPoslednihoKroku = aktualniCas;

            flappy.update();
            flappy.draw();
        }

        // Přepnutí zpět na Snake po 12s nebo při Game Over
        if (aktualniCas - casPosledniZmeny > 12000 || (flappy.getGameOver() && aktualniCas - casPosledniZmeny > 3000)) {
            herniRezim = 0;
            snake.reset();
            casPosledniZmeny = aktualniCas;
        }
    }
}
