#include "snake.h"
#include <Arduino.h>

SnakeGame::SnakeGame() {
    reset();
}

void SnakeGame::reset() {
    game_stopped = false;
    score = 0;
    snake = {{16, 12}, {15, 12}, {14, 12}};
    direction = {1, 0}; // Doprava
    nove_jidlo();
}

void SnakeGame::nove_jidlo() {
    jidlo.x = random(0, GRID_W);
    jidlo.y = random(0, GRID_H);

    for (const auto& dil : snake) {
        if (dil.x == jidlo.x && dil.y == jidlo.y) {
            nove_jidlo();
            return;
        }
    }
}

void SnakeGame::update() {
    if (game_stopped) return;

    Point newHead = {snake[0].x + direction.x, snake[0].y + direction.y};

    // Kontrola hranic (wrap around jako v původním kódu)
    if (newHead.x >= GRID_W) newHead.x = 0;
    if (newHead.x < 0)       newHead.x = GRID_W - 1;
    if (newHead.y >= GRID_H) newHead.y = 0;
    if (newHead.y < 0)       newHead.y = GRID_H - 1;

    // Kolize se sebou
    for (size_t i = 1; i < snake.size(); ++i) {
        if (snake[i].x == newHead.x && snake[i].y == newHead.y) {
            game_stopped = true;
            return;
        }
    }

    // Snědení jídla
    if (newHead.x == jidlo.x && newHead.y == jidlo.y) {
        snake.push_back({0, 0});
        score++;
        nove_jidlo();
    }

    snake.insert(snake.begin(), newHead);
    snake.pop_back();
}

void SnakeGame::goUp()    { if (direction.y == 0) direction = {0, -1}; }
void SnakeGame::goDown()  { if (direction.y == 0) direction = {0, 1}; }
void SnakeGame::goLeft()  { if (direction.x == 0) direction = {-1, 0}; }
void SnakeGame::goRight() { if (direction.x == 0) direction = {1, 0}; }

void SnakeGame::draw() {
    GFXcanvas16* canvas = gfx.getCanvas();
    if (!canvas) return; // Ochrana

    // Vyčistíme paměť na černo
    canvas->fillScreen(ST77XX_BLACK);

    // Kreslení skóre
    canvas->setTextColor(ST77XX_WHITE);
    canvas->setTextSize(2);
    canvas->setCursor(5, 5);
    canvas->print("Skore: ");
    canvas->print(score);

    // Kreslení jídla (Červená)
    canvas->fillRect(jidlo.x * BLOCK_SIZE, jidlo.y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, ST77XX_RED);

    // Kreslení hada (Zelená)
    for (const auto& p : snake) {
        canvas->fillRect(p.x * BLOCK_SIZE, p.y * BLOCK_SIZE, BLOCK_SIZE - 1, BLOCK_SIZE - 1, ST77XX_GREEN);
    }

    if (game_stopped) {
        canvas->setTextColor(ST77XX_RED);
        canvas->setTextSize(3);
        canvas->setCursor(45, 110);
        canvas->print("GAME OVER");
    }

    // Přenese hotový snímek z RAM na displej bez probliknutí
    gfx.pushCanvasToScreen();
}
