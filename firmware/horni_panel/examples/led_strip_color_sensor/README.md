# Příklad: Ovládání LED pásku barevným senzorem TCS34725

Tento příklad demonstruje propojení barevného senzoru **TCS34725** a adresovatelného LED pásku (**8x WS2812B / NeoPixel**) na spodním panelu s ESP32. LED pásek svítí barvou, kterou senzor aktuálně detekuje.

## Hardwarové zapojení

### I2C Sběrnice pro Barevný Senzor (TCS34725)
Na klasické desce ESP32 DevKit jsou výchozí piny pro hardwarové rozhraní I2C (Wire) definovány následovně:
- **GPIO 21** = **SDA** (Serial Data)
- **GPIO 22** = **SCL** (Serial Clock)

| Senzor TCS34725 Pin | ESP32 DevKit Pin | Popis |
| ------------------ | ---------------- | ----- |
| **SDA**            | **GPIO 21**      | I2C datový pin |
| **SCL**            | **GPIO 22**      | I2C hodinový pin |
| **VIN** / **3V3**  | **3V3**          | Napájení senzoru (3.3V) |
| **GND**            | **GND**          | Společná zem |

*Poznámka: Senzor TCS34725 má integrovanou LED diodu pro osvětlení snímaného objektu. Ta se obvykle rozsvítí automaticky při zapojení napájení (pokud pin LED na senzoru není uzemněn).*

### LED pásek (8 diod)
| LED pásek Pin | ESP32 DevKit Pin | Popis |
| ------------- | ---------------- | ----- |
| **DI** (Data) | **GPIO 19**       | Vstup pro datový signál |
| **5V** / **VCC**| **5V** / **VIN** | Napájení LED pásku (5V) |
| **GND**       | **GND**          | Společná zem |

---

## Použité knihovny
- **Adafruit NeoPixel**
- **Adafruit TCS34725**

Obě knihovny jsou spravovány nástrojem PlatformIO a jsou definovány v `platformio.ini` projektu.

## Jak to funguje
1. Po spuštění kód inicializuje I2C sběrnici na pinech **SDA=21** a **SCL=22** pomocí `Wire.begin(21, 22)`.
2. Inicializuje se LED pásek a barevný senzor. Pokud senzor není detekován na I2C sběrnici, celý LED pásek se rozsvítí **červeně** a kód se zastaví.
3. V každém cyklu (každých 100 ms) se načtou raw barevná data (červená `R`, zelená `G`, modrá `B` a celková intenzita světla `C`).
4. Barevné složky jsou normalizovány podle celkového jasu (kanálu `Clear`), což zamezí tomu, aby se barva měnila jen na základě vzdálenosti objektu od senzoru.
5. Přepočtené RGB hodnoty (0-255) jsou nastaveny na LED pásek, který se tak rozsvítí barvou snímanou senzorem. Výsledné hodnoty jsou zároveň odesílány do sériové linky.
