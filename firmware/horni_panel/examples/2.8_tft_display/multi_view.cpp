#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <math.h>

// Definice pinů pro ESP32-S3 a displej
#define TFT_CS   10
#define TFT_DC   13
#define TFT_RST  14
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_MISO -1 // MISO nepoužíváme

// Inicializace knihovny Adafruit_ST7789 s hardwarovou SPI
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Společné barvy a nastavení pro obrazovky
#define COLOR_CARD        0x0841 // Velmi tmavá šedá s nádechem modré
#define COLOR_BORDER      0x2104 // Tmavá šedá pro ohraničení
#define COLOR_TEXT_MUTED  0x7BEF // Tlumená šedá pro popisky
#define COLOR_GRID        0x0183 // Tmavě modrá pro středovou osu grafu
#define COLOR_GRID_SUB    0x00A1 // Ještě tmavší modrá pro pomocné osy grafu

#define COLOR_HUD 0x3410 // Tmavá vojenská zelená pro futuristický vzhled
#define COLOR_HUD_ACTIVE 0x07FF // Světle tyrkysová pro aktivní HUD prvky

// Globální časovače a stavové proměnné
uint32_t last_screen_switch = 0;
int active_screen = 0; // 0 = Dashboard, 1 = 3D Engine, 2 = Snake Game

uint32_t lastUpdate = 0;
uint32_t lastFastUpdate = 0;
uint32_t lastFrame = 0;
uint32_t fpsTimer = 0;
uint32_t lastSnakeUpdate = 0;
int frameCount = 0;
int fpsValue = 0;
float angle = 0;

// Nastavení rozměrů grafu (Screen 0)
#define GRAPH_X 172
#define GRAPH_Y 82
#define GRAPH_W 131
#define GRAPH_H 136
#define GRAPH_CENTER_Y (GRAPH_Y + GRAPH_H / 2)
int y_values[GRAPH_W];

// 3D Starfield nastavení (Screen 1)
#define NUM_STARS 55
struct Star {
  float x, y, z;
  int old_sx, old_sy;
} stars[NUM_STARS];

// Definice vrcholů 3D kostky (v rozsahu -1 až 1)
const float cube_verts[8][3] = {
  {-1.0f, -1.0f, -1.0f},
  { 1.0f, -1.0f, -1.0f},
  { 1.0f,  1.0f, -1.0f},
  {-1.0f,  1.0f, -1.0f},
  {-1.0f, -1.0f,  1.0f},
  { 1.0f, -1.0f,  1.0f},
  { 1.0f,  1.0f,  1.0f},
  {-1.0f,  1.0f,  1.0f}
};

const int cube_edges[12][2] = {
  {0, 1}, {1, 2}, {2, 3}, {3, 0},
  {4, 5}, {5, 6}, {6, 7}, {7, 4},
  {0, 4}, {1, 5}, {2, 6}, {3, 7}
};

// Rotace a souřadnice pro kostky (Screen 1)
float ax1 = 0.0f, ay1 = 0.0f, az1 = 0.0f;
int px1[8], py1[8];
int old_px1[8], old_py1[8];

float ax2 = 0.0f, ay2 = 0.0f, az2 = 0.0f;
int px2[8], py2[8];
int old_px2[8], old_py2[8];

// Proměnné pro hru Had (Screen 2)
int snake_x[100];
int snake_y[100];
int snake_len = 3;
int snake_dir = 0; // 0=Doprava, 1=Dolu, 2=Doleva, 3=Nahoru
int apple_x = 0;
int apple_y = 0;

// Funkce pro 3D rotaci a perspektivní projekci bodu na 2D obrazovku
void rotate_and_project(float x, float y, float z, float ax, float ay, float az, float size, int &sx, int &sy) {
  float cosy = cos(ax);
  float siny = sin(ax);
  float y1 = y * cosy - z * siny;
  float z1 = y * siny + z * cosy;

  float cosx = cos(ay);
  float sinx = sin(ay);
  float x2 = x * cosx + z1 * sinx;
  float z2 = -x * sinx + z1 * cosx;

  float cosz = cos(az);
  float sinz = sin(az);
  float x3 = x2 * cosz - y1 * sinz;
  float y3 = x2 * sinz + y1 * cosz;

  x3 *= size;
  y3 *= size;
  z2 *= size;

  float distance = 140.0f;
  float scale = 140.0f;
  float proj_z = distance + z2;

  if (proj_z <= 0.1f) proj_z = 0.1f;

  sx = 160 + (int)((x3 * scale) / proj_z);
  sy = 120 + (int)((y3 * scale) / proj_z);
}

