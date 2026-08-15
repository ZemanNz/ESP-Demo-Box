#include "game_engine.h"

Adafruit_ST7789* GameEngine::display = nullptr;
GFXcanvas16* GameEngine::canvas = nullptr;

void GameEngine::setDisplay(Adafruit_ST7789* displayPtr) {
    display = displayPtr;
    if (!canvas) {
        // Vytvořit 320x240 frame buffer v RAM paměti ESP32 (153 KB RAM)
        canvas = new GFXcanvas16(320, 240);
    }
}

bool GameEngine::isButtonPressed(GameButton btn) {
#ifdef PIN_BTN1
    bool b1 = (digitalRead(PIN_BTN1) == LOW);
#else
    bool b1 = false;
#endif

#ifdef PIN_BTN2
    bool b2 = (digitalRead(PIN_BTN2) == LOW);
#else
    bool b2 = false;
#endif

    switch (btn) {
        case BTN_ACTION:
        case BTN_UP:
        case BTN_RIGHT:
            return b1;

        case BTN_RESET:
        case BTN_DOWN:
        case BTN_LEFT:
            return b2;

        default:
            return false;
    }
}

void GameEngine::clearScreen(uint16_t color) {
    if (canvas) {
        canvas->fillScreen(color);
    } else if (display) {
        display->fillScreen(color);
    }
}

void GameEngine::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (canvas) {
        canvas->drawRect(x, y, w, h, color);
    } else if (display) {
        display->drawRect(x, y, w, h, color);
    }
}

void GameEngine::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (canvas) {
        canvas->fillRect(x, y, w, h, color);
    } else if (display) {
        display->fillRect(x, y, w, h, color);
    }
}

void GameEngine::drawText(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size) {
    if (canvas) {
        canvas->setTextColor(color);
        canvas->setTextSize(size);
        canvas->setCursor(x, y);
        canvas->print(text);
    } else if (display) {
        display->setTextColor(color);
        display->setTextSize(size);
        display->setCursor(x, y);
        display->print(text);
    }
}

void GameEngine::displayFrame() {
    if (display && canvas) {
        // Pošle celý hotový snímek z RAM na fyzický displej najednou -> nula problikávání!
        display->drawRGBBitmap(0, 0, canvas->getBuffer(), 320, 240);
    }
}
