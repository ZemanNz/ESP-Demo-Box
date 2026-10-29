#include "tetris.h"
#include <Arduino.h>

// 7 tvarů tetromin [typ 0..6][rotace 0..3][y][x]
// 0: I, 1: J, 2: L, 3: O, 4: S, 5: T, 6: Z
static const int TVARY[7][4][4][4] = {
    // 0: I (Azurová)
    {
        { {0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0} },
        { {0,0,1,0}, {0,0,1,0}, {0,0,1,0}, {0,0,1,0} },
        { {0,0,0,0}, {0,0,0,0}, {1,1,1,1}, {0,0,0,0} },
        { {0,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,1,0,0} }
    },
    // 1: J (Modrá)
    {
        { {1,0,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} },
        { {0,1,1,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0} },
        { {0,0,0,0}, {1,1,1,0}, {0,0,1,0}, {0,0,0,0} },
        { {0,1,0,0}, {0,1,0,0}, {1,1,0,0}, {0,0,0,0} }
    },
    // 2: L (Oranžová)
    {
        { {0,0,1,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} },
        { {0,1,0,0}, {0,1,0,0}, {0,1,1,0}, {0,0,0,0} },
        { {0,0,0,0}, {1,1,1,0}, {1,0,0,0}, {0,0,0,0} },
        { {1,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0} }
    },
    // 3: O (Žlutá)
    {
        { {0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} },
        { {0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} },
        { {0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} },
        { {0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} }
    },
    // 4: S (Zelená)
    {
        { {0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0} },
        { {0,1,0,0}, {0,1,1,0}, {0,0,1,0}, {0,0,0,0} },
        { {0,0,0,0}, {0,1,1,0}, {1,1,0,0}, {0,0,0,0} },
        { {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0} }
    },
    // 5: T (Fialová)
    {
        { {0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} },
        { {0,1,0,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0} },
        { {0,0,0,0}, {1,1,1,0}, {0,1,0,0}, {0,0,0,0} },
        { {0,1,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0} }
    },
    // 6: Z (Červená)
    {
        { {1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} },
        { {0,0,1,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0} },
        { {0,0,0,0}, {1,1,0,0}, {0,1,1,0}, {0,0,0,0} },
        { {0,1,0,0}, {1,1,0,0}, {1,0,0,0}, {0,0,0,0} }
    }
};

// 16-bit RGB565 barvy pro ST7789 displej
static const uint16_t BARVY_TFT[8] = {
    0x18E3, // 0: Pozadí hrací desky (velmi tmavě šedá)
    0x07FF, // 1: I - Azurová (Cyan)
    0x001F, // 2: J - Modrá (Blue)
    0xFD20, // 3: L - Oranžová (Orange)
    0xFFE0, // 4: O - Žlutá (Yellow)
    0x07E0, // 5: S - Zelená (Green)
    0x780F, // 6: T - Fialová (Purple)
    0xF800  // 7: Z - Červená (Red)
};

TetrisGame::TetrisGame() {
    reset();
}

void TetrisGame::reset() {
    for (int y = 0; y < VYSKA_PLOCHY; y++) {
        for (int x = 0; x < SIRKA_PLOCHY; x++) {
            plocha[y][x] = 0;
        }
    }
    konec_hry = false;
    skore = 0;
    pristi_typ = random(0, 7);
    novy_dilek();
}

void TetrisGame::novy_dilek() {
    aktualni_typ = pristi_typ;
    pristi_typ = random(0, 7);
    rotace = 0;
    new_tvar.x = 3;
    new_tvar.y = 0;
    if (kolize(new_tvar, rotace)) {
        konec_hry = true;
    }
}