// Funkce pro vykreslení laserové přechodové animace
void play_transition() {
  Serial.println("[WIPE] Spousteni skenovaciho prechodu...");
  for (int x = 0; x < 320; x += 16) {
    tft.fillRect(x, 0, 16, 240, ST77XX_BLACK);
    tft.drawFastVLine(x + 16, 0, 240, ST77XX_CYAN);
    tft.drawFastVLine(x + 17, 0, 240, ST77XX_WHITE);
    delay(12);
  }
}

// Inicializace retro hry Had
void init_snake_game() {
  tft.fillRect(0, 20, 320, 220, ST77XX_BLACK);
  
  snake_len = 3;
  snake_dir = 0;
  snake_x[0] = 5; snake_y[0] = 10;
  snake_x[1] = 4; snake_y[1] = 10;
  snake_x[2] = 3; snake_y[2] = 10;
  
  apple_x = rand() % 32;
  apple_y = rand() % 22;
  
  // Vykreslení hada
  for (int i = 0; i < snake_len; i++) {
    tft.fillRect(snake_x[i] * 10, 20 + snake_y[i] * 10, 10, 10, ST77XX_GREEN);
  }
  // Vykreslení jablka
  tft.fillCircle(apple_x * 10 + 5, 20 + apple_y * 10 + 5, 4, ST77XX_RED);
  
  // Vykreslení horního HUDu
  tft.fillRect(0, 0, 320, 20, 0x10A2);
  tft.drawFastHLine(0, 20, 320, 0x3186);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 6);
  tft.print("RETRO SNAKE GAME (AUTOPLAY)");
  tft.setCursor(220, 6);
  tft.print("SCORE: 0");
}

