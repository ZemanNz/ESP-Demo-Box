#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_LSM6DS3.h>

// =============================================================================
// DEFINICE HARDWARE PINŮ (ESP32-S3 Horní Panel)
// =============================================================================
// I2C Sběrnice pro LSM6DS3 a LCD 1602
#define PIN_I2C_SDA          8
#define PIN_I2C_SCL          9

// Hardwarový pin přerušení z gyroskopu LSM6DS3 (INT1)
#define PIN_IMU_INT         45

// SPI Sběrnice pro 2.8" TFT ST7789
#define PIN_TFT_CS          10
#define PIN_TFT_MOSI        11
#define PIN_TFT_SCLK        12
#define PIN_TFT_DC          13
#define PIN_TFT_RST         14
#define PIN_TFT_MISO        -1

// Tlačítko na horním panelu (GPIO 47)
// Krátký stisk = změna citlivosti interruptu (1 až 6)
// Dlouhý stisk (> 1.2 s) = vynulování úhlů na 0.0°
#define PIN_BUTTON_RESET    47

// =============================================================================
// REGISTRY LSM6DS3 PRO HARDWAROVÝ WAKE-UP / SHOCK DETEKCI
// =============================================================================
#define LSM_WHOAMI          0x0F
#define LSM_CTRL1_XL        0x10
#define LSM_TAP_CFG         0x58
#define LSM_WAKE_UP_DUR     0x5C
#define LSM_WAKE_UP_THS     0x5B
#define LSM_MD1_CFG         0x5E
#define LSM_WAKE_UP_SRC     0x1B
#define LSM_TAP_SRC         0x1C

