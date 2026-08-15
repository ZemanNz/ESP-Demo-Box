#include "flappy_bird.h"
#include <Arduino.h>
#include <algorithm>

FlappyGame::FlappyGame() {
    reset();
}

void FlappyGame::reset() {
    birdY = 100.0f;
    birdVelocity = 0.0f;
    gravity = 0.4f;
    lift = -5.5f;
    isGameOver = false;
    prekazky.clear();
    spawn_next_Obstacle();
}

void FlappyGame::jump() {
    if (isGameOver) return;
    birdVelocity = lift;
}

void FlappyGame::spawn_next_Obstacle() {
    Prekazka p;
    p.x = 320.0f;
    p.obstacleGapY = 30 + (random(0, 100)); 
    p.gapY = 65 + (random(0, 25));          
    prekazky.push_back(p);
}

void FlappyGame::update() {
    if (isGameOver) return;

    birdVelocity += gravity;
    birdY += birdVelocity;

    // Kontrola kolize s podlahou/stropem displeje (320x240)
    if (birdY < 0 || birdY + 16.0f > 240.0f) {
        isGameOver = true;
    }

    // Pohyb překážek
    for (auto& p : prekazky) {
        p.x -= 2.5f;
    }

    // Generování další překážky
    if (!prekazky.empty() && prekazky.back().x < 320.0f - (110 + random(0, 50))) {
        spawn_next_Obstacle();
    }

    // Mazání překážek mimo obrazovku
    prekazky.erase(
        std::remove_if(prekazky.begin(), prekazky.end(), [](const Prekazka& p) {
            return p.x < -40.0f;
        }),
        prekazky.end()
    );

    // Detekce kolizí s trubkami
    float birdX = 50.0f;
    float birdW = 20.0f;
    float birdH = 16.0f;
    float pipeW = 30.0f;

    for (const auto& p : prekazky) {
        if (birdX + birdW > p.x && birdX < p.x + pipeW) {
            float topPipeBottom = p.obstacleGapY;
            float bottomPipeTop = p.obstacleGapY + p.gapY;

            if (birdY < topPipeBottom || birdY + birdH > bottomPipeTop) {
                isGameOver = true;
            }
        }
    }
}

void FlappyGame::draw() {
    // Kreslení v RAM bufferu
    GameEngine::clearScreen(GAME_COLOR_CYAN);

    // Kreslení ptáčka (Žluté tělo, černé oko)
    float birdX = 50.0f;
    GameEngine::fillRect(birdX, birdY, 20, 16, GAME_COLOR_YELLOW);
    GameEngine::fillRect(birdX + 14, birdY + 3, 4, 4, GAME_COLOR_BLACK);

    // Kreslení překážek (Zelená)
    float pipeW = 30.0f;
    for (const auto& p : prekazky) {
        // Horní trubka
        GameEngine::fillRect(p.x, 0, pipeW, p.obstacleGapY, GAME_COLOR_GREEN);

        // Spodní trubka
        float bottomPipeY = p.obstacleGapY + p.gapY;
        GameEngine::fillRect(p.x, bottomPipeY, pipeW, 240.0f - bottomPipeY, GAME_COLOR_GREEN);
    }

    if (isGameOver) {
        GameEngine::drawText(50, 100, "GAME OVER", GAME_COLOR_RED, 3);
    }

    // Přenese hotový snímek z RAM na displej bez probliknutí!
    GameEngine::displayFrame();
}
