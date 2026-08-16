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

    Game2048State getState() const { return currentState; }
    
    // Uživatelova proměnná pro detekci pohybu
    bool pohyb;

private:
    void spawnRandomTile(); // Přidá 2 nebo 4 na náhodné prázdné políčko
    uint16_t getColorForNumber(int num); // Vrací barvu podle čísla

    int board[4][4];
    bool prazdne[4][4];
    byte pocetPrazdnych;
    Game2048State currentState;
    int score;
};
