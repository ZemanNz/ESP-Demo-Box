#include <Arduino.h>
#include <ESP32Servo.h>

// Definice GPIO pinu pro signálový vodič serva
#define SERVO_PIN 14

// Vytvoření instance serva
Servo myServo;

void setup() {
  // Inicializace sériového monitoru
  Serial.begin(115200);
  Serial.println("ESP-Demo-Box: Spodni panel - MG996R Kontinualni Servo na GPIO 13");

  // Alokace PWM časovačů pro ESP32
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // Nastavení frekvence PWM na 50 Hz
  myServo.setPeriodHertz(50);

  // Připojení serva k pinu GPIO 13 s nastavením šířky pulzu v mikrosekundách
  myServo.attach(SERVO_PIN, 1000, 2000);

  // Zastavení serva na začátku programu
  Serial.println("Inicializace: Zastavuji servo (hodnota 90)...");
  myServo.write(90);
  delay(2000); // 2 sekundy klidu
}

void loop() {
  // 1. Rotace vpřed
  Serial.println(">>> Rotace vpred (plna rychlost)...");
  myServo.write(180); // Plný rozsah vpřed
  delay(3000);

  // 2. Zastavení
  Serial.println(">>> Zastaveni serva...");
  myServo.write(90);  // Přesný střed zastaví rotaci
  delay(1500);

  // 3. Rotace vzad
  Serial.println(">>> Rotace vzad (plna rychlost)...");
  myServo.write(0);   // Plný rozsah vzad
  delay(3000);

  // 4. Zastavení
  Serial.println(">>> Zastaveni serva...");
  myServo.write(90);  // Přesný střed zastaví rotaci
  delay(1500);
}