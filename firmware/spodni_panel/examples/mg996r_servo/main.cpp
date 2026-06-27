#include <Arduino.h>
#include <ESP32Servo.h>

// Definice GPIO pinu pro signálový vodič serva
#define SERVO_PIN 14

// Vytvoření instance serva
Servo myServo;

void setup() {
  // Inicializace sériového monitoru na rychlost 115200 baudů
  Serial.begin(115200);
  Serial.println("ESP-Demo-Box: Spodni panel - MG996R Kontinualni Servo");

  // Alokace PWM časovačů pro ESP32 (vyžadováno knihovnou ESP32Servo)
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // Nastavení frekvence PWM na 50 Hz (standard pro běžná modelářská serva)
  myServo.setPeriodHertz(50);

  // Připojení serva k pinu GPIO 14 s nastavením šířky pulzu v mikrosekundách:
  // - 1000 us odpovídá maximální rychlosti zpětného chodu (hodnota 0 v myServo.write)
  // - 2000 us odpovídá maximální rychlosti dopředného chodu (hodnota 180 v myServo.write)
  // - Střed (cca 1500 us) odpovídá zastavení serva (hodnota 90 v myServo.write)
  myServo.attach(SERVO_PIN, 1000, 2000);

  // Zastavení serva na začátku programu
  Serial.println("Inicializace: Zastavuji servo (hodnota 90)...");
  myServo.write(90);
  delay(3000); // 3 sekundy klidu
}

void loop() {
  // 1. Pomalejší rotace vpřed
  Serial.println(">>> Rotace vpred (pomaly chod)...");
  myServo.write(100); // Hodnota kousek nad 90 (střed)
  delay(2500);

  // 2. Maximální rychlost vpřed
  Serial.println(">>> Rotace vpred (plna rychlost)...");
  myServo.write(180); // Plný rozsah vpravo / vpřed
  delay(2500);

  // 3. Zastavení
  Serial.println(">>> Zastaveni serva...");
  myServo.write(90);  // Přesný střed zastaví rotaci
  delay(2000);

  // 4. Pomalejší rotace vzad
  Serial.println(">>> Rotace vzad (pomaly chod)...");
  myServo.write(80);  // Hodnota kousek pod 90 (střed)
  delay(2500);

  // 5. Maximální rychlost vzad
  Serial.println(">>> Rotace vzad (plna rychlost)...");
  myServo.write(0);   // Plný rozsah vlevo / vzad
  delay(2500);

  // 6. Zastavení a pauza před novým cyklem
  Serial.println(">>> Zastaveni serva a cekani na dalsi cyklus...");
  myServo.write(90);
  delay(5000);
}
