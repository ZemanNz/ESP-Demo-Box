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

// Nastavení barev pro HUD
#define COLOR_HUD 0x3410 // Tmavá vojenská zelená pro futuristický vzhled
#define COLOR_HUD_ACTIVE 0x07FF // Světle tyrkysová pro aktivní HUD prvky

// 3D Starfield nastavení
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

// Definice 12 hran spojujících 8 vrcholů kostky
const int cube_edges[12][2] = {
  {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Zadní stěna
  {4, 5}, {5, 6}, {6, 7}, {7, 4}, // Přední stěna
  {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Spojovací hrany
};

// Rotace a souřadnice pro Vnější kostku (Cube 1)
float ax1 = 0.0f, ay1 = 0.0f, az1 = 0.0f;
int px1[8], py1[8];
int old_px1[8], old_py1[8];

// Rotace a souřadnice pro Vnitřní kostku (Cube 2)
float ax2 = 0.0f, ay2 = 0.0f, az2 = 0.0f;
int px2[8], py2[8];
int old_px2[8], old_py2[8];

// Funkce pro 3D rotaci a perspektivní projekci bodu na 2D obrazovku
void rotate_and_project(float x, float y, float z, float ax, float ay, float az, float size, int &sx, int &sy) {
  // Rotace kolem osy X (Pitch)
  float cosy = cos(ax);
  float siny = sin(ax);
  float y1 = y * cosy - z * siny;
  float z1 = y * siny + z * cosy;

  // Rotace kolem osy Y (Yaw)
  float cosx = cos(ay);
  float sinx = sin(ay);
  float x2 = x * cosx + z1 * sinx;
  float z2 = -x * sinx + z1 * cosx;

  // Rotace kolem osy Z (Roll)
  float cosz = cos(az);
  float sinz = sin(az);
  float x3 = x2 * cosz - y1 * sinz;
  float y3 = x2 * sinz + y1 * cosz;

  // Zvětšení podle rozměru objektu
  x3 *= size;
  y3 *= size;
  z2 *= size;

  // Matematická perspektivní projekce (vzdálenost kamery)
  float distance = 140.0f;
  float scale = 140.0f;
  float proj_z = distance + z2;

  if (proj_z <= 0.1f) proj_z = 0.1f; // Ochrana proti dělení nulou

  // Přepočet na souřadnice obrazovky se středem [160, 120]
  sx = 160 + (int)((x3 * scale) / proj_z);
  sy = 120 + (int)((y3 * scale) / proj_z);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Inicializace 3D grafickeho procesoru...");

  // Inicializace hardwarové SPI sběrnice
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);

  // Inicializace displeje jako 240x320 ST7789
  tft.init(240, 320);
  tft.invertDisplay(false); // Vypnutí výchozí inverze
  tft.setRotation(1);       // Landscape režim (320x240)

  // Smazání obrazovky na čistě černou
  tft.fillScreen(ST77XX_BLACK);

  // Inicializace 3D hvězdného pole (náhodné pozice)
  for (int i = 0; i < NUM_STARS; i++) {
    stars[i].x = (float)(rand() % 200 - 100);
    stars[i].y = (float)(rand() % 160 - 80);
    stars[i].z = (float)(rand() % 199 + 1);
    stars[i].old_sx = -1;
    stars[i].old_sy = -1;
  }

  // Výchozí projekce obou kostek (abychom měli co mazat)
  for (int i = 0; i < 8; i++) {
    rotate_and_project(cube_verts[i][0], cube_verts[i][1], cube_verts[i][2], 0, 0, 0, 65.0f, old_px1[i], old_py1[i]);
    rotate_and_project(cube_verts[i][0], cube_verts[i][1], cube_verts[i][2], 0, 0, 0, 32.0f, old_px2[i], old_py2[i]);
  }

  // --- Vykreslení statického futuristického HUD překryvu ---
  
  // Rohové zaměřovací závorky
  // Vlevo nahoře
  tft.drawLine(6, 6, 26, 6, COLOR_HUD);
  tft.drawLine(6, 6, 6, 26, COLOR_HUD);
  // Vpravo nahoře
  tft.drawLine(314, 6, 294, 6, COLOR_HUD);
  tft.drawLine(314, 6, 314, 26, COLOR_HUD);
  // Vlevo dole
  tft.drawLine(6, 234, 26, 234, COLOR_HUD);
  tft.drawLine(6, 234, 6, 214, COLOR_HUD);
  // Vpravo dole
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

  Serial.println("3D Prostredi pripraveno.");
}

void loop() {
  static uint32_t lastFrame = 0;
  static uint32_t fpsTimer = 0;
  static int frameCount = 0;
  static int fpsValue = 0;

  uint32_t currentMillis = millis();

  // Omezení snímkové frekvence na ~40 FPS (každých 25 ms) pro plynulý běh
  if (currentMillis - lastFrame >= 25) {
    lastFrame = currentMillis;
    frameCount++;

    // 1. MAZÁNÍ STARÝCH STRUKTUR (abychom eliminovali problikávání, mažeme pouze to, co jsme nakreslili)
    
    // Smazání hran vnější kostky
    for (int i = 0; i < 12; i++) {
      int p0 = cube_edges[i][0];
      int p1 = cube_edges[i][1];
      tft.drawLine(old_px1[p0], old_py1[p0], old_px1[p1], old_py1[p1], ST77XX_BLACK);
    }

    // Smazání hran vnitřní kostky
    for (int i = 0; i < 12; i++) {
      int p0 = cube_edges[i][0];
      int p1 = cube_edges[i][1];
      tft.drawLine(old_px2[p0], old_py2[p0], old_px2[p1], old_py2[p1], ST77XX_BLACK);
    }

    // Smazání starých hvězd
    for (int i = 0; i < NUM_STARS; i++) {
      if (stars[i].old_sx >= 0) {
        tft.drawPixel(stars[i].old_sx, stars[i].old_sy, ST77XX_BLACK);
      }
    }

    // 2. AKTUALIZACE A VYKRESLENÍ 3D HVĚZDNÉHO POLE
    for (int i = 0; i < NUM_STARS; i++) {
      stars[i].z -= 1.8f; // Posun hvězdy k pozorovateli
      
      // Pokud hvězda proletěla kolem nás, vygenerujeme novou v dálce
      if (stars[i].z <= 1.0f) {
        stars[i].x = (float)(rand() % 200 - 100);
        stars[i].y = (float)(rand() % 160 - 80);
        stars[i].z = 200.0f;
      }

      float scale = 140.0f;
      int sx = 160 + (int)((stars[i].x * scale) / stars[i].z);
      int sy = 120 + (int)((stars[i].y * scale) / stars[i].z);

      // Pokud je hvězda na obrazovce
      if (sx >= 0 && sx < 320 && sy >= 0 && sy < 240) {
        // Vyhneme se překreslení HUD textů
        bool in_hud_area = (sy < 20 && sx > 70 && sx < 250) || 
                           (sy > 210 && sx < 85) || 
                           (sy > 210 && sx > 210);

        if (!in_hud_area) {
          // Barva hvězdy podle její hloubky (vzdálenější hvězdy jsou tmavší)
          uint16_t star_color = ST77XX_WHITE;
          if (stars[i].z > 140.0f) star_color = 0x4208;      // Tmavě šedá
          else if (stars[i].z > 70.0f) star_color = 0x8410;  // Středně šedá
          else if (stars[i].z > 30.0f) star_color = 0xC618;  // Světle šedá
          
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

    // 3. VÝPOČET NOVÝCH ROTACÍ KOSTEK (každá rotuje jinou rychlostí v jiném směru)
    ax1 += 0.015f; ay1 += 0.022f; az1 += 0.009f;
    ax2 -= 0.025f; ay2 -= 0.011f; az2 += 0.018f;

    // Přepočet 3D bodů do 2D prostoru
    for (int i = 0; i < 8; i++) {
      rotate_and_project(cube_verts[i][0], cube_verts[i][1], cube_verts[i][2], ax1, ay1, az1, 65.0f, px1[i], py1[i]);
      rotate_and_project(cube_verts[i][0], cube_verts[i][1], cube_verts[i][2], ax2, ay2, az2, 32.0f, px2[i], py2[i]);
    }

    // 4. VYKRESLENÍ HRAN KOSTEK
    
    // Vnější kostka (Cyan / Tyrkysová)
    for (int i = 0; i < 12; i++) {
      int p0 = cube_edges[i][0];
      int p1 = cube_edges[i][1];
      tft.drawLine(px1[p0], py1[p0], px1[p1], py1[p1], ST77XX_CYAN);
    }

    // Vnitřní kostka (Vibrant Pink / Růžová)
    for (int i = 0; i < 12; i++) {
      int p0 = cube_edges[i][0];
      int p1 = cube_edges[i][1];
      tft.drawLine(px2[p0], py2[p0], px2[p1], py2[p1], 0xF81F);
    }

    // Uložení aktuálních souřadnic pro příští mazání
    for (int i = 0; i < 8; i++) {
      old_px1[i] = px1[i]; old_py1[i] = py1[i];
      old_px2[i] = px2[i]; old_py2[i] = py2[i];
    }

    // 5. UPDATE PROMĚNLIVÉHO TEXTU NA HUDu
    
    // Zobrazení úhlů uprostřed dole (převod na stupně)
    tft.fillRect(100, 222, 110, 10, ST77XX_BLACK);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(100, 222);
    tft.print("P:");
    tft.print((int)(ax1 * 57.2958f) % 360);
    tft.print(" Y:");
    tft.print((int)(ay1 * 57.2958f) % 360);
  }

  // Pomalá smyčka pro měření FPS (každou sekundu)
  if (currentMillis - fpsTimer >= 1000) {
    fpsValue = frameCount;
    frameCount = 0;
    fpsTimer = currentMillis;

    // Překreslení hodnoty FPS na HUDu bez blikání
    tft.fillRect(20, 222, 60, 10, ST77XX_BLACK);
    tft.setTextColor(ST77XX_GREEN);
    tft.setTextSize(1);
    tft.setCursor(20, 222);
    tft.print("FPS: ");
    tft.print(fpsValue);

    // Výpis diagnostiky na sériovou linku
    Serial.print("3D Engine running - FPS: ");
    Serial.print(fpsValue);
    Serial.print(" | Stars: ");
    Serial.print(NUM_STARS);
    Serial.print(" | Angles: P=");
    Serial.print((int)(ax1 * 57.2958f) % 360);
    Serial.print(" Y=");
    Serial.println((int)(ay1 * 57.2958f) % 360);
  }
}
