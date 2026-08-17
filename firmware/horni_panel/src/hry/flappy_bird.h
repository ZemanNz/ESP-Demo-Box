#pragma once

#include "../include/GraphicsManager.h"
#include <vector>

struct Prekazka {
    float x;
    float obstacleGapY;
    float gapY;
    bool passed;
};

class FlappyGame {
public:
    FlappyGame();

    void jump();
    void update();
    void draw();
    void reset();

    bool getGameOver() const { return isGameOver; }

private:
    void spawn_next_Obstacle();

    float birdY;
    float birdVelocity;
    float gravity;
    float lift;
    bool isGameOver;
    int score;

    std::vector<Prekazka> prekazky;
};
