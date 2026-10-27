#include "snake.h"
#include <Arduino.h>

SnakeGame::SnakeGame() {
    reset();
}

void SnakeGame::reset() {
    game_stopped = false;
    score = 0;
    snake = {{GRID_W/2, 8}, {GRID_W/2-1, 8}, {GRID_W/2-2, 8}};
    direction = {1, 0}; // Doprava
    next_direction = direction;
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

    direction = next_direction; // Směr se uzamkne pro celý krok!

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

void SnakeGame::goUp()    { if (direction.y == 0) next_direction = {0, -1}; }
void SnakeGame::goDown()  { if (direction.y == 0) next_direction = {0, 1}; }
void SnakeGame::goLeft()  { if (direction.x == 0) next_direction = {-1, 0}; }
void SnakeGame::goRight() { if (direction.x == 0) next_direction = {1, 0}; }

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
    canvas->fillRect(OFFSET_X + jidlo.x * BLOCK_SIZE, OFFSET_Y + jidlo.y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, ST77XX_RED);

    // Kreslení hada (Zelená)
    for (const auto& p : snake) {
        canvas->fillRect(OFFSET_X + p.x * BLOCK_SIZE, OFFSET_Y + p.y * BLOCK_SIZE, BLOCK_SIZE - 1, BLOCK_SIZE - 1, ST77XX_GREEN);
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

void SnakeGame::autoStep(){
    Point head = snake[0];
    Point moves[4] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}}; // UP, DOWN, LEFT, RIGHT
    Point bestMove = direction;
    int bestDist = 9999;
    for (int i = 0; i < 4; i++) {
        Point m = moves[i];

        // Lidsky čitelné:
        if (direction.x == 1  && m.x == -1) continue; // Jdu vpravo, zakázat vlevo
        if (direction.x == -1 && m.x == 1)  continue; // Jdu vlevo, zakázat vpravo
        if (direction.y == 1  && m.y == -1) continue; // Jdu dolů, zakázat nahoru
        if (direction.y == -1 && m.y == 1)  continue; // Jdu nahoru, zakázat dolů

        Point testPos = {head.x + m.x, head.y + m.y};
        if (testPos.x >= GRID_W) testPos.x = 0;
        if (testPos.x < 0)       testPos.x = GRID_W - 1;
        if (testPos.y >= GRID_H) testPos.y = 0;
        if (testPos.y < 0)       testPos.y = GRID_H - 1;

        //kolize
        bool kolize = false;

        for(size_t j = 0; j < snake.size() -1 ; j++){ // -1 -> konec ocasu zmizi
            if(snake[j].x == testPos.x && snake[j].y == testPos.y){
                kolize = true;
                break;
            }
        }

        if(kolize){continue;}

        //vzdalenost

        int dist = abs(testPos.x - jidlo.x) + abs(testPos.y - jidlo.y);
        if (dist < bestDist) {
            bestDist = dist;
            bestMove = m;
        }
    }
    next_direction = bestMove;
}
