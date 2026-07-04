// LSM6DS3 Hardware Pedometer (Krokoměr) demo pro maturitu
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_LSM6DS3.h>

#define I2C_SDA 8
#define I2C_SCL 9

Adafruit_LSM6DS3 lsm6ds3;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- LSM6DS3 Hardware Krokoměr Demo ---");
  Serial.flush();

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!lsm6ds3.begin_I2C(0x6A) && !lsm6ds3.begin_I2C(0x6B)) {
    Serial.println("CHYBA: LSM6DS3 nebyl nalezen! Zkontrolujte I2C zapojení.");
    Serial.flush();
    while (1) delay(1000);
  }
  Serial.println("LSM6DS3 úspěšně inicializován.");

  // Nastavení rozsahů a datového toku pro krokoměr (26 Hz je ideální pro detekci kroků a šetří energii)
  lsm6ds3.setAccelRange(LSM6DS_ACCEL_RANGE_2_G);
  lsm6ds3.setAccelDataRate(LSM6DS_RATE_26_HZ);

  // Volitelně můžeme přesměrovat signál detekce kroku na hardwarový pin INT1
  lsm6ds3.configInt1(false, false, false, true); // (drdy_temp, drdy_g, drdy_xl, step_detect)

  // Zapnutí hardwarového krokoměru
  Serial.println("Zapínám integrovaný krokoměr...");
  lsm6ds3.enablePedometer(true);
  Serial.flush();
}

void loop() {
  // Přečteme počet kroků přímo z vnitřního registru senzoru
  uint16_t steps = lsm6ds3.readPedometer();
  
  Serial.print("Počet kroků: ");
  Serial.println(steps);
  Serial.flush();
  
  delay(1000); // Aktualizace jednou za sekundu
}
