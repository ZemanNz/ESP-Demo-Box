// LSM6DS3 Hardware Single/Double Tap Detection (Detekce poklepání) demo pro maturitu
#include <Arduino.h>
#include <Wire.h>

#define I2C_SDA 8
#define I2C_SCL 9

// Zde definujeme I2C adresu (zkusíme nejprve standardní 0x6A)
const uint8_t LsmAddr = 0x6A;

// Adresy vnitřních registrů LSM6DS3 podle datasheetu
#define LSM_WHOAMI      0x0F
#define LSM_CTRL1_XL    0x10
#define LSM_TAP_CFG     0x58
#define LSM_TAP_THS_6D  0x59
#define LSM_INT_DUR2    0x5A
#define LSM_WAKEUP_THS  0x5B
#define LSM_MD1_CFG     0x5E
#define LSM_TAP_SRC     0x1C

// Pomocné funkce pro přímý zápis a čtení registrů přes I2C sběrnici
void writeRegister(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(LsmAddr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t readRegister(uint8_t reg) {
  Wire.beginTransmission(LsmAddr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(LsmAddr, (uint8_t)1);
  return Wire.read();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- LSM6DS3 Detekce poklepání (Tap Detection) ---");
  Serial.flush();

  Wire.begin(I2C_SDA, I2C_SCL);

  // Ověření komunikace pomocí registru WHO_AM_I (LSM6DS3 vrací 0x69)
  uint8_t whoami = readRegister(LSM_WHOAMI);
  if (whoami != 0x69) {
    Serial.print("CHYBA: Senzor neodpovídá na adrese 0x6A! WHOAMI vrátil: 0x");
    Serial.println(whoami, HEX);
    Serial.flush();
    while (1) delay(1000);
  }
  Serial.println("LSM6DS3 komunikuje správně.");

  // 1. Nastavení akcelerometru: ODR = 416 Hz (vysoká rychlost pro spolehlivou detekci), rozsah +/- 2G
  writeRegister(LSM_CTRL1_XL, 0x60); // 0x60 = 416 Hz, 2G

  // 2. Zapnutí detekce poklepání v osách X, Y, Z a zapnutí hlavního přerušení
  // Zápis do TAP_CFG (0x58): bit7 = 1 (povolit interrupt), bits 4..2 = 111 (povolit X, Y, Z osy)
  writeRegister(LSM_TAP_CFG, 0x8E);

  // 3. Nastavení prahové hodnoty pro poklepání (tap threshold)
  // Zápis do TAP_THS_6D (0x59): bit 4..0 = 01001 (práh pro poklepání - citlivost, doporučeno 0x09)
  writeRegister(LSM_TAP_THS_6D, 0x09);

  // 4. Nastavení časových limitů pro otřes (Shock), zklidnění (Quiet) a odezvu dvojkliku (Latency)
  // Zápis do INT_DUR2 (0x5A): bits 7..4 = 0111 (Shock), 3..2 = 01 (Quiet), 1..0 = 11 (Latency)
  writeRegister(LSM_INT_DUR2, 0x7F);

  // 5. Povolení detekce dvojitého poklepání (Double Tap)
  // Zápis do WAKEUP_THS (0x5B): bit 7 = 1 (povolit Double Tap), ostatní bity = 0
  writeRegister(LSM_WAKEUP_THS, 0x80);

  // 6. Volitelně přesměrovat detekci na fyzický pin INT1
  // Zápis do MD1_CFG (0x5E): bit 6 = 1 (Single Tap), bit 3 = 1 (Double Tap) -> 0x48
  writeRegister(LSM_MD1_CFG, 0x48);

  Serial.println("Detekce poklepání byla aktivována. Poklepejte na senzor!");
  Serial.flush();
}

void loop() {
  // Přečteme registr TAP_SRC (0x1C), který uchovává informace o detekovaném poklepání
  uint8_t tapSrc = readRegister(LSM_TAP_SRC);

  // Bit 6 (tap_ia) je nastaven na 1, pokud došlo k jakémukoliv poklepání
  if (tapSrc & 0x40) {
    bool isDouble = (tapSrc & 0x10) != 0; // Bit 4 je nastaven při Double Tap
    bool isSingle = (tapSrc & 0x20) != 0; // Bit 5 je nastaven při Single Tap

    Serial.print("--- DETEKCE POKLEPÁNÍ --- ");
    if (isDouble) {
      Serial.print("DVOJITÉ POKLEPÁNÍ");
    } else if (isSingle) {
      Serial.print("JEDNODUCHÉ POKLEPÁNÍ");
    }

    // Určení osy poklepání
    if (tapSrc & 0x04) Serial.print(" v ose X");
    if (tapSrc & 0x02) Serial.print(" v ose Y");
    if (tapSrc & 0x01) Serial.print(" v ose Z");

    Serial.println("!");
    Serial.flush();
    
    // Krátká prodleva pro zamezení opakovaných výpisů z jednoho klepnutí
    delay(250);
  }

  delay(10); // Polling loop
}
