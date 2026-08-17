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
    pocetPrazdnych = 0;
    for (byte y = 0; y < 4; y++) {
        for (byte x = 0; x < 4; x++) {
            if(board[x][y] == 0) {
                prazdne[x][y] = 1;
                pocetPrazdnych++;
            } else {
                prazdne[x][y] = 0;
            }
        }
    }
    if(pocetPrazdnych > 0) {
        byte randomIndex = random(0, pocetPrazdnych);
        byte hodnota = random(0, 10) < 8 ? 2 : 4; // 80% šance na 2, 10% na 4  

        for(byte y = 0; y < 4; y++) {
            for(byte x = 0; x < 4; x++) {
                if(prazdne[x][y]){
                    if(randomIndex == 0) {
                        board[x][y] = hodnota;
                        return;
                    }
                    randomIndex--;
                }
            }
        }
    }
    else{
        currentState = G2048_GAME_OVER;
    }

}

// Tahové funkce (Tvoje hlavní výzva!)
// Zkus implementovat posun všech čísel doleva a sečtení těch stejných.
void Game2048::moveLeft() {
    pohyb = false;
    for (int y = 0; y < 4; y++) {
        // Fáze 1: Gravitace doleva
        int volno_x = 0;
        for (int x = 0; x <= 3; x++) {
            if (board[x][y] != 0) {
                int ulozeneCislo = board[x][y];
                if (x != volno_x) {
                    board[volno_x][y] = ulozeneCislo;
                    board[x][y] = 0;
                    pohyb = true;
                }
                volno_x++;
            }
        }   
        
        // Fáze 2: Sčítání
        for (int x = 0; x < 3; x++) {
            if (board[x][y] != 0 && board[x][y] == board[x+1][y]) {
                board[x][y] = board[x][y] * 2;
                score += board[x][y];
                board[x+1][y] = 0;
                pohyb = true;
            }
        }
        
        // Fáze 3: Gravitace doleva po sčítání
        volno_x = 0;
        for (int x = 0; x <= 3; x++) {
            if (board[x][y] != 0) {
                int ulozeneCislo = board[x][y];
                if (x != volno_x) {
                    board[volno_x][y] = ulozeneCislo;
                    board[x][y] = 0;
                }
                volno_x++;
            }
        }
    }
    
    if (pohyb) {
        spawnRandomTile();
    }
}
void Game2048::moveRight() {
    pohyb = false;
    for (int y = 0; y < 4; y++) {
        // Fáze 1: Gravitace doprava
        int volno_x = 3;
        for (int x = 3; x >= 0; x--) {
            if (board[x][y] != 0) {
                int ulozeneCislo = board[x][y];
                if (x != volno_x) {
                    board[volno_x][y] = ulozeneCislo;
                    board[x][y] = 0;
                    pohyb = true;
                }
                volno_x--;
            }
        }   
        
        // Fáze 2: Sčítání
        for (int x = 3; x > 0; x--) {
            if (board[x][y] != 0 && board[x][y] == board[x-1][y]) {
                board[x][y] = board[x][y] * 2;
                score += board[x][y];
                board[x-1][y] = 0;
                pohyb = true;
            }
        }
        
        // Fáze 3: Gravitace doprava po sčítání
        volno_x = 3;
        for (int x = 3; x >= 0; x--) {
            if (board[x][y] != 0) {
                int ulozeneCislo = board[x][y];
                if (x != volno_x) {
                    board[volno_x][y] = ulozeneCislo;
                    board[x][y] = 0;
                }
                volno_x--;
            }
        }
    }
    
    if (pohyb) {
        spawnRandomTile();
    }
}
void Game2048::moveUp() {
    pohyb = false;
    for (int x = 0; x < 4; x++) {
        
        // Fáze 1: Gravitace
        int volno_y = 0;
        for (int y = 0; y <= 3; y++) {
            if (board[x][y] != 0) {
                int ulozeneCislo = board[x][y];
                if (y != volno_y) { // Pokud už není úplně dole
                    board[x][volno_y] = ulozeneCislo;
                    board[x][y] = 0;
                    pohyb = true;
                }
                volno_y++;
            }
        }   
        
        // Fáze 2: Sčítání
        for (int y = 0; y < 3; y++) { // Zde je y > 0 správně, abychom nekontrolovali board[x][-1]
            if (board[x][y] != 0 && board[x][y] == board[x][y+1]) {
                board[x][y] = board[x][y] * 2;
                score += board[x][y];
                board[x][y+1] = 0;
                pohyb = true;
            }
        }
        
        // Fáze 3: Gravitace po sčítání
        volno_y = 0;
        for (int y = 0; y <= 3; y++) {
            if (board[x][y] != 0) {
                int ulozeneCislo = board[x][y];
                if (y != volno_y) { // Pokud už není úplně dole
                    board[x][volno_y] = ulozeneCislo;
                    board[x][y] = 0;
                }
                volno_y++;
            }
        }
    }
    
    if (pohyb) {
        spawnRandomTile();
    }
}
void Game2048::moveDown() {
    pohyb = false;
    for (int x = 0; x < 4; x++) {
        
        // Fáze 1: Gravitace
        int volno_y = 3;
        for (int y = 3; y >= 0; y--) {
            if (board[x][y] != 0) {
                int ulozeneCislo = board[x][y];
                if (y != volno_y) { // Pokud už není úplně dole
                    board[x][volno_y] = ulozeneCislo;
                    board[x][y] = 0;
                    pohyb = true;
                }
                volno_y--;
            }
        }   
        
        // Fáze 2: Sčítání
        for (int y = 3; y > 0; y--) { // Zde je y > 0 správně, abychom nekontrolovali board[x][-1]
            if (board[x][y] != 0 && board[x][y] == board[x][y-1]) {
                board[x][y] = board[x][y] * 2;
                score += board[x][y];
                board[x][y-1] = 0;
                pohyb = true;
            }
        }
        
        // Fáze 3: Gravitace po sčítání
        volno_y = 3;
        for (int y = 3; y >= 0; y--) {
            if (board[x][y] != 0) {
                int ulozeneCislo = board[x][y];
                if (y != volno_y) {
                    board[x][volno_y] = ulozeneCislo;
                    board[x][y] = 0;
                }
                volno_y--;
            }
        } 
    }

    if (pohyb) {
        spawnRandomTile();
    }
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

    // Vykreslení skóre do horního okraje
    canvas->setTextColor(ST77XX_WHITE);
    canvas->setTextSize(2);
    canvas->setCursor(10, 2);
    canvas->print("Skore: ");
    canvas->print(score);

    // Výpočet pozic pro vykreslení krásně doprostřed displeje
    int tileSize = 50;
    int spacing = 6;
    int startX = (320 - (4 * tileSize + 3 * spacing)) / 2; 
    int startY = 20; // Hrací plocha začíná o kousek níž, aby se vešlo skóre

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
