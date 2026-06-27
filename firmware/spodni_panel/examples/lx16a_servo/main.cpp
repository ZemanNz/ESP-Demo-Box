#include <Arduino.h>
#include "SmartServoBus.hpp"

using namespace lx16a;

// Inicializace sběrnice pro servo
static SmartServoBus servoBus;

void setup() {
  // Spuštění sériového monitoru
  Serial.begin(115200);
  Serial.println("ESP-Demo-Box: Spodni panel - LX-16A Smart Servo priklad");

  // Inicializace sběrnice:
  // 1 - počet serv připojených na sběrnici (v tomto příkladu 1)
  // UART_NUM_2 - hardwarový UART2 na ESP32
  // GPIO_NUM_14 - GPIO pin 14 pro komunikaci
  servoBus.begin(1, UART_NUM_2, GPIO_NUM_14);

  // Krátká pauza pro stabilizaci sběrnice
  delay(1000);

  Serial.println("Dotazuji se na ID pripojeneho serva (pomocí broadcastu)...");

  // Získání aktuálního ID (254 je broadcast adresa, na kterou odpoví jediné připojené servo)
  uint8_t currentId = servoBus.getId();

  if (currentId == 255) {
    Serial.println("CHYBA: Zadny servo nebylo nalezeno! Zkontrolujte zapojeni a napajeni.");
  } else {
    Serial.printf("Uspesne nalezeno servo s ID: %d\n", currentId);

    // Pokud je ID nastaveno na 1 (nebo jiné než 0), změníme ho na 0
    if (currentId != 0) {
      Serial.printf("Menim ID serva z %d na ID 0...\n", currentId);
      
      // Nastavení nového ID (0) pro servo, které momentálně naslouchá na broadcastu (254)
      servoBus.setId(0);

      // Prodleva pro zápis do paměti serva
      delay(500);

      // Zpětné ověření nového ID
      uint8_t verifiedId = servoBus.getId();
      Serial.printf("Overeni: Nove ID serva je %d\n", verifiedId);

      if (verifiedId == 0) {
        Serial.println("Zmena ID na 0 probehla USPESNE!");
      } else {
        Serial.println("CHYBA: Overeni selhalo, servo stale hlasi jine ID.");
      }
    } else {
      Serial.println("Servo jiz ma nastavene ID 0.");
    }
  }
}

// Pomocná funkce pro přesun na cílový úhel a sledování pohybu
void moveToAngle(Angle targetAngle) {
  Serial.printf("\n>>> Prikaz k presunu na: %.1f stupnu\n", targetAngle.deg());
  servoBus.set(0, targetAngle);

  // Sledování pohybu po dobu 2 sekund (10 kroků po 200 ms)
  for (int i = 0; i < 10; i++) {
    delay(200);
    // Načtení aktuální polohy z čidla serva
    Angle currentAngle = servoBus.pos(0);
    if (!currentAngle.isNaN()) {
      Serial.printf("    Aktualni pozice: %.1f stupnu\n", currentAngle.deg());
    } else {
      Serial.println("    Aktualni pozice: neodpovida (offline)");
    }
  }
}

void loop() {
  // Postupný pohyb: 0 -> 120 -> 240 -> 120 -> opakování
  moveToAngle(0_deg);
  moveToAngle(120_deg);
  moveToAngle(240_deg);
  moveToAngle(120_deg);
}
