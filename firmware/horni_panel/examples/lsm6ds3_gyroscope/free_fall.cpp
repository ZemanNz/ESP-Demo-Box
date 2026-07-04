// LSM6DS3 Hardware Free-Fall Detection (Detekce volného pádu) demo pro maturitu
#include <Arduino.h>
#include <Wire.h>

#define I2C_SDA 8
#define I2C_SCL 9

const uint8_t LsmAddr = 0x6A;

// Adresy vnitřních registrů LSM6DS3 podle datasheetu
#define LSM_WHOAMI      0x0F
#define LSM_CTRL1_XL    0x10
#define LSM_TAP_CFG     0x58
#define LSM_WAKE_UP_DUR 0x5C
#define LSM_FREE_FALL   0x5D
#define LSM_MD1_CFG     0x5E
#define LSM_WAKE_UP_SRC 0x1B

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
  Serial.println("\n--- LSM6DS3 Detekce volného pádu (Free-Fall) ---");
  Serial.flush();

  Wire.begin(I2C_SDA, I2C_SCL);

  uint8_t whoami = readRegister(LSM_WHOAMI);
  if (whoami != 0x69) {
    Serial.print("CHYBA: Senzor neodpovídá na adrese 0x6A! WHOAMI: 0x");
    Serial.println(whoami, HEX);
    Serial.flush();
    while (1) delay(1000);
  }
  Serial.println("LSM6DS3 komunikuje správně.");

  // 1. Nastavení akcelerometru: ODR = 416 Hz, rozsah +/- 2G (vyžadováno pro detekci volného pádu)
  writeRegister(LSM_CTRL1_XL, 0x60); // 0x60 = 416 Hz, 2G

  // 2. Aktivace hlavního přerušovacího engine
  // Zápis do TAP_CFG (0x58): bit 7 = 1 (povolit interrupt)
  writeRegister(LSM_TAP_CFG, 0x80);

  // 3. Nastavení parametrů volného pádu: práh a doba trvání
  // Zápis do WAKE_UP_DUR (0x5C): nastavit bit 7 na 0 (nejvýznamnější bit doby pádu)
  uint8_t wakeUpDur = readRegister(LSM_WAKE_UP_DUR);
  writeRegister(LSM_WAKE_UP_DUR, wakeUpDur & 0x7F);

  // Zápis do FREE_FALL (0x5D):
  // bits 7..3 = 00110 (Práh: ~150 mg, velmi citlivý)
  // bits 2..0 = 011 (Doba trvání: ~30 ms)
  writeRegister(LSM_FREE_FALL, 0x33);

  // 4. Přesměrování přerušení volného pádu na hardwarový pin INT1
  // Zápis do MD1_CFG (0x5E): bit 4 = 1 (int1_ff - volný pád) -> 0x10
  writeRegister(LSM_MD1_CFG, 0x10);

  Serial.println("Detekce volného pádu byla aktivována. Vyzkoušejte pád senzoru!");
  Serial.flush();
}

void loop() {
  // Čtení registru WAKE_UP_SRC (0x1B)
  uint8_t wakeUpSrc = readRegister(LSM_WAKE_UP_SRC);

  // Bit 5 (ff_ia) je nastaven na 1, pokud došlo k detekci stavu bez tíže (pádu)
  if (wakeUpSrc & 0x20) {
    Serial.println("!!! DETEKOVÁN VOLNÝ PÁD (FREE-FALL) !!!");
    Serial.flush();
    
    // Delší prodleva pro uklidnění po nárazu
    delay(1000);
  }

  delay(10); // Polling loop
}
