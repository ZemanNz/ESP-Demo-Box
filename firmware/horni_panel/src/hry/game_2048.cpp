#include "game_2048.h"

Game2048::Game2048() {
    reset();
}

void Game2048::reset() {
    // Vynulujeme pole
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            board[x][y] = 0;
        }
    }
    score = 0;
    currentState = G2048_PLAYING;

    // Na začátku hry se objeví dvě startovní čísla
    spawnRandomTile();
    spawnRandomTile();
}

// ------------------------------------------------------------------
// TVŮJ DOMÁCÍ ÚKOL ZAČÍNÁ TADY
// ------------------------------------------------------------------

// Tuto funkci si doplň sám! Měla by najít prázdné políčko (kde je 0)
// a dát tam s 90% šancí číslo 2 a s 10% šancí číslo 4.
void Game2048::spawnRandomTile() {
    // TIP: Najdi všechny prázdné buňky, vyber jednu náhodnou 
    // a vlož do ní 2 nebo 4.
}

// Tahové funkce (Tvoje hlavní výzva!)
// Zkus implementovat posun všech čísel doleva a sečtení těch stejných.
void Game2048::moveLeft() {
    
}
void Game2048::moveRight() {
    
}
void Game2048::moveUp() {
    
}
void Game2048::moveDown() {
    
}

// ------------------------------------------------------------------
// KONEC DOMÁCÍHO ÚKOLU (Grafiku máš hotovou dole)
// ------------------------------------------------------------------


// Pomocná funkce pro barvy (můžeš si barvy upravit podle sebe)
uint16_t Game2048::getColorForNumber(int num) {
    switch (num) {
        case 0: return ST77XX_DARKGREY; // Prázdné políčko
        case 2: return ST77XX_CYAN;
        case 4: return ST77XX_BLUE;
        case 8: return ST77XX_GREEN;
        case 16: return ST77XX_YELLOW;
        case 32: return ST77XX_ORANGE;
        case 64: return ST77XX_RED;
        case 128: return ST77XX_MAGENTA;
        // Můžeš přidat další barvy pro 256, 512, 1024, 2048
        default: return ST77XX_WHITE; 
    }
}

// Tahle funkce všechna data z tvého pole překreslí na displej
void Game2048::draw() {
    GFXcanvas16* canvas = gfx.getCanvas();
    if (!canvas) return;

    canvas->fillScreen(ST77XX_BLACK);

    // Výpočet pozic pro vykreslení krásně doprostřed displeje
    int tileSize = 50;
    int spacing = 6;
    int startX = (320 - (4 * tileSize + 3 * spacing)) / 2; 
    int startY = (240 - (4 * tileSize + 3 * spacing)) / 2; 

    // Projdeme pole 4x4
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int num = board[x][y];
            int px = startX + x * (tileSize + spacing);
            int py = startY + y * (tileSize + spacing);

            // Vykreslíme zaoblený čtverec správnou barvou
            canvas->fillRoundRect(px, py, tileSize, tileSize, 5, getColorForNumber(num));

            // Vykreslíme číslo dovnitř (pokud není 0)
            if (num > 0) {
                canvas->setTextColor(ST77XX_BLACK); // Černý text na barevném pozadí
                
                // Zmenšení textu pro velká čísla (např. 1024 a víc)
                if (num > 99) canvas->setTextSize(2);
                else canvas->setTextSize(3);

                // Zjednodušené centrování textu do čtverečku
                int textX = px + (num > 99 ? 5 : 8); 
                int textY = py + 15;
                
                canvas->setCursor(textX, textY);
                canvas->print(num);
            }
        }
    }

    if (currentState == G2048_GAME_OVER) {
        canvas->setTextColor(ST77XX_RED);
        canvas->setTextSize(4);
        canvas->setCursor(45, 100);
        canvas->print("KONEC!");
    }

    gfx.pushCanvasToScreen();
}
