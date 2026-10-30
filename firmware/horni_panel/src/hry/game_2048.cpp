#include "game_2048.h"

Game2048::Game2048() {
    bestScore = 0;
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

bool Game2048::checkGameOver() {
    // 1. Pokud existuje jakékoliv volné pole, hra pokračuje
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (board[x][y] == 0) return false;
        }
    }

    // 2. Pokud je plocha plná, zkontrolujeme, zda lze ještě něco sečíst
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (x < 3 && board[x][y] == board[x + 1][y]) return false;
            if (y < 3 && board[x][y] == board[x][y + 1]) return false;
        }
    }

    // Žádné prázdné pole a žádný možný tah -> GAME OVER
    return true;
}

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
                        // Po vložení dlaždice ihned zkontrolujeme, zda ještě zbývá nějaký tah
                        if (checkGameOver()) {
                            currentState = G2048_GAME_OVER;
                        }
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
    if (currentState == G2048_GAME_OVER) return;
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
    } else {
        if (checkGameOver()) {
            currentState = G2048_GAME_OVER;
            pohyb = true;
        }
    }
}
void Game2048::moveRight() {
    if (currentState == G2048_GAME_OVER) return;
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
    } else {
        if (checkGameOver()) {
            currentState = G2048_GAME_OVER;
            pohyb = true;
        }
    }
}
void Game2048::moveUp() {
    if (currentState == G2048_GAME_OVER) return;
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
    } else {
        if (checkGameOver()) {
            currentState = G2048_GAME_OVER;
            pohyb = true;
        }
    }
}
void Game2048::moveDown() {
    if (currentState == G2048_GAME_OVER) return;
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
    } else {
        if (checkGameOver()) {
            currentState = G2048_GAME_OVER;
            pohyb = true;
        }
    }
}

// ------------------------------------------------------------------
// KONEC DOMÁCÍHO ÚKOLU (Grafiku máš hotovou dole)
// ------------------------------------------------------------------


// Pomocná funkce pro barvy (RGB565)
uint16_t Game2048::getColorForNumber(int num) {
    switch (num) {
        case 0:    return ST77XX_DARKGREY; // Prázdné políčko (tmavě šedá)
        case 2:    return 0x07FF;          // Azurová (Cyan)       - RGB(0, 255, 255)
        case 4:    return 0x001F;          // Sytá modrá           - RGB(0, 0, 255)
        case 8:    return 0x07E0;          // Jasná zelená         - RGB(0, 255, 0)
        case 16:   return 0xFFE0;          // Žlutá                - RGB(255, 255, 0)
        case 32:   return 0xFD20;          // Oranžová             - RGB(255, 165, 0)
        case 64:   return 0xF800;          // Červená              - RGB(255, 0, 0)
        case 128:  return 0xF81F;          // Purpurová / Magenta  - RGB(255, 0, 255)
        case 256:  return 0xF8B2;          // Sytě růžová (Pink)   - RGB(255, 20, 147)
        case 512:  return 0x07EF;          // Jarní mořská zelená  - RGB(0, 255, 128)
        case 1024: return 0x7FE0;          // Limetková / Sv.zelená- RGB(128, 255, 0)
        case 2048: return 0xFEA0;          // Zlatá (Gold)         - RGB(255, 215, 0)
        default:   return ST77XX_WHITE;    // Nad 2048 bílá
    }
}

// ==================================================================
// AI AUTOPILOT - POMOCNÉ FUNKCE
// ==================================================================

