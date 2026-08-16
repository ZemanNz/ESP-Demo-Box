#pragma once

#include "../include/GraphicsManager.h"
#include <vector>

struct Point {
    int x, y;
};

class SnakeGame {
public:
    SnakeGame();

    void reset();
    void update();
    void draw();

    void goUp();
    void goDown();
    void goLeft();
    void goRight();

    bool isGameOver() const { return game_stopped; }

private:
    void nove_jidlo();

    std::vector<Point> snake;
    Point direction;
    Point jidlo;
    bool game_stopped;

    static const int BLOCK_SIZE = 10; // 10px na políčko (při 320x240 je to 32x24 políček)
    static const int GRID_W = 32;
    static const int GRID_H = 24;
};
