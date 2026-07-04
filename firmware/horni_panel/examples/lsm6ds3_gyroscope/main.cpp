#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_LSM6DS3.h>

// Definice I2C pinů pro ESP32-S3 (dle zapojení na hlavní sběrnici I2C0)
#define I2C_SDA 8
#define I2C_SCL 9

// Inicializace objektu pro senzor LSM6DS3
Adafruit_LSM6DS3 lsm6ds3;

// Globální proměnné pro integraci úhlu z gyroskopu (relativní natočení)
float angleX = 0.0;
float angleY = 0.0;
float angleZ = 0.0;

// Proměnné pro kalibraci (odstranění offsetu/biasu gyroskopu v klidu)
float gyroBiasX = 0.0;
float gyroBiasY = 0.0;
float gyroBiasZ = 0.0;

// Čas poslední aktualizace integrace v mikrosekundách
unsigned long lastUpdate = 0;

// Čas posledního výpisu na Serial Monitor (pro omezení zahlcení terminálu)
unsigned long lastPrint = 0;
const unsigned long printInterval = 1000; // Interval výpisu v milisekundách

// Prahová hodnota pro šum (deadzone) v rad/s
// Pokud je naměřená úhlová rychlost pod touto hodnotou, považujeme ji za 0 (minimalizuje drift)
const float GYRO_DEADZONE = 0.03;

// Přepočtový koeficient z radiánů na stupně (180 / PI)
const float RAD_TO_DEG_CONST = 57.295779513;

/**
 * Funkce provede kalibraci gyroskopu. 
 * Načte 200 vzorků v klidovém stavu a vypočítá průměrný šum/offset (bias),
 * který se pak v loop() odečítá od naměřených hodnot pro zamezení samovolného driftu úhlu.
 */
void calibrateGyroscope() {
  Serial.println("==================================================");
  Serial.println("SPUŠTĚNÍ KALIBRACE GYROSKOPU...");
  Serial.println("Uveďte senzor do klidu. Nehýbejte s ním po dobu 2 sekund.");
  Serial.println("==================================================");
  
  float sumX = 0;
  float sumY = 0;
  float sumZ = 0;
  const int numSamples = 200;

  for (int i = 0; i < numSamples; i++) {
    sensors_event_t accel, gyro, temp;
    lsm6ds3.getEvent(&accel, &gyro, &temp);
    
    sumX += gyro.gyro.x;
    sumY += gyro.gyro.y;
    sumZ += gyro.gyro.z;
    
    delay(10); // Krátká prodleva mezi měřeními (celkem 2 sekundy kalibrace)
  }

  // Výpočet průměrné odchylky (bias) pro každou osu
  gyroBiasX = sumX / numSamples;
  gyroBiasY = sumY / numSamples;
  gyroBiasZ = sumZ / numSamples;

  Serial.println("Kalibrace úspěšně dokončena!");
  Serial.print("Naměřený offset (rad/s) -> ");
  Serial.print("X: "); Serial.print(gyroBiasX, 4);
  Serial.print(" | Y: "); Serial.print(gyroBiasY, 4);
  Serial.print(" | Z: "); Serial.println(gyroBiasZ, 4);
  Serial.println("==================================================");
}

