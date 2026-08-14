# Příklad: 0.96" OLED Displej (SSD1306, I2C 128x64 px)

Tento příklad demonstruje připojení a grafické ovládaní monohromatického 0.96" OLED displeje s integrovaným řadičem **SSD1306** přes I2C sběrnici ke spodní desce ESP32-WROOM.

---

## Hardwarové zapojení

| Pin na OLED Displeji | Funkce | ESP32 DevKit Pin | Poznámka |
| :--- | :--- | :--- | :--- |
| **GND** | Zem | **GND** | Společná zem |
| **VCC** | Napájení | **3.3V** | Napájení 3.3V |
| **SDA** | Sériová data I2C | **GPIO 21** | Datová linka I2C sběrnice |
| **SCL** | Hodiny I2C | **GPIO 22** | Hodinová linka I2C sběrnice |

---

## Použité knihovny
- **Adafruit SSD1306** (`adafruit/Adafruit SSD1306`)
- **Adafruit GFX Library** (`adafruit/Adafruit GFX Library`)
- **Adafruit BusIO** (`adafruit/Adafruit BusIO`)

Všechny knihovny se automaticky spravují a stahují pomocí PlatformIO.

---

## Jak to funguje
1. **Inicializace I2C**: V `setup()` se inicializuje sběrnice `Wire.begin(21, 22)` na výchozích pinech spodního panelu.
2. **Bufferovaný vykreslovací režim**: Knihovna Adafruit GFX udržuje celý obrazový rámec v paměti RAM mikrokontroléru (128 × 64 bitů = 1 KB RAM). Všechny kreslicí příkazy jako `drawFastHLine()`, `drawRect()`, `fillRect()`, `setCursor()` a `print()` kreslí do tohoto vnitřního vyrovnávacího bufferu.
3. **Odeslání na displej**: Zavoláním příkazu `display.display()` se celý buffer najednou přenese přes I2C do řadiče SSD1306, což zajišťuje plynulý obraz bez blikání.
