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

void loop() {
  // Periodické ověřování přítomnosti serva každých 5 sekund
  uint8_t currentId = servoBus.getId();
  if (currentId != 255) {
    Serial.printf("[Loop] Pripojeno servo s ID: %d\n", currentId);
  } else {
    Serial.println("[Loop] Servo neodpovida.");
  }
  delay(5000);
}
