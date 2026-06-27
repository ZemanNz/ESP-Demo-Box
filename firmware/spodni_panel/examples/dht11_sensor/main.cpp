#include <Arduino.h>
#include <DHT.h>

// Definice GPIO pinu pro připojení datového vodiče ze senzoru DHT11
#define DHTPIN 23

// Volba typu senzoru (v našem případě DHT11)
#define DHTTYPE DHT11

// Inicializace senzoru DHT
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  // Spuštění sériové komunikace na rychlosti 115200 baudů
  Serial.begin(115200);
  Serial.println("ESP-Demo-Box: Spodni panel - DHT11 Teplomer a Vlhkomer");

  // Spuštění komunikace se senzorem DHT11
  dht.begin();
}

void loop() {
  // Čtení vlhkosti vzduchu (v procentech)
  float humidity = dht.readHumidity();
  // Čtení teploty (ve stupních Celsia)
  float temperature = dht.readTemperature();

  // Kontrola, zda se data podařilo ze senzoru úspěšně načíst
  // Funkce isnan() vrací true, pokud hodnota není číslo (Is Not a Number)
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("CHYBA: Selhalo cteni dat ze senzoru DHT11! Zkontrolujte zapojeni.");
  } else {
    // Výpis naměřených hodnot na sériový monitor
    Serial.printf("Teplota: %.1f °C | Vlhkost: %.1f %%\n", temperature, humidity);
  }

  // DHT11 vyžaduje minimální prodlevu mezi měřeními 2 sekundy (je to pomalý senzor)
  delay(2000);
}