// 1. SIMULACE TAHU NANEČISTO
// dir: 0 = NAHORU, 1 = DOLŮ, 2 = DOLEVA, 3 = DOPRAVA
bool Game2048::simulateMove(const int src[4][4], int dest[4][4], int dir) {
    // TODO: 
    // 1. Zkopírovat src do dest
    for(int x = 0; x < 4; x++) {
        for(int y = 0; y < 4; y++) {
            dest[x][y] = src[x][y];
        }
    }
    
    switch(dir){
        case 0:{

            for (int x = 0; x < 4; x++) {
        
                // Fáze 1: Gravitace
                int volno_y = 0;
                for (int y = 0; y <= 3; y++) {
                    if (dest[x][y] != 0) {
                        int ulozeneCislo = dest[x][y];
                        if (y != volno_y) { // Pokud už není úplně dole
                            dest[x][volno_y] = ulozeneCislo;
                            dest[x][y] = 0;
                        }
                        volno_y++;
                    }
                }   
                
                // Fáze 2: Sčítání
                for (int y = 0; y < 3; y++) { // Zde je y > 0 správně, abychom nekontrolovali dest[x][-1]
                    if (dest[x][y] != 0 && dest[x][y] == dest[x][y+1]) {
                        dest[x][y] = dest[x][y] * 2;
                        dest[x][y+1] = 0;
                    }
                }
                
                // Fáze 3: Gravitace po sčítání
                volno_y = 0;
                for (int y = 0; y <= 3; y++) {
                    if (dest[x][y] != 0) {
                        int ulozeneCislo = dest[x][y];
                        if (y != volno_y) { // Pokud už není úplně dole
                            dest[x][volno_y] = ulozeneCislo;
                            dest[x][y] = 0;
                        }
                        volno_y++;
                    }
                }
            }
        
            // NAHORU
            break;
        }
        case 1:{

            for (int x = 0; x < 4; x++) {
        
                // Fáze 1: Gravitace
                int volno_y = 3;
                for (int y = 3; y >= 0; y--) {
                    if (dest[x][y] != 0) {
                        int ulozeneCislo = dest[x][y];
                        if (y != volno_y) { // Pokud už není úplně dole
                            dest[x][volno_y] = ulozeneCislo;
                            dest[x][y] = 0;
                        }
                        volno_y--;
                    }
                }   
                
                // Fáze 2: Sčítání
                for (int y = 3; y > 0; y--) { // Zde je y > 0 správně, abychom nekontrolovali dest[x][-1]
                    if (dest[x][y] != 0 && dest[x][y] == dest[x][y-1]) {
                        dest[x][y] = dest[x][y] * 2;
                        dest[x][y-1] = 0;
                    }
                }
                
                // Fáze 3: Gravitace po sčítání
                volno_y = 3;
                for (int y = 3; y >= 0; y--) {
                    if (dest[x][y] != 0) {
                        int ulozeneCislo = dest[x][y];
                        if (y != volno_y) {
                            dest[x][volno_y] = ulozeneCislo;
                            dest[x][y] = 0;
                        }
                        volno_y--;
                    }
                } 
            }
            // DOLŮ
            break;
        }
        case 2:{

            for (int y = 0; y < 4; y++) {
                // Fáze 1: Gravitace doleva
                int volno_x = 0;
                for (int x = 0; x <= 3; x++) {
                    if (dest[x][y] != 0) {
                        int ulozeneCislo = dest[x][y];
                        if (x != volno_x) {
                            dest[volno_x][y] = ulozeneCislo;
                            dest[x][y] = 0;
                        }
                        volno_x++;
                    }
                }   
                
                // Fáze 2: Sčítání
                for (int x = 0; x < 3; x++) {
                    if (dest[x][y] != 0 && dest[x][y] == dest[x+1][y]) {
                        dest[x][y] = dest[x][y] * 2;
                        dest[x+1][y] = 0;
                    }
                }
                
                // Fáze 3: Gravitace doleva po sčítání
                volno_x = 0;
                for (int x = 0; x <= 3; x++) {
                    if (dest[x][y] != 0) {
                        int ulozeneCislo = dest[x][y];
                        if (x != volno_x) {
                            dest[volno_x][y] = ulozeneCislo;
                            dest[x][y] = 0;
                        }
                        volno_x++;
                    }
                }
            }
            // DOLEVA
            break;
        }
        case 3: { // DOPRAVA
            for (int y = 0; y < 4; y++) {
                // Fáze 1: Gravitace doprava
                int volno_x = 3;
                for (int x = 3; x >= 0; x--) {
                    if (dest[x][y] != 0) {
                        int ulozeneCislo = dest[x][y];
                        if (x != volno_x) {
                            dest[volno_x][y] = ulozeneCislo;
                            dest[x][y] = 0;
                        }
                        volno_x--;
                    }
                }   
                
                // Fáze 2: Sčítání doprava
                for (int x = 3; x > 0; x--) {
                    if (dest[x][y] != 0 && dest[x][y] == dest[x-1][y]) {
                        dest[x][y] = dest[x][y] * 2;
                        dest[x-1][y] = 0;
                    }
                }
                
                // Fáze 3: Gravitace doprava po sčítání
                volno_x = 3;
                for (int x = 3; x >= 0; x--) {
                    if (dest[x][y] != 0) {
                        int ulozeneCislo = dest[x][y];
                        if (x != volno_x) {
                            dest[volno_x][y] = ulozeneCislo;
                            dest[x][y] = 0;
                        }
                        volno_x--;
                    }
                }
            }
            break;
        }
    }

    // 3. Porovnat dest se src:
    //    - Pokud se aspoň jedno políčko liší, vrátit true (tah je platný)
    //    - Pokud je pole úplně stejné, vrátit false (tímto směrem se nedá hnout)

    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
            if (dest[x][y] != src[x][y]) return true; // Změnilo se
        }
    }
    return false; // Deska zůstala stejná
}