bool TetrisGame::kolize(Point novy, int nova_rotace) {
    for (int px = 0; px < 4; px++) {
        for (int py = 0; py < 4; py++) {
            if (TVARY[aktualni_typ][nova_rotace][py][px] != 0) {
                Point test = {novy.x + px, novy.y + py};

                // Zdi a podlaha
                if (test.x < 0 || test.x >= SIRKA_PLOCHY || test.y >= VYSKA_PLOCHY) {
                    return true;
                }

                // Předešlé spadlé kostky
                if (test.y >= 0 && plocha[test.y][test.x] > 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

void TetrisGame::moveLeft() {
    if (konec_hry) return;
    Point a = {new_tvar.x - 1, new_tvar.y};
    if (!kolize(a, rotace)) {
        new_tvar.x--;
    }
}

void TetrisGame::moveRight() {
    if (konec_hry) return;
    Point a = {new_tvar.x + 1, new_tvar.y};
    if (!kolize(a, rotace)) {
        new_tvar.x++;
    }
}

void TetrisGame::rotate() {
    if (konec_hry) return;
    int next_rot = (rotace + 1) % 4;
    if (!kolize(new_tvar, next_rot)) {
        rotace = next_rot;
    }
}

void TetrisGame::boost() {
    if (konec_hry) return;

    int kolik = 999;
    for (int i = new_tvar.y; i < VYSKA_PLOCHY; i++) {
        if (kolize({new_tvar.x, i}, rotace)) {
            kolik = (i - new_tvar.y) - 1;
            break;
        }
    }

    new_tvar.y += kolik;
    update();
}

void TetrisGame::update() {
    if (konec_hry) return;

    Point a = {new_tvar.x, new_tvar.y + 1};

    if (kolize(a, rotace)) {
        uzamknout_dilek();
        smazat_plne_rady();
        novy_dilek();
    } else {
        new_tvar.y++;
    }
}

void TetrisGame::uzamknout_dilek() {
    for (int px = 0; px < 4; px++) {
        for (int py = 0; py < 4; py++) {
            if (TVARY[aktualni_typ][rotace][py][px] != 0) {
                int rx = new_tvar.x + px;
                int ry = new_tvar.y + py;
                if (ry >= 0 && ry < VYSKA_PLOCHY && rx >= 0 && rx < SIRKA_PLOCHY) {
                    plocha[ry][rx] = aktualni_typ + 1;
                }
            }
        }
    }
}

void TetrisGame::smazat_plne_rady() {
    int pocet_rad = 0;

    for (int i = 0; i < VYSKA_PLOCHY; i++) {
        bool vyplnena = true;
        for (int j = 0; j < SIRKA_PLOCHY; j++) {
            if (plocha[i][j] == 0) {
                vyplnena = false;
                break;
            }
        }

        if (vyplnena) {
            pocet_rad++;

            for (int y = i; y > 0; y--) {
                for (int x = 0; x < SIRKA_PLOCHY; x++) {
                    plocha[y][x] = plocha[y - 1][x];
                }
            }

            // Smazání horního řádku
            for (int x = 0; x < SIRKA_PLOCHY; x++) {
                plocha[0][x] = 0;
            }
        }
    }

    // Výpočet skóre
    if (pocet_rad == 1)      skore += 100;
    else if (pocet_rad == 2) skore += 300;
    else if (pocet_rad == 3) skore += 500;
    else if (pocet_rad >= 4) skore += 800;
}

int TetrisGame::ohodnot_plochu(int testovaci_plocha[VYSKA_PLOCHY][SIRKA_PLOCHY]) {
    int vysky[SIRKA_PLOCHY] = {0};
    int max_vyska = 0;
    int celkova_vyska = 0;

    for (int x = 0; x < SIRKA_PLOCHY; x++) {
        for (int y = 0; y < VYSKA_PLOCHY; y++) {
            if (testovaci_plocha[y][x] != 0) {
                vysky[x] = VYSKA_PLOCHY - y;
                break;
            }
        }
        celkova_vyska += vysky[x];
        if (vysky[x] > max_vyska) max_vyska = vysky[x];
    }

    // Zubatost
    int zubatost = 0;
    for (int x = 0; x < SIRKA_PLOCHY - 1; x++) {
        zubatost += abs(vysky[x] - vysky[x + 1]);
    }

    // Díry pod kostkami
    int diry = 0;
    for (int x = 0; x < SIRKA_PLOCHY; x++) {
        bool nasel_blok = false;
        for (int y = 0; y < VYSKA_PLOCHY; y++) {
            if (testovaci_plocha[y][x] != 0) {
                nasel_blok = true;
            } else if (nasel_blok && testovaci_plocha[y][x] == 0) {
                diry++;
            }
        }
    }

    // Plné řady
    int plne_rady = 0;
    for (int y = 0; y < VYSKA_PLOCHY; y++) {
        bool plna = true;
        for (int x = 0; x < SIRKA_PLOCHY; x++) {
            if (testovaci_plocha[y][x] == 0) {
                plna = false;
                break;
            }
        }
        if (plna) plne_rady++;
    }

    int hodnoceni = (plne_rady * 350)
                  - (diry * 120)
                  - (celkova_vyska * 5)
                  - (max_vyska * max_vyska / 3)
                  - (zubatost * 8);

    return hodnoceni;
}

int TetrisGame::najdi_dopad(int test_x, int test_rot) {
    int kolik = 0;
    while (!kolize({test_x, kolik}, test_rot)) {
        kolik++;
    }
    return kolik - 1;
}

void TetrisGame::autoStep() {
    if (konec_hry) return;

    int nejlepsi_skore = -999999;
    int nejlepsi_x = new_tvar.x;
    int nejlepsi_rot = rotace;

    for (int r = 0; r < 4; r++) {
        for (int x = -2; x < SIRKA_PLOCHY; x++) {
            if (kolize({x, 0}, r)) {
                continue;
            }

            int y = najdi_dopad(x, r);

            int kopie[VYSKA_PLOCHY][SIRKA_PLOCHY];
            for (int i = 0; i < VYSKA_PLOCHY; i++) {
                for (int j = 0; j < SIRKA_PLOCHY; j++) {
                    kopie[i][j] = plocha[i][j];
                }
            }

            for (int px = 0; px < 4; px++) {
                for (int py = 0; py < 4; py++) {
                    if (TVARY[aktualni_typ][r][py][px] != 0) {
                        int rx = x + px;
                        int ry = y + py;
                        if (ry >= 0 && ry < VYSKA_PLOCHY && rx >= 0 && rx < SIRKA_PLOCHY) {
                            kopie[ry][rx] = aktualni_typ + 1;
                        }
                    }
                }
            }

            int bodovani = ohodnot_plochu(kopie);

            if (bodovani > nejlepsi_skore) {
                nejlepsi_skore = bodovani;
                nejlepsi_rot = r;
                nejlepsi_x = x;
            }
        }
    }

    rotace = nejlepsi_rot;
    new_tvar.x = nejlepsi_x;
    boost();
}

void TetrisGame::draw() {
    GFXcanvas16* canvas = gfx.getCanvas();
    if (!canvas) return;

    // 1. Černé pozadí celého displeje 320x240
    canvas->fillScreen(ST77XX_BLACK);

    // 2. Kreslení hrací plochy (10x20 bloků po 12px = 120x240 px)
    for (int y = 0; y < VYSKA_PLOCHY; y++) {
        for (int x = 0; x < SIRKA_PLOCHY; x++) {
            int blockX = OFFSET_X + x * VELIKOST_BLOKU;
            int blockY = OFFSET_Y + y * VELIKOST_BLOKU;
            uint16_t barva = BARVY_TFT[plocha[y][x]];

            canvas->fillRect(blockX, blockY, VELIKOST_BLOKU - 1, VELIKOST_BLOKU - 1, barva);
        }
    }

    // 3. Kreslení padajícího dílku
    if (!konec_hry) {
        uint16_t barva = BARVY_TFT[aktualni_typ + 1];
        for (int py = 0; py < 4; py++) {
            for (int px = 0; px < 4; px++) {
                if (TVARY[aktualni_typ][rotace][py][px] != 0) {
                    int blockX = OFFSET_X + (new_tvar.x + px) * VELIKOST_BLOKU;
                    int blockY = OFFSET_Y + (new_tvar.y + py) * VELIKOST_BLOKU;
                    if (blockY >= 0) {
                        canvas->fillRect(blockX, blockY, VELIKOST_BLOKU - 1, VELIKOST_BLOKU - 1, barva);
                    }
                }
            }
        }
    }

    // 4. Ohraničení hrací desky (bílá čára vlevo i vpravo)
    canvas->drawFastVLine(OFFSET_X - 2, 0, 240, ST77XX_WHITE);
    canvas->drawFastVLine(OFFSET_X + SIRKA_PLOCHY * VELIKOST_BLOKU + 1, 0, 240, ST77XX_WHITE);

    // 5. Boční UI panel
    int panelX = OFFSET_X + SIRKA_PLOCHY * VELIKOST_BLOKU + 12;

    canvas->setTextColor(ST77XX_CYAN);
    canvas->setTextSize(2);
    canvas->setCursor(panelX, 10);
    canvas->print("TETRIS");

    canvas->setTextColor(ST77XX_WHITE);
    canvas->setTextSize(1);
    canvas->setCursor(panelX, 36);
    canvas->print("SKORE:");

    canvas->setTextColor(ST77XX_YELLOW);
    canvas->setTextSize(2);
    canvas->setCursor(panelX, 48);
    canvas->print(skore);

    // Náhled dalšího dílku (NEXT)
    int nextBoxX = 238; // Zcela vpravo
    int nextBoxY = 15;
    canvas->drawRoundRect(nextBoxX - 4, nextBoxY - 4, 68, 62, 4, 0x5AEB);
    canvas->setTextColor(0xDEFB);
    canvas->setTextSize(1);
    canvas->setCursor(nextBoxX + 16, nextBoxY);
    canvas->print("NEXT:");

    uint16_t nextCol = BARVY_TFT[pristi_typ + 1];
    int miniSize = 10;
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (TVARY[pristi_typ][0][py][px] != 0) {
                int bx = nextBoxX + 10 + px * miniSize;
                int by = nextBoxY + 14 + py * miniSize;
                canvas->fillRect(bx, by, miniSize - 1, miniSize - 1, nextCol);
            }
        }
    }

    // Ovládací nápověda
    canvas->setTextColor(ST77XX_GREEN);
    canvas->setTextSize(1);
    canvas->setCursor(panelX, 115);
    canvas->print("DPAD: Pohyb");
    canvas->setCursor(panelX, 130);
    canvas->print("NAHORU: Otocit");
    canvas->setCursor(panelX, 145);
    canvas->print("DOLU: Drop");
    canvas->setCursor(panelX, 160);
    canvas->print("SW1: Auto bot");
    canvas->setCursor(panelX, 175);
    canvas->print("CENTER: Hard drop");
    canvas->setCursor(panelX, 190);
    canvas->print("JOY BTN: Reset");
    canvas->setTextColor(0xFD20);
    canvas->setCursor(panelX, 210);
    canvas->print("CTR+JOY: Reset TOP");

    // 6. Game Over hláška
    if (konec_hry) {
        canvas->fillRect(OFFSET_X + 5, 95, 110, 45, ST77XX_RED);
        canvas->drawRect(OFFSET_X + 5, 95, 110, 45, ST77XX_WHITE);
        canvas->setTextColor(ST77XX_WHITE);
        canvas->setTextSize(2);
        canvas->setCursor(OFFSET_X + 36, 102);
        canvas->print("GAME");
        canvas->setCursor(OFFSET_X + 36, 120);
        canvas->print("OVER");
    }

    // Odeslání snímku z RAM na displej
    gfx.pushCanvasToScreen();
}