void setup() {
  // Inicializace sériového monitoru s požadovanou rychlostí 115200 baudů
  Serial.begin(115200);
  delay(1000); // Prodleva pro stabilizaci komunikace

  Serial.println("\n--- LSM6DS3 Testovací firmware pro maturitu ---");
  Serial.flush();

  // 1. Inicializace I2C sběrnice na pinech 8 (SDA) a 9 (SCL)
  Serial.print("Inicializace I2C sběrnice (SDA: GPIO ");
  Serial.print(I2C_SDA);
  Serial.print(", SCL: GPIO ");
  Serial.print(I2C_SCL);
  Serial.println(")...");
  Serial.flush();
  Wire.begin(I2C_SDA, I2C_SCL);

  // Spuštění integrovaného I2C skeneru pro diagnostiku zapojení
  Serial.println("Skenování I2C sběrnice...");
  Serial.flush();
  int devicesCount = 0;
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("  -> Nalezeno I2C zařízení na adrese: 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      Serial.flush();
      devicesCount++;
    }
  }
  if (devicesCount == 0) {
    Serial.println("  -> Žádné I2C zařízení nenalezeno na pinech 8 a 9!");
    Serial.flush();
  }

  // 2. Inicializace senzoru LSM6DS3 přes I2C s automatickou detekcí adresy (0x6A nebo 0x6B)
  Serial.println("Hledání senzoru LSM6DS3...");
  Serial.flush();
  
  bool initialized = false;
  // standardní adresa pro Adafruit a mnoho dalších modulů
  if (lsm6ds3.begin_I2C(0x6A)) {
    initialized = true;
    Serial.println("LSM6DS3 úspěšně nalezen and inicializován na adrese 0x6A!");
  } 
  // záložní adresa (pokud je SD0 pin připojen k VCC)
  else if (lsm6ds3.begin_I2C(0x6B)) {
    initialized = true;
    Serial.println("LSM6DS3 úspěšně nalezen and inicializován na adrese 0x6B!");
  }

  if (!initialized) {
    Serial.println("CHYBA: LSM6DS3 nebyl nalezen! Zkontrolujte zapojení SDA/SCL, napájení a zem.");
    Serial.flush();
    while (1) {
      delay(1000);
    }
  }
  Serial.flush();

  // Nastavení rozsahů senzoru pro optimální přesnost
  // Pro demonstraci zvolíme citlivost gyroskopu 250 dps (stupňů za sekundu)
  lsm6ds3.setGyroRange(LSM6DS_GYRO_RANGE_250_DPS);
  // Citlivost akcelerometru zvolíme +/- 2G (nejvyšší přesnost pro jemný pohyb)
  lsm6ds3.setAccelRange(LSM6DS_ACCEL_RANGE_2_G);

  // Zobrazení aktuálně nastavených rozsahů na Serial Monitor
  Serial.print("Rozsah gyroskopu nastaven na: ");
  switch (lsm6ds3.getGyroRange()) {
    case LSM6DS_GYRO_RANGE_125_DPS:  Serial.println("125 DPS"); break;
    case LSM6DS_GYRO_RANGE_250_DPS:  Serial.println("250 DPS"); break;
    case LSM6DS_GYRO_RANGE_500_DPS:  Serial.println("500 DPS"); break;
    case LSM6DS_GYRO_RANGE_1000_DPS: Serial.println("1000 DPS"); break;
    case LSM6DS_GYRO_RANGE_2000_DPS: Serial.println("2000 DPS"); break;
  }

  Serial.print("Rozsah akcelerometru nastaven na: ");
  switch (lsm6ds3.getAccelRange()) {
    case LSM6DS_ACCEL_RANGE_2_G:  Serial.println("+/- 2G"); break;
    case LSM6DS_ACCEL_RANGE_4_G:  Serial.println("+/- 4G"); break;
    case LSM6DS_ACCEL_RANGE_8_G:  Serial.println("+/- 8G"); break;
    case LSM6DS_ACCEL_RANGE_16_G: Serial.println("+/- 16G"); break;
  }

  // Spuštění kalibrace pro eliminaci klidového driftu gyroskopu
  calibrateGyroscope();

  // Inicializace časovačů
  lastUpdate = micros();
  lastPrint = millis();
}

