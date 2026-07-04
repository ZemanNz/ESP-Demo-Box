// LSM6DS3 6D Orientation (Detekce prostorové orientace) demo pro maturitu
#include <Arduino.h>
#include <Wire.h>

#define I2C_SDA 8
#define I2C_SCL 9

const uint8_t LsmAddr = 0x6A;

// Adresy vnitřních registrů LSM6DS3 podle datasheetu
#define LSM_WHOAMI      0x0F
#define LSM_CTRL1_XL    0x10
#define LSM_TAP_CFG     0x58
#define LSM_TAP_THS_6D  0x59
#define LSM_D6D_SRC     0x1D

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
  Serial.println("\n--- LSM6DS3 6D Orientace (Portrait/Landscape/Flat) ---");
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

  // 1. Nastavení akcelerometru: ODR = 104 Hz, rozsah +/- 2G
  writeRegister(LSM_CTRL1_XL, 0x40); // 0x40 = 104 Hz, 2G

  // 2. Aktivace hlavního přerušovacího engine
  // Zápis do TAP_CFG (0x58): bit 7 = 1 (povolit interrupt)
  writeRegister(LSM_TAP_CFG, 0x80);

  // 3. Konfigurace prahové hodnoty pro 6D detekci náklonu
  // Zápis do TAP_THS_6D (0x59): 
  // bit 7 = 0 (d4d_en = 6D orientace), bits 6..5 = 10 (sixd_ths: limit náklonu 60 stupňů) -> 0x40
  writeRegister(LSM_TAP_THS_6D, 0x40);

  Serial.println("6D orientace aktivována. Otáčejte senzorem v prostoru!");
  Serial.flush();
}

void loop() {
  // Přečteme stavový registr D6D_SRC (0x1D)
  uint8_t d6dSrc = readRegister(LSM_D6D_SRC);
  
  static uint8_t lastOrientation = 0xFF;
  
  // Zjistíme, který ze 6 směrů je aktivní (bit 0 až 5)
  uint8_t currentOrientation = d6dSrc & 0x3F;

  // Vypíšeme orientaci pouze při její změně, abychom nezahltili Serial Monitor
  if (currentOrientation != lastOrientation) {
    lastOrientation = currentOrientation;

    Serial.print("Nová poloha senzoru: ");
    
    if (d6dSrc & 0x20) {
      Serial.println("VODOROVNĚ - LÍCEM NAHORU (Z High / Flat Up)");
    } else if (d6dSrc & 0x10) {
      Serial.println("VODOROVNĚ - LÍCEM DOLŮ (Z Low / Flat Down)");
    } else if (d6dSrc & 0x08) {
      Serial.println("SVISLE - LEVÝ BOK (Y High)");
    } else if (d6dSrc & 0x04) {
      Serial.println("SVISLE - PRAVÝ BOK (Y Low)");
    } else if (d6dSrc & 0x02) {
      Serial.println("NA VÝŠKU - SPODNÍ HRANA NAHORU (X High)");
    } else if (d6dSrc & 0x01) {
      Serial.println("NA VÝŠKU - KLASICKY NAHORU (X Low / Portrait)");
    } else {
      Serial.println("Neznámá / přechodová poloha");
    }
    Serial.flush();
  }

  delay(100); // Kontrola 10krát za sekundu je více než dostatečná
}
