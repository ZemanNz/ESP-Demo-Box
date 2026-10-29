#pragma once

#include "../include/GraphicsManager.h"
#include "snake.h" // Pro sdílenou strukturu Point

class TetrisGame {
public:
    TetrisGame();

    void reset();
    void update();
    void draw();

    // Ovládání
    void moveLeft();
    void moveRight();
    void rotate();
    void boost();
    void autoStep();

    bool isGameOver() const { return konec_hry; }
    int getScore() const { return skore; }
    int getCurrentType() const { return aktualni_typ; }
    int getNextType() const { return pristi_typ; }

private:
    static const int SIRKA_PLOCHY = 10;   // 10 sloupců
    static const int VYSKA_PLOCHY = 20;   // 20 řádků
    static const int VELIKOST_BLOKU = 12; // 12 px (10*12 = 120 px, 20*12 = 240 px)
    static const int OFFSET_X = 20;       // Herní pole začíná na X = 20, zbytek vpravo je UI panel
    static const int OFFSET_Y = 0;        // Celých 240 px na výšku

    int plocha[VYSKA_PLOCHY][SIRKA_PLOCHY];
    Point new_tvar;
    int aktualni_typ;
    int pristi_typ;
    int rotace;
    int skore;
    bool konec_hry;

    void novy_dilek();
    bool kolize(Point novy, int nova_rotace);
    void uzamknout_dilek();
    void smazat_plne_rady();
    int ohodnot_plochu(int testovaci_plocha[VYSKA_PLOCHY][SIRKA_PLOCHY]);
    int najdi_dopad(int test_x, int test_rot);
};