// Aktualizace a chování hada
void update_snake() {
  int head_x = snake_x[0];
  int head_y = snake_y[0];
  
  int diff_x = apple_x - head_x;
  int diff_y = apple_y - head_y;
  
  int preferred_dir = snake_dir;
  
  // Rozhodnutí o nejlepší cestě k jablku
  if (abs(diff_x) >= abs(diff_y) && diff_x != 0) {
    preferred_dir = (diff_x > 0) ? 0 : 2; // Vpravo nebo vlevo
  } else if (diff_y != 0) {
    preferred_dir = (diff_y > 0) ? 1 : 3; // Dolu nebo nahoru
  }
  
  // Ověření, že preferovaný směr nevede ke kolizi
  int new_x = head_x;
  int new_y = head_y;
  if (preferred_dir == 0) new_x++;
  else if (preferred_dir == 1) new_y++;
  else if (preferred_dir == 2) new_x--;
  else if (preferred_dir == 3) new_y--;
  
  bool collides = false;
  if (new_x < 0 || new_x >= 32 || new_y < 0 || new_y >= 22) collides = true;
  for (int i = 0; i < snake_len; i++) {
    if (snake_x[i] == new_x && snake_y[i] == new_y) collides = true;
  }
  
  // Pokud koliduje, zkusíme najít jakýkoliv jiný volný směr
  if (collides) {
    for (int d = 0; d < 4; d++) {
      int check_dir = d;
      if (abs(check_dir - snake_dir) == 2) continue; // Neotáčet se o 180 stupňů
      
      int cx = head_x;
      int cy = head_y;
      if (check_dir == 0) cx++;
      else if (check_dir == 1) cy++;
      else if (check_dir == 2) cx--;
      else if (check_dir == 3) cy--;
      
      bool c_collides = false;
      if (cx < 0 || cx >= 32 || cy < 0 || cy >= 22) c_collides = true;
      for (int i = 0; i < snake_len; i++) {
        if (snake_x[i] == cx && snake_y[i] == cy) c_collides = true;
      }
      
      if (!c_collides) {
        preferred_dir = check_dir;
        new_x = cx;
        new_y = cy;
        collides = false;
        break;
      }
    }
  }
  
  snake_dir = preferred_dir;
  
  // Uložení starého ocasu pro smazání
  int tail_x = snake_x[snake_len - 1];
  int tail_y = snake_y[snake_len - 1];
  
  // Posun těla
  for (int i = snake_len - 1; i > 0; i--) {
    snake_x[i] = snake_x[i - 1];
    snake_y[i] = snake_y[i - 1];
  }
  
  snake_x[0] = new_x;
  snake_y[0] = new_y;
  
  // Kontrola snězení jablka
  if (new_x == apple_x && new_y == apple_y) {
    if (snake_len < 100) {
      snake_x[snake_len] = tail_x;
      snake_y[snake_len] = tail_y;
      snake_len++;
    }
    apple_x = rand() % 32;
    apple_y = rand() % 22;
    tft.fillCircle(apple_x * 10 + 5, 20 + apple_y * 10 + 5, 4, ST77XX_RED);
    
    // Překreslení skóre
    tft.fillRect(270, 0, 50, 20, 0x10A2);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(270, 6);
    tft.print(snake_len - 3);
  } else {
    // Smazání ocasu
    tft.fillRect(tail_x * 10, 20 + tail_y * 10, 10, 10, ST77XX_BLACK);
  }
  
  // Kontrola smrti
  if (collides) {
    init_snake_game();
    return;
  }
  
  // Vykreslení nové hlavy
  tft.fillRect(snake_x[0] * 10, 20 + snake_y[0] * 10, 10, 10, ST77XX_GREEN);
  // Očičko
  tft.fillRect(snake_x[0] * 10 + 2, 20 + snake_y[0] * 10 + 2, 2, 2, ST77XX_BLACK);
}

