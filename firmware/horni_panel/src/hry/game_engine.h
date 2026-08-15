#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// Definice barev ve formátu RGB565 pro Adafruit_GFX
#define GAME_COLOR_BLACK   ST77XX_BLACK
#define GAME_COLOR_WHITE   ST77XX_WHITE
#define GAME_COLOR_RED     ST77XX_RED
#define GAME_COLOR_GREEN   ST77XX_GREEN
#define GAME_COLOR_BLUE    ST77XX_BLUE
#define GAME_COLOR_YELLOW  ST77XX_YELLOW
#define GAME_COLOR_CYAN    ST77XX_CYAN
#define GAME_COLOR_MAGENTA ST77XX_MAGENTA

enum GameButton {
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_ACTION,
    BTN_RESET
};

class GameEngine {
public:
    // Předání odkazu na displej inicializovaný v hlavním setup() programu
    static void setDisplay(Adafruit_ST7789* displayPtr);

    // Vstupy (čtou přímo ze stavu tlačítkových pinů)
    static bool isButtonPressed(GameButton btn);

    // Kreslení do RAM paměti (bez problikávání)
    static void clearScreen(uint16_t color = GAME_COLOR_BLACK);
    static void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    static void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    static void drawText(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size = 2);

    // Překreslení RAM bufferu na fyzický displej (zavolá se na konci vykreslení snímku)
    static void displayFrame();

private:
    static Adafruit_ST7789* display;
    static GFXcanvas16* canvas;
};
