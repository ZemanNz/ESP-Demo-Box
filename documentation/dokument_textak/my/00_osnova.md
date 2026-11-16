# Maturitní práce – ESP-Demo-Box

**Autor:** Nikola Zeman  
**Škola:** Střední průmyslová škola a Vyšší odborná škola Brno, Sokolská, příspěvková organizace  
**Obor:** Technické lyceum (78–42–M/01)  
**Vedoucí práce:** *[doplnit]*  
**Třída a školní rok:** *[doplnit]*

---

## Dohodnutá osnova práce (Varianta 1)

### Přední strany
- Titulní list
- Zadání práce
- Prohlášení a poděkování
- Abstrakt (CZ / EN) a klíčová slova
- Obsah

### 1 Úvod a cíle práce
- **1.1 Motivace a původ projektu:** Impuls od vyučujícího po vzoru prezentačního kufru z VUT FSI pro Dny otevřených dveří na SPŠ Sokolská; potřeba výukové platformy a dokumentace senzorů pro školní kroužek robotiky.
- **1.2 Stanovení cílů práce:** Mechanická modularita, elektrická bezpečnost a měření spotřeby, dvouprocesorová architektura, interaktivní režimy a hry s autopilotem, sada výukových příkladů (`examples/`).

### 2 Koncepce systému a výběr komponent
- **2.1 Architektura Master–Slave (kdo v kufru velí):** Rozdělení na horní panel (`ESP32-S3` – Master) a spodní panel (`ESP32-WROOM-32` – Slave), důvody rozdělení a hierarchie řízení.
- **2.2 Přehled senzorů, displejů a akčních členů:** Periferie horního a spodního panelu, tabulka rozpočtu a financování z projektu JMK.

### 3 Konstrukční řešení, 3D modelování a tisk
- **3.1 Volba CAD softwaru Onshape:** Práce v prohlížeči na OS Linux, bezplatná licence, cloud a online komunitní knihovna modelů součástek.
- **3.2 Modulární systém uchycení a ochranné prvky:** Spodní nosný rošt s otvory pro mosazné závitové vložky pro snadnou výměnu modulů, lineární posuv se šroubem T8 a koncovými spínači, ochranné průhledné plexisklo nad oběma ESP32.
- **3.3 3D tisk a realizace:** Tisk z materiálu PETG na tiskárně Bambu Lab A1 a osazení do hliníkového kufru.

### 4 Elektrické zapojení, schémata v KiCadu a energetická bilance
- **4.1 Návrh schématu v programu KiCad:** Popis 3 listů schématu (napájení, horní panel, spodní panel), dělič napětí pro HC-SR04 a oddělení sběrnice I2C.
- **4.2 Napájecí soustava, teorie a výpočet spotřeby:** Větev 12 V, panelový V/A metr DSN-VC288, měniče LM2596 (5 V a 6,5 V), teoretický výpočet odběru a srovnání s reálným měřením na kufru.
- **4.3 Řešení společného resetu obou desek:** Odstranění parazitního proudu mezi piny EN pomocí Schottkyho diod 1N5819.

### 5 Vývojové nástroje, architektura firmwaru a komunikace
- **5.1 Vývojové prostředí, verzování a spolupráce s AI:** PlatformIO, Google Antigravity, verzování pomocí Git/GitHub, otevřené vymezení role AI (grafika displeje a web) oproti vlastní práci.
- **5.2 Dokumentace senzorů a složka `examples/` pro kroužek robotiky:** Samostatné ukázkové programy s dokumentací pro výuku práce s periferiemi.
- **5.3 Architektura firmwaru a FreeRTOS:** Oddělení `SystemState` a `SensorManager`, rozdělení úloh na jádra a ochrana dat pomocí mutexů.
- **5.4 Binární UART protokol a synchronizace panelů:** Zarovnání `#pragma pack(push, 1)`, XOR checksum, hybridní přenos s Heartbeatem, startovní Handshake a řešení konfliktu autority u serv.

### 6 Provozní režimy, vlastní hry a webové rozhraní
- **6.1 Vykreslování GUI a eliminace problikávání (Double Buffering):** `TFT_eSPI` a `TFT_eSprite` v PSRAM.
- **6.2 Senzorové a demonstrační režimy:** Přehled interaktivních obrazovek (teplota/vlhkost, vzdálenost se 4 senzory, 3D gyroskop, barvy, světlo, serva, bzučák).
- **6.3 Vývoj her na notebooku a režim Autopilot:** Vlastnoruční vývoj her (Snake, Flappy Bird, 2048, Tetris) nejprve v PC simulátoru a implementace autopilota pro výstavy.
- **6.4 Webové rozhraní, Wi-Fi CSI a úsporný režim:** Web server v `PROGMEM` s WebSockety, detekce pohybu pomocí Wi-Fi CSI a automatický Light Sleep.

### 7 Závěr a zhodnocení
- Zhodnocení splnění cílů, reálné nasazení na Dnech otevřených dveří, přínos pro kroužek robotiky a budoucí rozšíření.

### Seznam použité literatury a přílohy
