# Příklad: Chytré servo LX-16A (Sériová sběrnice)

Tento příklad demonstruje komunikaci s chytrým sériovým servomotorem **Lewansoul LX-16A** připojeným ke spodnímu panelu s ESP32. Kód nejprve zjistí aktuální ID serva (předpokládá se ID 1) a následně jej změní na vyžadované **ID 0**.

## Hardwarové zapojení

Sériová serva typu LX-16A komunikují po jednovodičové sběrnici (Half-Duplex Serial). Knihovna automaticky konfiguruje UART rozhraní ESP32 do jednovodičového režimu na vybraném pinu.

| LX-16A Konektor (Pin) | ESP32 DevKit Pin | Popis |
| --------------------- | ---------------- | ----- |
| **G** / **GND** (Zem)  | **GND**          | Společná zem |
| **V** / **VCC** (Napájení)| **Vnější zdroj** (6V - 7.4V) | Napájení serva (nepřipojovat přímo na 3.3V pin ESP32!) |
| **D** / **RX/TX** (Data)| **GPIO 14**      | Jednovodičová datová sběrnice |

*Upozornění: Serva LX-16A vyžadují napájecí napětí v rozsahu **6.0V až 7.4V** (ideálně 2S LiPo baterie nebo stabilizovaný síťový zdroj). Napájení přímo z USB přes ESP32 (5V) může fungovat pro čtení registrů/ID a pro pohyb naprázdno, ale při zátěži dojde k resetu ESP32 z důvodu proudového přetížení.*

---

## Použité knihovny
- **Esp32-lx16a** (https://github.com/RoboticsBrno/Esp32-lx16a)

Knihovna se do projektu instaluje přímo z GitHub repozitáře, nastavení je uvedeno v `platformio.ini`.

## Jak to funguje
1. Sběrnice serva se inicializuje v `setup()` pomocí `servoBus.begin(1, UART_NUM_2, GPIO_NUM_14)`. Parametry znamenají:
   - Počet serv: 1
   - Hardwarový UART: UART2
   - Datový pin: GPIO 14
2. Kód vyšle broadcast dotaz na ID `254` (adresa, na kterou odpoví jakékoliv jediné připojené servo).
3. Pokud servo odpoví, kód zkontroluje jeho aktuální ID.
4. Pokud je ID různé od `0` (např. tovární ID 1), kód pošle příkaz ke změně ID na `0` a změnu následně zpětně ověří.
5. V loopu program periodicky ověřuje přítomnost serva s ID 0 každých 5 sekund a vypisuje stav na sériový monitor (rychlost **115200 baudů**).