// =============================================================================
// INSTANCE ZAŘÍZENÍ A PROMĚNNÉ
// =============================================================================
Adafruit_LSM6DS3 lsm6ds3;
LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_ST7789 tft = Adafruit_ST7789(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

#ifndef RAD_TO_DEG
  #define RAD_TO_DEG 57.29577951308232f
#endif

uint8_t lsmAddr = 0x6B; // Zvolená I2C adresa LSM6DS3

// -----------------------------------------------------------------------------
// TABULKA ÚROVNÍ CITLIVOSTI INTERRUPTU (WAKE-UP / SHOCK THRESHOLD)
// -----------------------------------------------------------------------------
struct SensitivityLevel {
    uint8_t thsRegister; // Hodnota do WAKE_UP_THS (1 LSB ≈ 62.5 mg při +/-4G)
    const char* name;    // Krátký název pro LCD
    const char* desc;    // Popis pro TFT a Serial
    float approxMg;      // Přibližný práh v mg
};

const SensitivityLevel THRESHOLD_LEVELS[6] = {
    { 0x02, "1:EXTR", "1/6 Extra citlive (tuknuti prstem)",  125.0f },
    { 0x04, "2:JEMN", "2/6 Jemne (poklepani na kufr)",       250.0f },
    { 0x08, "3:STRE", "3/6 Stredni (drbnuti do stolu/kufru)", 500.0f },
    { 0x0E, "4:VYSS", "4/6 Vetsi drbnuti (postrceni)",       875.0f },
    { 0x14, "5:SILN", "5/6 Silny naraz (bouchnuti)",        1250.0f },
    { 0x20, "6:HRUB", "6/6 Hruby otras (pad / uder)",       2000.0f }
};

int currentSensLevel = 2; // Výchozí: Úroveň 3 (Střední, 500 mg)

// Stav přerušení a nárazu
volatile bool hwIntTriggered = false;
bool shockDetectedActive = false;
unsigned long shockTriggerTime = 0;
float shockPeakG = 0.0f;
char shockAxis = '-';
float allTimeMaxG = 1.0f;

// Proměnné pro integraci úhlů
float gyroBiasX = 0.0f;
float gyroBiasY = 0.0f;
float gyroBiasZ = 0.0f;
float accelInitAngleX = 0.0f;
float accelInitAngleZ = 0.0f;

float angleX = 0.0f; // Sklápění víka (Roll)
float angleY = 0.0f; // Otáčení kufru na stole (točna)
float angleZ = 0.0f; // Náklon do stran na bok (Pitch)

float lastGx = 0.0f, lastGy = 0.0f, lastGz = 0.0f;
unsigned long lastImuMicros = 0;
unsigned long lastTftUpdate = 0;
unsigned long lastLcdUpdate = 0;
unsigned long lastSerialUpdate = 0;

// Tlačítko
bool lastBtnState = HIGH;
unsigned long btnPressTime = 0;

bool hasLcd1602 = false;

// =============================================================================
// POMOCNÉ FUNKCE PRO I2C REGISTRY LSM6DS3
// =============================================================================
void writeLsmReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(lsmAddr);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

uint8_t readLsmReg(uint8_t reg) {
    Wire.beginTransmission(lsmAddr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(lsmAddr, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0;
}

// ISR obsluha hardwarového pinu INT1 (GPIO 45)
void IRAM_ATTR onImuHardwareInterrupt() {
    hwIntTriggered = true;
}

// Konfigurace registrů přerušení podle aktuální úrovně citlivosti
void applySensitivityLevel(int level) {
    if (level < 0) level = 0;
    if (level > 5) level = 5;
    currentSensLevel = level;

    uint8_t thsVal = THRESHOLD_LEVELS[level].thsRegister;

    // 1. Nastavení TAP_CFG: Bit 7 = INTERRUPTS_ENABLE, Bit 4 = SLOPE_FDS (High-pass filter pro rychlou detekci nárazu)
    writeLsmReg(LSM_TAP_CFG, 0x90);

    // 2. Doba trvání otřesu (0 = 1 ODR cyklus pro okamžitou bleskovou detekci)
    writeLsmReg(LSM_WAKE_UP_DUR, 0x00);

    // 3. Práh citlivosti nárazu (WAKE_UP_THS)
    writeLsmReg(LSM_WAKE_UP_THS, thsVal);

    // 4. Přesměrování signálu na hardwarový pin INT1 (Bit 5: INT1_WU = 1 -> 0x20)
    writeLsmReg(LSM_MD1_CFG, 0x20);

    Serial.printf("\n[LSM6DS3] Nastavena citlivost interruptu: %s (Reg 0x5B = 0x%02X, ~%.0f mg)\n",
                  THRESHOLD_LEVELS[level].desc, thsVal, THRESHOLD_LEVELS[level].approxMg);
}

// =============================================================================
// KALIBRACE GYROSKOPU A VÝCHOZÍHO GRAVITAČNÍHO NÁKLONU
// =============================================================================
void calibrateIMU() {
    Serial.println("\n=======================================================");
    Serial.println("  ZAHÁJENÍ KALIBRACE LSM6DS3 (Kufr v klidu!)");
    Serial.println("=======================================================");

    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, 30);
    tft.println("KALIBRACE GYROSKOPU");
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(20, 70);
    tft.println("Nehybejte kufrem...");
    tft.setCursor(20, 100);
    tft.println("Ukladam klidovou nulu...");

    if (hasLcd1602) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("KALIBRACE IMU...");
        lcd.setCursor(0, 1);
        lcd.print("NEHYBAT S KUFREM");
    }

    float sumGx = 0.0f, sumGy = 0.0f, sumGz = 0.0f;
    float sumAx = 0.0f, sumAy = 0.0f, sumAz = 0.0f;
    const int SAMPLES = 150;

    for (int i = 0; i < SAMPLES; i++) {
        sensors_event_t accel, gyro, temp;
        lsm6ds3.getEvent(&accel, &gyro, &temp);

        sumGx += gyro.gyro.x * (float)RAD_TO_DEG;
        sumGy += gyro.gyro.y * (float)RAD_TO_DEG;
        sumGz += gyro.gyro.z * (float)RAD_TO_DEG;

        sumAx += accel.acceleration.x / 9.80665f;
        sumAy += accel.acceleration.y / 9.80665f;
        sumAz += accel.acceleration.z / 9.80665f;

        int barW = (i * 260) / SAMPLES;
        tft.fillRect(30, 150, barW, 14, ST77XX_GREEN);
        tft.drawRect(28, 148, 264, 18, ST77XX_WHITE);
        delay(10);
    }

    gyroBiasX = sumGx / (float)SAMPLES;
    gyroBiasY = sumGy / (float)SAMPLES;
    gyroBiasZ = sumGz / (float)SAMPLES;

    float initAx = sumAx / (float)SAMPLES;
    float initAy = sumAy / (float)SAMPLES;
    float initAz = sumAz / (float)SAMPLES;

    // Počáteční gravitační sklon víka kufru
    accelInitAngleX = atan2f(initAz, -initAy) * (float)RAD_TO_DEG;
    accelInitAngleZ = atan2f(initAx, -initAy) * (float)RAD_TO_DEG;

    angleX = 0.0f; angleY = 0.0f; angleZ = 0.0f;
    lastGx = 0.0f; lastGy = 0.0f; lastGz = 0.0f;
    lastImuMicros = micros();

    // Aplikujeme výchozí citlivost interruptu
    applySensitivityLevel(currentSensLevel);

    // Vykreslíme základní masku na TFT displej
    tft.fillScreen(ST77XX_BLACK);
    tft.fillRect(0, 0, 320, 32, 0x18E3); // Tmavě modré záhlaví
    tft.drawRect(0, 0, 320, 240, ST77XX_WHITE);
    tft.drawLine(0, 32, 320, 32, ST77XX_WHITE);
    tft.setTextColor(ST77XX_WHITE, 0x18E3);
    tft.setTextSize(2);
    tft.setCursor(10, 8);
    tft.print("TEST GYROSKOPU + OTRES");

    // Popisky úhlů
    tft.setTextSize(2);
    tft.setTextColor(0xF800, ST77XX_BLACK); // Červená X
    tft.setCursor(12, 45);
    tft.print("X (Sklon vika):");

    tft.setTextColor(0x07E0, ST77XX_BLACK); // Zelená Y
    tft.setCursor(12, 75);
    tft.print("Y (Otoc. stul):");

    tft.setTextColor(0x001F, ST77XX_BLACK); // Modrá Z
    tft.setCursor(12, 105);
    tft.print("Z (Bocni hrana):");

    // Sekce detekce nárazu / drbnutí
    tft.drawLine(0, 135, 320, 135, ST77XX_WHITE);
    tft.fillRect(0, 136, 320, 26, 0x2104); // Tmavé pozadí pro status
    tft.setTextColor(ST77XX_WHITE, 0x2104);
    tft.setTextSize(2);
    tft.setCursor(10, 141);
    tft.print("OTRES / DRBNUTI:");

    tft.drawLine(0, 205, 320, 205, 0x7BEF);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setCursor(10, 212);
    tft.print("Tlacitko GPIO47: Kratce=Citlivost | Dlouze=Reset nuly");
    tft.setCursor(10, 226);
    tft.print("Serial: Zadej 1-6 nebo +/- pro zmenu prahu interruptu");

    if (hasLcd1602) {
        lcd.clear();
    }
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n=======================================================");
    Serial.println("  START: TEST GYROSKOPU + DETEKCE NÁRAZU (INTERRUPTU)");
    Serial.println("=======================================================");

    // 1. Tlačítko pro změnu citlivosti a reset nuly
    pinMode(PIN_BUTTON_RESET, INPUT_PULLUP);

    // 2. Hardwarový pin přerušení z LSM6DS3 (INT1 na GPIO 45)
    pinMode(PIN_IMU_INT, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(PIN_IMU_INT), onImuHardwareInterrupt, RISING);

    // 3. I2C sběrnice na 400 kHz (SDA: 8, SCL: 9)
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(400000);

    // 4. TFT 2.8" ST7789 displej
    SPI.begin(PIN_TFT_SCLK, PIN_TFT_MISO, PIN_TFT_MOSI, PIN_TFT_CS);
    tft.init(240, 320);
    tft.invertDisplay(false);
    tft.setRotation(1); // 320x240 na šířku
    tft.fillScreen(ST77XX_BLACK);

    // 5. Znakový LCD 1602
    Wire.beginTransmission(0x27);
    if (Wire.endTransmission() == 0) {
        lcd.init();
        lcd.backlight();
        hasLcd1602 = true;
        Serial.println("[LCD 1602] Detekovan na adrese 0x27");
    }

    // 6. LSM6DS3 inicializace
    bool lsmOk = false;
    if (lsm6ds3.begin_I2C(0x6B, &Wire)) {
        lsmOk = true;
        lsmAddr = 0x6B;
    } else if (lsm6ds3.begin_I2C(0x6A, &Wire)) {
        lsmOk = true;
        lsmAddr = 0x6A;
    }

    if (!lsmOk) {
        Serial.println("CHYBA: LSM6DS3 nenalezen!");
        tft.fillScreen(ST77XX_RED);
        tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
        tft.setTextSize(2);
        tft.setCursor(20, 80);
        tft.println("CHYBA: LSM6DS3 nenalezen!");
        while (1) delay(500);
    }

    lsm6ds3.setGyroRange(LSM6DS_GYRO_RANGE_500_DPS);
    lsm6ds3.setAccelRange(LSM6DS_ACCEL_RANGE_4_G);
    lsm6ds3.setGyroDataRate(LSM6DS_RATE_104_HZ);
    lsm6ds3.setAccelDataRate(LSM6DS_RATE_104_HZ);

    // 7. Spuštění kalibrace
    calibrateIMU();
}

// =============================================================================
// HLAVNÍ SMYČKA
// =============================================================================
void loop() {
    unsigned long now = millis();
    unsigned long nowUs = micros();

    // 1. Časový krok dt
    float dt = (float)(nowUs - lastImuMicros) / 1000000.0f;
    lastImuMicros = nowUs;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.01f;

    // 2. Vyčtení dat ze senzoru LSM6DS3
    sensors_event_t accel, gyro, temp;
    lsm6ds3.getEvent(&accel, &gyro, &temp);

    // 3. Úhlová rychlost v deg/s po odečtení biasu
    float gx = (gyro.gyro.x * (float)RAD_TO_DEG) - gyroBiasX;
    float gy = (gyro.gyro.y * (float)RAD_TO_DEG) - gyroBiasY;
    float gz = (gyro.gyro.z * (float)RAD_TO_DEG) - gyroBiasZ;

    // Klidové pásmo šumu
    const float STILL_THRESHOLD = 0.20f;
    if (fabsf(gx) < STILL_THRESHOLD) gx = 0.0f;
    if (fabsf(gy) < STILL_THRESHOLD) gy = 0.0f;
    if (fabsf(gz) < STILL_THRESHOLD) gz = 0.0f;

    // 4. Zrychlení v g
    float ax_g = accel.acceleration.x / 9.80665f;
    float ay_g = accel.acceleration.y / 9.80665f;
    float az_g = accel.acceleration.z / 9.80665f;
    float totalG = sqrtf(ax_g * ax_g + ay_g * ay_g + az_g * az_g);

    if (totalG > allTimeMaxG) {
        allTimeMaxG = totalG;
    }

    // 5. Komplementární filtr: spojení gyra s gravitační referencí pro svislý panel
    if (totalG > 0.70f && totalG < 1.30f) {
        float accelAngleX = (atan2f(az_g, -ay_g) * (float)RAD_TO_DEG) - accelInitAngleX;
        float accelAngleZ = (atan2f(ax_g, -ay_g) * (float)RAD_TO_DEG) - accelInitAngleZ;

        const float alpha = 0.98f;
        angleX = alpha * (angleX + 0.5f * (gx + lastGx) * dt) + (1.0f - alpha) * accelAngleX;
        angleZ = alpha * (angleZ + 0.5f * (gz + lastGz) * dt) + (1.0f - alpha) * accelAngleZ;
    } else {
        angleX += 0.5f * (gx + lastGx) * dt;
        angleZ += 0.5f * (gz + lastGz) * dt;
    }

    // Osa Y (rotace kolem svislé osy kufru) - čistá integrace
    angleY += 0.5f * (gy + lastGy) * dt;

    lastGx = gx; lastGy = gy; lastGz = gz;

    // -------------------------------------------------------------------------
    // 6. DETEKCE NÁRAZU / DRBNUTÍ PŘES INTERRUPT REGISTR A HARDWARE PIN
    // -------------------------------------------------------------------------
    uint8_t wuSrc = readLsmReg(LSM_WAKE_UP_SRC);
    bool swIntTriggered = (wuSrc & 0x08) != 0; // Bit 3: WU_IA událost
    bool pinIntActive = (digitalRead(PIN_IMU_INT) == HIGH);

    if (hwIntTriggered || swIntTriggered || pinIntActive) {
        hwIntTriggered = false;
        shockDetectedActive = true;
        shockTriggerTime = now;
        shockPeakG = totalG;

        if (wuSrc & 0x04) shockAxis = 'X';
        else if (wuSrc & 0x02) shockAxis = 'Y';
        else if (wuSrc & 0x01) shockAxis = 'Z';
        else shockAxis = '?';

        Serial.printf("\n>>> [DRBNUTI DETEKOVANO!] Sila: %.2f g | Osa: %c | INT1: %d | Prah: %s <<<\n",
                      totalG, shockAxis, pinIntActive ? 1 : 0, THRESHOLD_LEVELS[currentSensLevel].name);
    }

    // Pokud už od nárazu uplynula 1.2 sekundy, zhasneme indikátor
    if (shockDetectedActive && (now - shockTriggerTime >= 1200)) {
        shockDetectedActive = false;
    }

    // -------------------------------------------------------------------------
    // 7. OBSLUHA TLAČÍTKA NA PANELU (GPIO 47)
    // -------------------------------------------------------------------------
    bool btnNow = digitalRead(PIN_BUTTON_RESET);
    if (btnNow == LOW && lastBtnState == HIGH) {
        btnPressTime = now;
    }
    if (btnNow == HIGH && lastBtnState == LOW) {
        unsigned long dur = now - btnPressTime;
        if (dur >= 1200) {
            // Dlouhý stisk (> 1.2 s) -> Vynulování úhlů
            Serial.println("[RESET] Dlouhy stisk -> Nuluji uhely na 0.0°");
            sensors_event_t a2, g2, t2;
            lsm6ds3.getEvent(&a2, &g2, &t2);
            float a2x = a2.acceleration.x / 9.80665f;
            float a2y = a2.acceleration.y / 9.80665f;
            float a2z = a2.acceleration.z / 9.80665f;
            accelInitAngleX = atan2f(a2z, -a2y) * (float)RAD_TO_DEG;
            accelInitAngleZ = atan2f(a2x, -a2y) * (float)RAD_TO_DEG;
            angleX = 0.0f; angleY = 0.0f; angleZ = 0.0f;
            lastGx = 0.0f; lastGy = 0.0f; lastGz = 0.0f;
        } else if (dur >= 50) {
            // Krátký stisk -> Změna citlivosti interruptu na další úroveň (1 až 6)
            int nextLevel = (currentSensLevel + 1) % 6;
            applySensitivityLevel(nextLevel);
        }
    }
    lastBtnState = btnNow;

    // -------------------------------------------------------------------------
    // 8. OVLÁDÁNÍ CITLIVOSTI ZE SÉRIOVÉHO MONITORU (PC KLÁVESNICE)
    // -------------------------------------------------------------------------
    if (Serial.available()) {
        char ch = Serial.read();
        if (ch >= '1' && ch <= '6') {
            applySensitivityLevel(ch - '1');
        } else if (ch == '+' || ch == 'w') {
            applySensitivityLevel((currentSensLevel + 5) % 6); // Zvýšit citlivost
        } else if (ch == '-' || ch == 's') {
            applySensitivityLevel((currentSensLevel + 1) % 6); // Snížit citlivost
        } else if (ch == 'r' || ch == '0') {
            Serial.println("[RESET] Prikaz z PC -> Nuluji uhely!");
            angleX = 0.0f; angleY = 0.0f; angleZ = 0.0f;
        }
    }

    // -------------------------------------------------------------------------
    // 9. ZOBRAZENÍ NA TFT DISPLEJ (každých 50 ms = 20 fps)
    // -------------------------------------------------------------------------
    if (now - lastTftUpdate >= 50) {
        lastTftUpdate = now;

        // Vypíšeme živé úhly X, Y, Z
        tft.setTextSize(2);

        // Osa X
        tft.setTextColor(0xF800, ST77XX_BLACK);
        char bX[16]; snprintf(bX, sizeof(bX), "%+6.1f\xf7 ", angleX);
        tft.setCursor(205, 45); tft.print(bX);

        // Osa Y
        tft.setTextColor(0x07E0, ST77XX_BLACK);
        char bY[16]; snprintf(bY, sizeof(bY), "%+6.1f\xf7 ", angleY);
        tft.setCursor(205, 75); tft.print(bY);

        // Osa Z
        tft.setTextColor(0x001F, ST77XX_BLACK);
        char bZ[16]; snprintf(bZ, sizeof(bZ), "%+6.1f\xf7 ", angleZ);
        tft.setCursor(205, 105); tft.print(bZ);

        // Vizuální stav detekce drbnutí / interruptu
        if (shockDetectedActive) {
            // Zvýrazněný červený alarmový pruh
            tft.fillRect(0, 0, 320, 32, ST77XX_RED);
            tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
            tft.setTextSize(2);
            tft.setCursor(15, 8);
            tft.print("! DRBNUTI DETEKOVANO !");

            tft.fillRect(10, 168, 300, 30, 0x8000); // Tmavě červený obdélník
            tft.drawRect(8, 166, 304, 34, ST77XX_RED);
            tft.setTextColor(ST77XX_YELLOW, 0x8000);
            tft.setTextSize(2);
            tft.setCursor(18, 175);
            char bS[32]; snprintf(bS, sizeof(bS), "NARAZ: %.2fg  OSA:[%c]", shockPeakG, shockAxis);
            tft.print(bS);
        } else {
            // Běžné klidové zobrazení
            tft.fillRect(0, 0, 320, 32, 0x18E3);
            tft.setTextColor(ST77XX_WHITE, 0x18E3);
            tft.setTextSize(2);
            tft.setCursor(10, 8);
            tft.print("TEST GYROSKOPU + OTRES");

            tft.fillRect(10, 166, 304, 34, ST77XX_BLACK);
            tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
            tft.setTextSize(2);
            tft.setCursor(18, 175);
            char bK[32]; snprintf(bK, sizeof(bK), "KLID (Max:%.1fg) Prah:%d/6", allTimeMaxG, currentSensLevel + 1);
            tft.print(bK);
        }
    }

    // -------------------------------------------------------------------------
    // 10. ZOBRAZENÍ NA ZNAKOVÝ LCD 1602 (každých 150 ms)
    // -------------------------------------------------------------------------
    if (hasLcd1602 && (now - lastLcdUpdate >= 150)) {
        lastLcdUpdate = now;

        char l1[17];
        char l2[17];

        if (shockDetectedActive) {
            snprintf(l1, sizeof(l1), "!DRBNUTI!  %3.1fg", shockPeakG);
            snprintf(l2, sizeof(l2), "Osa:%c Prah:%s", shockAxis, THRESHOLD_LEVELS[currentSensLevel].name);
        } else {
            snprintf(l1, sizeof(l1), "STAV:KLID %3.1fg", totalG);
            snprintf(l2, sizeof(l2), "Prah:%s [INT:%d]", THRESHOLD_LEVELS[currentSensLevel].name, digitalRead(PIN_IMU_INT));
        }

        lcd.setCursor(0, 0);
        lcd.print(l1);
        lcd.setCursor(0, 1);
        lcd.print(l2);
    }

    // -------------------------------------------------------------------------
    // 11. SÉRIOVÝ MONITOR (každých 200 ms)
    // -------------------------------------------------------------------------
    if (now - lastSerialUpdate >= 200) {
        lastSerialUpdate = now;
        Serial.printf("Uhy: X=%+5.1f° Y=%+5.1f° Z=%+5.1f° | G=%.2fg (Max=%.2fg) | Prah: %s | INT1: %d | Otres: %s\n",
                      angleX, angleY, angleZ, totalG, allTimeMaxG,
                      THRESHOLD_LEVELS[currentSensLevel].name,
                      digitalRead(PIN_IMU_INT),
                      shockDetectedActive ? ">>> AKTIVNI! <<<" : "klid");
    }

    delay(8); // Takt ~104 Hz
}
