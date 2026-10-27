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
    int getScore() const { return score; }
    void autoStep();

private:
    void nove_jidlo();

    std::vector<Point> snake;
    Point direction;
    Point next_direction;
    Point jidlo;
    bool game_stopped;
    int score;

    static const int BLOCK_SIZE = 12; // 12px na políčko (26x20 políček, 520 celkem)
    static const int GRID_W = 26;
    static const int GRID_H = 20;
    static const int OFFSET_X = (320 - (GRID_W * BLOCK_SIZE)) / 2; // 4 px vycentrování
    static const int OFFSET_Y = (240 - (GRID_H * BLOCK_SIZE)) / 2; // 0 px
};
