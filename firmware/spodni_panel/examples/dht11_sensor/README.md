# Příklad: Senzor teploty a vlhkosti DHT11

Tento příklad ukazuje, jak číst teplotu a vlhkost vzduchu ze senzoru **DHT11** na spodním panelu s ESP32 a naměřené hodnoty vypisovat na sériový monitor.

## Hardwarové zapojení

Standardní senzor DHT11 má **4 piny**. Při pohledu na mřížku senzoru zepředu (piny směřují dolů) jsou piny číslovány zleva doprava (1 až 4):

| DHT11 Pin | Popis | ESP32 DevKit Pin | Poznámka |
| --------- | ----- | ---------------- | -------- |
| **1**     | **VCC** (Napájení) | **3V3** nebo **5V** | Napájení senzoru |
| **2**     | **DATA** (Signál)  | **GPIO 23**       | Datový pin (vyžaduje pull-up rezistor, viz níže) |
| **3**     | **NC**             | Nezapojuje se     | Neobsazeno (Not Connected) |
| **4**     | **GND** (Zem)      | **GND**           | Společná zem |

### DŮLEŽITÉ UPOZORNĚNÍ: Pull-up Rezistor
* **Samostatné čidlo (4 piny)**: Mezi pin **VCC (1)** a pin **DATA (2)** je nutné připojit **pull-up rezistor o hodnotě 4.7kΩ až 10kΩ** (viz schéma zapojení DHT11). Bez tohoto rezistoru nebude komunikace fungovat a program bude hlásit chybu čtení.
* **Modul na PCB (3 piny)**: Pokud máte DHT11 zakoupené jako hotový modul na malé destičce (která má typicky jen 3 piny: `+`/`VCC`, `Out`/`Data`, `-`/`GND`), pak je pull-up rezistor již integrován přímo na destičce a dodatečný rezistor není potřeba.

---

## Použité knihovny
- **DHT sensor library** (Adafruit)
- **Adafruit Unified Sensor** (stahuje se automaticky jako závislost)

Obě knihovny jsou spravovány PlatformIO a jsou definovány v `platformio.ini` projektu.

## Jak to funguje
1. Senzor se inicializuje v `setup()` pomocí `dht.begin()`.
2. V loopu se každé 2 sekundy načte vlhkost vzduchu (`dht.readHumidity()`) a teplota (`dht.readTemperature()`).
3. Pomocí `isnan()` kontrolujeme, zda data přišla v pořádku (např. zda nejsou odpojené dráty nebo zda nechybí pull-up rezistor).
4. Pokud je vše v pořádku, vypíší se naměřené hodnoty do sériové linky (rychlost **115200 baudů**).