// Inicializace specifických grafických prvků pro vybranou obrazovku
void init_screen(int screen) {
  tft.fillScreen(ST77XX_BLACK);
  
  if (screen == 0) {
    Serial.println("[SCREEN] Inicializace Dashboardu...");
    // 1. Horní panel (Header)
    tft.fillRoundRect(10, 8, 300, 30, 6, 0x10A2);
    tft.drawRoundRect(10, 8, 300, 30, 6, 0x3186);
    
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 15);
    tft.print("ESP32-S3 DEMO BOX");

    // 2. Levá karta - SYSTEM STATUS
    tft.fillRoundRect(10, 46, 145, 184, 8, COLOR_CARD);
    tft.drawRoundRect(10, 46, 145, 184, 8, COLOR_BORDER);
    
    tft.setTextColor(ST77XX_ORANGE);
    tft.setTextSize(1);
    tft.setCursor(20, 56);
    tft.print("SYSTEM STATUS");
    tft.drawFastHLine(15, 68, 135, COLOR_BORDER);

    // 3. Pravá karta - OSCILLOSCOPE
    tft.fillRoundRect(165, 46, 145, 184, 8, COLOR_CARD);
    tft.drawRoundRect(165, 46, 145, 184, 8, COLOR_BORDER);
    
    tft.setTextColor(ST77XX_CYAN);
    tft.setTextSize(1);
    tft.setCursor(175, 56);
    tft.print("OSCILLOSCOPE");
    tft.drawFastHLine(170, 68, 135, COLOR_BORDER);

    // Reset hodnot grafu na středovou hodnotu
    for (int i = 0; i < GRAPH_W; i++) {
      y_values[i] = GRAPH_CENTER_Y;
    }
  } 
  else if (screen == 1) {
    Serial.println("[SCREEN] Inicializace 3D Enginu...");
    // Rohové zaměřovací závorky HUDu
    tft.drawLine(6, 6, 26, 6, COLOR_HUD);
    tft.drawLine(6, 6, 6, 26, COLOR_HUD);
    tft.drawLine(314, 6, 294, 6, COLOR_HUD);
    tft.drawLine(314, 6, 314, 26, COLOR_HUD);
    tft.drawLine(6, 234, 26, 234, COLOR_HUD);
    tft.drawLine(6, 234, 6, 214, COLOR_HUD);
    tft.drawLine(314, 234, 294, 234, COLOR_HUD);
    tft.drawLine(314, 234, 314, 214, COLOR_HUD);

    // Hlavní popisek nahoře
    tft.setTextColor(ST77XX_GREEN);
    tft.setTextSize(1);
    tft.setCursor(95, 8);
    tft.print("ESP32-S3 3D RENDER ENGINE");

    // Status dole vpravo
    tft.setTextColor(COLOR_HUD_ACTIVE);
    tft.setCursor(220, 222);
    tft.print("3D_SPACE: ACTIVE");

    // Inicializace 3D hvězdného pole (náhodné pozice)
    for (int i = 0; i < NUM_STARS; i++) {
      stars[i].x = (float)(rand() % 200 - 100);
      stars[i].y = (float)(rand() % 160 - 80);
      stars[i].z = (float)(rand() % 199 + 1);
      stars[i].old_sx = -1;
      stars[i].old_sy = -1;
    }

    // Výchozí projekce kostek pro zamezení chyb při mazání v prvním kroku
    for (int i = 0; i < 8; i++) {
      rotate_and_project(cube_verts[i][0], cube_verts[i][1], cube_verts[i][2], ax1, ay1, az1, 65.0f, old_px1[i], old_py1[i]);
      rotate_and_project(cube_verts[i][0], cube_verts[i][1], cube_verts[i][2], ax2, ay2, az2, 32.0f, old_px2[i], old_py2[i]);
    }
  }
  else if (screen == 2) {
    init_snake_game();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Spousteni Multi-View Systemu...");

  // Inicializace hardwarové SPI sběrnice
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);

  // Inicializace displeje jako 240x320 ST7789
  tft.init(240, 320);
  tft.invertDisplay(false); // Vypnutí výchozí inverze
  tft.setRotation(1);       // Nastavení režimu na šířku (320x240)

  // Spuštění první obrazovky (Dashboard)
  active_screen = 0;
  init_screen(active_screen);
  
  last_screen_switch = millis();
  lastUpdate = millis();
  lastFastUpdate = millis();
  lastFrame = millis();
  fpsTimer = millis();
}

