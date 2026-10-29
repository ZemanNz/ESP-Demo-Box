#include "flappy_bird.h"
#include <Arduino.h>
#include <algorithm>

FlappyGame::FlappyGame() {
    bestScore = 0;
    reset();
}

void FlappyGame::reset() {
    birdY = 100.0f;
    birdVelocity = 0.0f;
    gravity = 0.4f;
    lift = -5.5f;
    isGameOver = false;
    score = 0;
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
    p.passed = false;
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

    for (auto& p : prekazky) {
        if (birdX + birdW > p.x && birdX < p.x + pipeW) {
            float topPipeBottom = p.obstacleGapY;
            float bottomPipeTop = p.obstacleGapY + p.gapY;

            if (birdY < topPipeBottom || birdY + birdH > bottomPipeTop) {
                isGameOver = true;
            }
        }
        
        // Pokud pták překonal překážku (jeho X je větší než pravý okraj trubky) a ještě jsme to nezapočítali
        if (!p.passed && birdX > p.x + pipeW) {
            p.passed = true;
            score++;
        }
    }
}

void FlappyGame::autoStep() {
    if (isGameOver) {
        return; // Autopilot se po skončení hry sám neresetuje
    }

    float birdX = 50.0f;
    float pipeW = 30.0f;

    // Hledáme nejbližší překážku před námi nebo tu, kterou právě prolétáme
    const Prekazka* targetPipe = nullptr;
    for (const auto& p : prekazky) {
        if (p.x + pipeW > birdX - 5.0f) {
            targetPipe = &p;
            break;
        }
    }

    if (targetPipe) {
        // Skáčeme až v dolní třetině mezery (cca 68% výšky mezery od jejího vrchu),
        // aby velký zdvih při skoku (-5.5 px) nenarazil do horní stěny trubky
        float targetJumpY = targetPipe->obstacleGapY + (targetPipe->gapY * 0.68f);

        // Skáčeme pouze když ptáček padá a propadl se do dolní třetiny mezery
        if (birdY >= targetJumpY && birdVelocity > 0.0f) {
            jump();
        }
    } else {
        // Žádná překážka v dohledu - udržujeme se bezpečně ve spodnější polovině obrazovky
        if (birdY > 150.0f && birdVelocity > 0.0f) {
            jump();
        }
    }

    // Záchranný skok těsně nad podlahou obrazovky
    if (birdY + 16.0f > 225.0f && birdVelocity > 0.0f) {
        jump();
    }
}

void FlappyGame::draw() {
    GFXcanvas16* canvas = gfx.getCanvas();
    if (!canvas) return; // Ochrana

    // 1. Vyčistíme pozadí (Cyan = modré nebe)
    canvas->fillScreen(ST77XX_CYAN);

    // 2. Kreslení překážek (Zelené trubky kreslíme nejprve, aby nepřekryly skóre)
    float pipeW = 30.0f;
    for (const auto& p : prekazky) {
        // Horní trubka
        canvas->fillRect(p.x, 0, pipeW, p.obstacleGapY, ST77XX_GREEN);

        // Spodní trubka
        float bottomPipeY = p.obstacleGapY + p.gapY;
        canvas->fillRect(p.x, bottomPipeY, pipeW, 240.0f - bottomPipeY, ST77XX_GREEN);
    }

    // 3. Kreslení ptáčka (Žluté tělo, černé oko)
    float birdX = 50.0f;
    canvas->fillRect(birdX, birdY, 20, 16, ST77XX_YELLOW);
    canvas->fillRect(birdX + 14, birdY + 3, 4, 4, ST77XX_BLACK);

    // 4. Kreslení skóre AŽ NAKONEC přes trubky (vždy plně viditelné i při průletu trubky)
    canvas->setTextColor(ST77XX_BLACK);
    canvas->setTextSize(3);
    canvas->setCursor(10, 8);
    canvas->print(score);

    if (isGameOver) {
        canvas->fillRect(55, 95, 210, 50, ST77XX_RED);
        canvas->drawRect(55, 95, 210, 50, ST77XX_WHITE);
        canvas->setTextColor(ST77XX_WHITE);
        canvas->setTextSize(3);
        canvas->setCursor(79, 108);
        canvas->print("GAME OVER");
    }

    // Přenese hotový snímek z RAM na displej bez probliknutí
    gfx.pushCanvasToScreen();
}