// 2. VYHODNOCENÍ DESKY (HEURISTIKA)
int Game2048::evaluateBoard(const int b[4][4]) {
    int totalScore = 0;

    // -------------------------------------------------------------
    // PILÍŘ 1: VÁHOVÁ MATICE (Jemnější had 1..16 končící na [3][3])
    // -------------------------------------------------------------
    // Používáme škálu 1..16, aby matice nepřeválcovala ostatní pilíře
    const int weights[4][4] = {
        {  4,  3,  2,  1 },   // y = 0: <- zprava doleva
        {  5,  6,  7,  8 },   // y = 1: -> zleva doprava
        { 12, 11, 10,  9 },   // y = 2: <- zprava doleva
        { 13, 14, 15, 16 }    // y = 3: -> zleva doprava (největší váha 16 na [3][3] vpravo dole)
    };

    for (int y = 0; y <= 3; y++) {
        for (int x = 0; x <= 3; x++) {
            totalScore += b[x][y] * weights[y][x];
        }
    }

    // -------------------------------------------------------------
    // PILÍŘ 2: POČET VOLNÝCH POLÍČEK
    // -------------------------------------------------------------
    for (int y = 0; y <= 3; y++) {
        for (int x = 0; x <= 3; x++) {
            if (b[x][y] == 0) {
                totalScore += 500;
            }
        }
    }

    // -------------------------------------------------------------
    // PILÍŘ 3: MAXIMUM V ROHU [3][3]
    // -------------------------------------------------------------
    int max_val = 0;
    for (int y = 0; y <= 3; y++) {
        for (int x = 0; x <= 3; x++) {
            if (b[x][y] > max_val) {
                max_val = b[x][y];
            }
        }
    }

    if (max_val == b[3][3]) {
        totalScore += 10000;
    }

    // -------------------------------------------------------------
    // PILÍŘ 4: SLUČITELNOST SOUSEDŮ (MERGE BONUS)
    // -------------------------------------------------------------
    // Horizontální sousedé (x a x+1)
    for (int y = 0; y <= 3; y++) {
        for (int x = 0; x <= 2; x++) {
            if (b[x][y] != 0 && b[x][y] == b[x + 1][y]) {
                totalScore += b[x][y] * 2;
            }
        }
    }

    // Vertikální sousedé (y a y+1)
    for (int x = 0; x <= 3; x++) {
        for (int y = 0; y <= 2; y++) {
            if (b[x][y] != 0 && b[x][y] == b[x][y + 1]) {
                totalScore += b[x][y] * 2;
            }
        }
    }

    return totalScore;
}


// 3. VÝBĚR A PROVEDENÍ NEJLEPŠÍHO TAHU
void Game2048::autoStep() {
    if (currentState == G2048_GAME_OVER) {
        return;
    }

    int bestDir = -1;
    int bestScore = -1000000;

    // Otestujeme všechny 4 směry (0 = UP, 1 = DOWN, 2 = LEFT, 3 = RIGHT)
    for (int dir = 0; dir < 4; dir++) {
        int simulatedBoard[4][4];

        // 1. Lze se tímto směrem pohnout?
        if (!simulateMove(board, simulatedBoard, dir)) {
            continue; // Neplatný tah, přeskočit
        }

        // 2. Ohodnotit výsledný stav
        int score = evaluateBoard(simulatedBoard);

        // 3. Uchovat nejlepší směr
        if (score > bestScore) {
            bestScore = score;
            bestDir = dir;
        }
    }

    // 4. Provedení reálného tahu
    if (bestDir != -1) {
        switch (bestDir) {
            case 0: moveUp();    break;
            case 1: moveDown();  break;
            case 2: moveLeft();  break;
            case 3: moveRight(); break;
        }
    } else {
        // Žádný směr nevedl k platnému tahu
        currentState = G2048_GAME_OVER;
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

    // Vykreslení skóre do horního okraje (navrchu)
    canvas->setTextColor(ST77XX_WHITE);
    canvas->setTextSize(2);
    canvas->setCursor(10, 2);
    canvas->print("Skore: ");
    canvas->print(score);

    if (currentState == G2048_GAME_OVER) {
        canvas->fillRect(55, 95, 210, 50, ST77XX_RED);
        canvas->drawRect(55, 95, 210, 50, ST77XX_WHITE);
        canvas->setTextColor(ST77XX_WHITE);
        canvas->setTextSize(3);
        canvas->setCursor(79, 108);
        canvas->print("GAME OVER");
    }

    gfx.pushCanvasToScreen();
}
