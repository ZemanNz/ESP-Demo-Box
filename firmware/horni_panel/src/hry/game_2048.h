#pragma once

#include "../include/GraphicsManager.h"

enum Game2048State {
    G2048_PLAYING,
    G2048_GAME_OVER,
    G2048_VICTORY
};

class Game2048 {
public:
    Game2048();

    void reset();
    void draw();

    // Tyhle funkce si naprogramuješ sám!
    // Musí posunout čísla, sečíst stejná a pokud se něco pohlo, zavolat spawnRandomTile()
    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();
    void autoStep(); // AI Autopilot
    bool checkGameOver(); // Zkontroluje, zda zbývá nějaký platný tah

    Game2048State getState() const { return currentState; }
    int getScore() const { return score; }
    void setBestScore(int b) { bestScore = b; }

    int getMaxTile() const {
        int maxVal = 2;
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                if (board[x][y] > maxVal) {
                    maxVal = board[x][y];
                }
            }
        }
        return maxVal;
    }
    
    // Uživatelova proměnná pro detekci pohybu
    bool pohyb;

private:
    void spawnRandomTile(); // Přidá 2 nebo 4 na náhodné prázdné políčko
    uint16_t getColorForNumber(int num); // Vrací barvu podle čísla

    // --- Pomocné funkce pro AI autopilota ---
    // 0 = NAHORU, 1 = DOLŮ, 2 = DOLEVA, 3 = DOPRAVA
    bool simulateMove(const int src[4][4], int dest[4][4], int dir);
    int evaluateBoard(const int b[4][4]);

    int board[4][4];
    bool prazdne[4][4];
    byte pocetPrazdnych;
    Game2048State currentState;
    int score;
    int bestScore;
};