void loop() {
  // Načtení aktuálního času v mikrosekundách pro přesný výpočet časové změny (dt)
  unsigned long nowMicros = micros();
  float dt = (float)(nowMicros - lastUpdate) / 1000000.0; // Přepočet na sekundy
  lastUpdate = nowMicros;

  // Bezpečnostní limit na dt (zabrání obrovskému skoku úhlu např. při startu nebo pozastavení procesoru)
  if (dt > 0.1) dt = 0.01;

  // Načtení dat ze senzoru LSM6DS3 (zrychlení, úhlová rychlost a teplota)
  sensors_event_t accel, gyro, temp;
  lsm6ds3.getEvent(&accel, &gyro, &temp);

  // 1. Odečtení nakalibrovaného klidového biasu (korekce systematické chyby)
  float rawGyroX = gyro.gyro.x - gyroBiasX;
  float rawGyroY = gyro.gyro.y - gyroBiasY;
  float rawGyroZ = gyro.gyro.z - gyroBiasZ;

  // 2. Mrtvé pásmo (deadzone) - potlačení integrace drobného šumu, když je senzor v klidu
  if (abs(rawGyroX) < GYRO_DEADZONE) rawGyroX = 0.0;
  if (abs(rawGyroY) < GYRO_DEADZONE) rawGyroY = 0.0;
  if (abs(rawGyroZ) < GYRO_DEADZONE) rawGyroZ = 0.0;

  // 3. Přepočet úhlové rychlosti z radiánů/s (výchozí pro knihovnu Adafruit) na stupně/s
  float gyroDegSecX = rawGyroX * RAD_TO_DEG_CONST;
  float gyroDegSecY = rawGyroY * RAD_TO_DEG_CONST;
  float gyroDegSecZ = rawGyroZ * RAD_TO_DEG_CONST;

  // 4. Výpočet absolutního úhlu náklonu z akcelerometru (podle vektoru gravitace)
  // Roll (otočení kolem osy X)
  float accelRoll = atan2(accel.acceleration.y, accel.acceleration.z) * RAD_TO_DEG_CONST;
  // Pitch (otočení kolem osy Y)
  float accelPitch = atan2(-accel.acceleration.x, sqrt(accel.acceleration.y * accel.acceleration.y + accel.acceleration.z * accel.acceleration.z)) * RAD_TO_DEG_CONST;

  // 5. Komplementární filtr (sloučení rychlé integrace gyra a stabilního úhlu z akcelerometru)
  // Koeficient alpha (0.98) dává 98% váhu gyroskopu (krátkodobá přesnost) a 2% akcelerometru (dlouhodobá stabilita/korekce driftu)
  const float alpha = 0.98;
  angleX = alpha * (angleX + gyroDegSecX * dt) + (1.0 - alpha) * accelRoll;
  angleY = alpha * (angleY + gyroDegSecY * dt) + (1.0 - alpha) * accelPitch;

  // Osu Z (Yaw) nelze korigovat samotným akcelerometrem (gravitační vektor je s ní rovnoběžný),
  // proto u ní zůstává čistá numerická integrace gyroskopu.
  angleZ += gyroDegSecZ * dt;

  // Výpis naměřených dat na Serial Monitor s časovým omezením
  unsigned long currentMillis = millis();
  if (currentMillis - lastPrint >= printInterval) {
    lastPrint = currentMillis;

    // Přehledný, inženýrsky formátovaný výstup
    Serial.println("\n=======================================================");
    Serial.println("  LSM6DS3 DATA MONITOR (ESP32-S3 via I2C0)");
    Serial.println("=======================================================");

    // Akcelerometr - zrychlení v m/s^2
    Serial.print("  Akcelerometr [m/s^2] | ");
    Serial.print("X: "); Serial.print(accel.acceleration.x, 2);
    Serial.print(" \tY: "); Serial.print(accel.acceleration.y, 2);
    Serial.print(" \tZ: "); Serial.println(accel.acceleration.z, 2);

    // Gyroskop - úhlová rychlost ve stupních/s (rotace)
    Serial.print("  Gyroskop     [st/s]  | ");
    Serial.print("X: "); Serial.print(gyroDegSecX, 1);
    Serial.print(" \tY: "); Serial.print(gyroDegSecY, 1);
    Serial.print(" \tZ: "); Serial.println(gyroDegSecZ, 1);

    // Integrovaný úhel - o kolik stupňů se senzor otočil od startu
    Serial.print("  Úhel otočení [stupně]| ");
    Serial.print("X: "); Serial.print(angleX, 1);
    Serial.print(" \tY: "); Serial.print(angleY, 1);
    Serial.print(" \tZ: "); Serial.println(angleZ, 1);

    // Teplota čipu
    Serial.print("  Teplota senzoru      | ");
    Serial.print(temp.temperature, 1);
    Serial.println(" °C");
    Serial.println("=======================================================");
  }
}
