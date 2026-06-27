# Příklad: Adresovatelný LED pásek (RGB barevný přechod)

Tento příklad demonstruje ovládání adresovatelného LED pásku (8x WS2812B / NeoPixel) připojeného ke spodnímu panelu s ESP32. Příklad vytváří plynulý efekt tekoucí duhy (RGB barevný přechod) běžící dokola.

## Hardwarové zapojení

| LED pásek Pin | ESP32 DevKit Pin | Popis |
| ------------- | ---------------- | ----- |
| **DI** (Data) | **GPIO 19**       | Vstup pro datový signál |
| **5V** / **VCC**| **5V** / **VIN** | Napájení (5V z USB) |
| **GND**       | **GND**          | Společná zem |

*Upozornění: Pro delší LED pásky (s velkým počtem diod) je nutné použít externí 5V napájecí zdroj, aby nedošlo k přetížení USB portu nebo stabilizátoru na ESP32. Pro 8 diod s nastaveným jasem na hodnotu 50/255 plně postačuje napájení přímo z ESP32 DevKit.*

## Použité knihovny
- **Adafruit NeoPixel** (instaluje se automaticky přes PlatformIO podle nastavení v `platformio.ini`)

## Popis kódu
Kód využívá knihovnu `Adafruit_NeoPixel`. Ve funkci `setup()` se inicializuje komunikace a nastaví jas (brightness) na rozumnou hodnotu 50 (z maximálních 255) z důvodu ochrany zraku a šetření proudu.

V hlavní smyčce `loop()` se pomocí pomocné funkce `Wheel()` generuje barva z celého RGB spektra na základě pozice diody a času (proměnná `j`). Tím vzniká plynule tekoucí barevný přechod přes všech 8 diod.
