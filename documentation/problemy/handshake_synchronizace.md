# Problém: Synchronizace bootování dvou nezávislých mikrokontrolérů

## Popis situace
Architektura výukového kufru je fyzicky rozdělena mezi dva nezávislé čipy:
1. **Spodní panel (ESP32-WROOM-32D)** – Zpracovává pouze lokální periferie a bootuje velmi rychle (v řádu milisekund).
2. **Horní panel (ESP32-S3)** – Působí jako Master (hlavní mozek). Obsluhuje Wi-Fi připojení, inicializuje TFT displej a senzory, což způsobuje, že jeho bootovací sekvence je nepoměrně delší (v řádu sekund).

Pokud by spodní panel okamžitě po startu začal odesílat data nebo vykonávat řídicí povely, hrozila by desynchronizace, ztráta dat, a v nejhorším případě neřízený a nebezpečný pohyb připojených servomotorů.

## Architektonické řešení: Boot Handshake
Tento problém je vyřešen implementací synchronizační sekvence zvané "Handshake" (potřesení rukou) realizované přes obousměrnou sběrnici UART.

1. **Fáze čekání (Slave):** Spodní ESP32 ihned po rychlém nabootování zablokuje veškeré výkonné hardwarové periferie (motory) a přejde do pasivního režimu. V pravidelných intervalech (např. 500 ms) odesílá na UART sběrnici signál `[SYS_READY]`.
2. **Fáze potvrzení (Master):** Horní ESP32-S3 po dokončení své zdlouhavé inicializace (připojení Wi-Fi, grafika) záměrně vyprázdní sériový buffer, čímž zahodí případný šum z bootování, a přejde do režimu poslechu. Jakmile zachytí signál `[SYS_READY]`, odesílá zpět potvrzovací povel `[START_WORK]`.
3. **Fáze exekuce:** Po přijetí povelu `[START_WORK]` oba mikrokontroléry paralelně a plně synchronizovaně odblokují své procesy a zahájí hlavní programové smyčky. 

Tímto mechanismem je garantováno, že distribuovaný systém vždy startuje jako jeden ucelený a bezpečný stroj bez ohledu na to, jak dlouho trvá připojení k Wi-Fi síti.
