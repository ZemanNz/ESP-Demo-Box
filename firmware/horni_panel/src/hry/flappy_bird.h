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
    void autoStep(); // AI Autopilot

    bool getGameOver() const { return isGameOver; }
    int getScore() const { return score; }
    void setBestScore(int b) { bestScore = b; }

private:
    void spawn_next_Obstacle();

    float birdY;
    float birdVelocity;
    float gravity;
    float lift;
    bool isGameOver;
    int score;
    int bestScore;

    std::vector<Prekazka> prekazky;
};