void loop() {
  uint32_t currentMillis = millis();

  // Status automat pro přepínání obrazovek každých 5 sekund (5000 ms)
  if (currentMillis - last_screen_switch >= 5000) {
    play_transition(); // Spuštění animace skeneru
    
    // Záměna obrazovky (0 -> 1 -> 2 -> 0)
    active_screen = (active_screen + 1) % 3;
    
    init_screen(active_screen); // Inicializace nové grafiky
    
    // Reset všech lokálních časovačů pro bezchybný start
    last_screen_switch = millis();
    lastUpdate = millis();
    lastFastUpdate = millis();
    lastFrame = millis();
    fpsTimer = millis();
    lastSnakeUpdate = millis();
    frameCount = 0;
    
    return; // Přeskočení zbytku smyčky v tomto cyklu
  }

  // --- RENDEROVACÍ LOGIKA AKTIVNÍ OBRAZOVKY ---
  
  if (active_screen == 0) {
    // === OBRAZOVKA 0: SYSTÉMOVÝ DASHBOARD ===
    
    if (currentMillis - lastFastUpdate >= 30) {
      lastFastUpdate = currentMillis;

      // A. Smazání starého grafu překreslením barvou pozadí karty
      for (int i = 0; i < GRAPH_W - 1; i++) {
        tft.drawLine(GRAPH_X + i, y_values[i], GRAPH_X + i + 1, y_values[i + 1], COLOR_CARD);
      }
      tft.fillCircle(GRAPH_X + GRAPH_W - 2, y_values[GRAPH_W - 2], 2, COLOR_CARD);

      // B. Vykreslení referenčních čar mřížky
      tft.drawFastHLine(GRAPH_X, GRAPH_CENTER_Y, GRAPH_W, COLOR_GRID); 
      tft.drawFastHLine(GRAPH_X, GRAPH_CENTER_Y - 30, GRAPH_W, COLOR_GRID_SUB); 
      tft.drawFastHLine(GRAPH_X, GRAPH_CENTER_Y + 30, GRAPH_W, COLOR_GRID_SUB); 

      // C. Posun dat v grafu doleva
      for (int i = 0; i < GRAPH_W - 1; i++) {
        y_values[i] = y_values[i + 1];
      }

      // D. Výpočet nové vlnové hodnoty
      angle += 0.08f;
      float val = sin(angle) * 28.0f + sin(angle * 2.3f) * 10.0f;
      y_values[GRAPH_W - 1] = GRAPH_CENTER_Y + (int)val;

      // E. Vykreslení neonově zelené linky signálu
      for (int i = 0; i < GRAPH_W - 1; i++) {
        tft.drawLine(GRAPH_X + i, y_values[i], GRAPH_X + i + 1, y_values[i + 1], ST77XX_GREEN);
      }
      tft.fillCircle(GRAPH_X + GRAPH_W - 2, y_values[GRAPH_W - 2], 2, ST77XX_RED);

      // F. Blikání LED kontrolky v záhlaví
      bool ledOn = (currentMillis / 500) % 2 == 0;
      tft.fillCircle(290, 23, 4, ledOn ? ST77XX_GREEN : 0x03E0);

      // G. Animovaný progress bar
      int progress_w = map((currentMillis % 10000), 0, 10000, 0, 125);
      tft.fillRect(20, 208, 125, 6, COLOR_BORDER);
      tft.fillRect(20, 208, progress_w, 6, ST77XX_CYAN);
    }

    if (currentMillis - lastUpdate >= 1000) {
      lastUpdate = currentMillis;

      // Uptime
      tft.fillRect(20, 78, 125, 30, COLOR_CARD);
      tft.setTextColor(COLOR_TEXT_MUTED);
      tft.setTextSize(1);
      tft.setCursor(20, 78);
      tft.print("Uptime:");
      tft.setTextColor(ST77XX_WHITE);
      tft.setTextSize(2);
      tft.setCursor(20, 88);
      tft.print(currentMillis / 1000);
      tft.print(" s");

      // Free RAM
      tft.fillRect(20, 120, 125, 30, COLOR_CARD);
      tft.setTextColor(COLOR_TEXT_MUTED);
      tft.setTextSize(1);
      tft.setCursor(20, 120);
      tft.print("Free RAM:");
      tft.setTextColor(ST77XX_YELLOW);
      tft.setTextSize(2);
      tft.setCursor(20, 130);
      tft.print(ESP.getFreeHeap() / 1024);
      tft.print(" KB");

      // CPU Temp (simulovaná)
      tft.fillRect(20, 162, 125, 30, COLOR_CARD);
      tft.setTextColor(COLOR_TEXT_MUTED);
      tft.setTextSize(1);
      tft.setCursor(20, 162);
      tft.print("CPU Temp:");
      tft.setTextColor(ST77XX_ORANGE);
      tft.setTextSize(2);
      tft.setCursor(20, 172);
      float simTemp = 41.5f + (float)(rand() % 10) / 10.0f;
      tft.print(simTemp, 1);
      tft.print(" C");
    }
  } 
  else if (active_screen == 1) {
    // === OBRAZOVKA 1: 3D PROJEKČNÍ ENGINE ===
    
    if (currentMillis - lastFrame >= 25) {
      lastFrame = currentMillis;
      frameCount++;

      // A. Smazání starých kostek a hvězd
      for (int i = 0; i < 12; i++) {
        int p0 = cube_edges[i][0];
        int p1 = cube_edges[i][1];
        tft.drawLine(old_px1[p0], old_py1[p0], old_px1[p1], old_py1[p1], ST77XX_BLACK);
        tft.drawLine(old_px2[p0], old_py2[p0], old_px2[p1], old_py2[p1], ST77XX_BLACK);
      }
      for (int i = 0; i < NUM_STARS; i++) {
        if (stars[i].old_sx >= 0) {
          tft.drawPixel(stars[i].old_sx, stars[i].old_sy, ST77XX_BLACK);
        }
      }

      // B. Aktualizace a vykreslení hvězdného pole
      for (int i = 0; i < NUM_STARS; i++) {
        stars[i].z -= 1.8f;
        if (stars[i].z <= 1.0f) {
          stars[i].x = (float)(rand() % 200 - 100);
          stars[i].y = (float)(rand() % 160 - 80);
          stars[i].z = 200.0f;
        }

        float scale = 140.0f;
        int sx = 160 + (int)((stars[i].x * scale) / stars[i].z);
        int sy = 120 + (int)((stars[i].y * scale) / stars[i].z);

        if (sx >= 0 && sx < 320 && sy >= 0 && sy < 240) {
          bool in_hud_area = (sy < 20 && sx > 70 && sx < 250) || 
                             (sy > 210 && sx < 85) || 
                             (sy > 210 && sx > 210);

          if (!in_hud_area) {
            uint16_t star_color = ST77XX_WHITE;
            if (stars[i].z > 140.0f) star_color = 0x4208;
            else if (stars[i].z > 70.0f) star_color = 0x8410;
            else if (stars[i].z > 30.0f) star_color = 0xC618;
            
            tft.drawPixel(sx, sy, star_color);
            stars[i].old_sx = sx;
            stars[i].old_sy = sy;
          } else {
            stars[i].old_sx = -1;
          }
        } else {
          stars[i].old_sx = -1;
        }
      }

      // C. Výpočet nových úhlů rotace
      ax1 += 0.015f; ay1 += 0.022f; az1 += 0.009f;
      ax2 -= 0.025f; ay2 -= 0.011f; az2 += 0.018f;

      // Projekce bodů kostek do 2D
      for (int i = 0; i < 8; i++) {
        rotate_and_project(cube_verts[i][0], cube_verts[i][1], cube_verts[i][2], ax1, ay1, az1, 65.0f, px1[i], py1[i]);
        rotate_and_project(cube_verts[i][0], cube_verts[i][1], cube_verts[i][2], ax2, ay2, az2, 32.0f, px2[i], py2[i]);
      }

      // D. Vykreslení hran obou kostek
      for (int i = 0; i < 12; i++) {
        int p0 = cube_edges[i][0];
        int p1 = cube_edges[i][1];
        tft.drawLine(px1[p0], py1[p0], px1[p1], py1[p1], ST77XX_CYAN);
        tft.drawLine(px2[p0], py2[p0], px2[p1], py2[p1], 0xF81F);
      }

      // Uložení aktuálních pozic pro mazání
      for (int i = 0; i < 8; i++) {
        old_px1[i] = px1[i]; old_py1[i] = py1[i];
        old_px2[i] = px2[i]; old_py2[i] = py2[i];
      }

      // E. Update textu úhlů na HUDu
      tft.fillRect(100, 222, 110, 10, ST77XX_BLACK);
      tft.setTextColor(ST77XX_YELLOW);
      tft.setTextSize(1);
      tft.setCursor(100, 222);
      tft.print("P:");
      tft.print((int)(ax1 * 57.2958f) % 360);
      tft.print(" Y:");
      tft.print((int)(ay1 * 57.2958f) % 360);
    }

    if (currentMillis - fpsTimer >= 1000) {
      fpsValue = frameCount;
      frameCount = 0;
      fpsTimer = currentMillis;

      tft.fillRect(20, 222, 60, 10, ST77XX_BLACK);
      tft.setTextColor(ST77XX_GREEN);
      tft.setTextSize(1);
      tft.setCursor(20, 222);
      tft.print("FPS: ");
      tft.print(fpsValue);
    }
  }
  else if (active_screen == 2) {
    // === OBRAZOVKA 2: AUTOPLAY HRA HAD ===
    if (currentMillis - lastSnakeUpdate >= 120) {
      lastSnakeUpdate = currentMillis;
      update_snake();
    }
  }
}
