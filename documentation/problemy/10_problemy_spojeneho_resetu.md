# Problémy spojeného resetu dvou mikrokontrolérů ESP32 a jejich řešení

## 1. Popis problému a výchozí stav
V projektu **ESP-Demo-Box** jsou použity dva samostatné mikrokontroléry:
* **Horní panel:** ESP32-S3 (hlavní řídicí jednotka, TFT displej, senzory, Wi-Fi webserver).
* **Dolní panel:** ESP32-WROOM-32 (vstupy z tlačítek, joysticku, servomotory, OLED displej).

Cílem bylo umožnit uživateli provést **současný restart obou mikrokontrolérů stiskem jediného fyzického tlačítka RST**. 

Původní zapojení bylo realizováno přímým spojením signálů:
* Tlačítko RST bylo zapojeno tak, že při stisku spojovalo signál se zemí (`GND`).
* Od tlačítka vedl vodič přímo na pin `EN / RST` horního ESP32 a zároveň pokračoval spojovacím kabelem dolů na pin `EN` spodního ESP32.

```
       [+3.3V Horní]              [+3.3V Spodní]
             |                          |
          [10k R1]                   [10k R2]
             |                          |
Pin EN (Horní ESP) -------------------- Pin EN (Spodní ESP)
                         |
                 Tlačítko RST
                     --/ o--
                         |
                       [GND]
```

### Pozorované chování:
1. **Při napájení celého kufru ze step-down měničů:** Systém zdánlivě fungoval, protože obě desky dostávaly napájení současně z jednoho společného zdroje.
2. **Při napájení z USB kabelu / nabíječky na mobil:**
   * Pokud byl napájecí adaptér nebo USB kabel připojen pouze k jedné desce (nebo byla jedna deska zapojena dříve než druhá), **program na obou panelech vůbec nenaběhl**.
   * Na dolním panelu se nerozsvítil malý OLED displej.
   * Na horním panelu sice svítilo podsvícení TFT (napájené z 3.3V lišty), ale displej zůstal prázdný a nevypisoval ani text `BOOT: OK`.
   * Při připojení k PC sériový monitor neukazoval vůbec žádná data, a to ani základní hlášku bootloaderu ROM při stisku resetu.

---

## 2. Diagnostika a technická příčina

### A) Naměřená hodnota na pinu EN
Měřením multimetrem na pinu `EN` spodního panelu při napájení adaptérem bylo naměřeno napětí pouhých **0,8 V**.

Podle technické dokumentace (Datasheet Espressif ESP32 a ESP32-S3):
* Minimální logická úroveň `HIGH` (kdy je procesor spuštěn a běží): $V_{IH} \ge 0,75 \times V_{DD} \approx 2,475\text{ V}$.
* Logická úroveň `LOW` (hardwarový reset / procesor vypnut): $V_{IL} \le 0,25 \times V_{DD} \approx 0,825\text{ V}$.

Naměřených **0,8 V** drželo interní obvody obou mikrokontrolérů v **permanentním hardwarovém resetu**. Čip byl vypnutý a neprováděl žádné instrukce.

### B) Vznik parazitního děliče napětí
Každá vývojová deska ESP32 obsahuje na pinu `EN`:
1. Interní pull-up rezistor (typicky $10\text{ k}\Omega$) k interní větvi $+3,3\text{ V}$.
2. Zpožďovací RC kondenzátor (typicky $100\text{ nF}$ až $1\text{ }\mu\text{F}$) proti zemi.
3. Obvod automatického bootování/resetu s tranzistory připojenými k USB převodníku (linky DTR/RTS).

Pokud je napájena pouze jedna deska (např. Horní deska má na sběrnici $+3,3\text{ V}$, zatímco Spodní deska je vypnutá a její napájení je na $0\text{ V}$), nastává kritický jev:
* Proud z $+3,3\text{ V}$ napájené desky protéká přes její $10\text{ k}\Omega$ pull-up rezistor, společným vodičem EN přechází do nenapájené desky a přes její pull-up rezistor a interní ESD ochranné diody čipu odtéká do její neaktivní 3,3V sběrnice (která se chová jako virtuální zem).
* Vzniká **odporový dělič napětí**, který stáhne napětí na společném uzlu EN pod $1\text{ V}$.
* Obě desky tím pádem uváznou v resetu. Navíc tranzistory programovacího obvodu z USB převodníku mohou stavět linku EN k zemi, pokud jsou řídicí signály plovoucí.

---

## 3. Návrh a realizace řešení

Pro bezchybnou funkci musí být signály `EN` obou mikrokontrolérů **galvanicky a směrově oddělené**. Tlačítko musí mít možnost stáhnout oba piny na logickou nulu (`GND`), ale piny se nesmí navzájem elektricky ovlivňovat.

### Řešení pomocí Schottkyho diod
Do obvodu byly zařazeny dvě rychlé Schottkyho diody **1N5819** (závěrné napětí $40\text{ V}$, proud $1\text{ A}$, nízký úbytek napětí v propustném směru $V_F \approx 0,2\text{ V}$ při malém proudu).

#### Schéma zapojení:
```
Pin EN (Horní ESP32-S3)  ------>|-----+
                          (D1 1N5819) |
                            Proužek   |     Tlačítko RST
                                      +------/  o-----> Společná GND
                            Proužek   |
Pin EN (Spodní ESP32)    ------>|-----+
                          (D2 1N5819)
```

* **Anoda D1** je připojena přímo na pin `EN` horního ESP32-S3.
* **Anoda D2** je připojena přímo na pin `EN` spodního ESP32.
* **Katody (označené proužkem)** jsou spojeny dohromady a připojeny na jeden kontakt spínacího tlačítka RST.
* Druhý kontakt tlačítka RST je připojen na systémovou zem (`GND`).

---

## 4. Princip fungování opraveného zapojení

1. **Klidový stav (tlačítko rozepnuto):**
   * Každá deska si drží svůj pin `EN` na stabilních $+3,3\text{ V}$ pomocí vlastního interního pull-up rezistoru.
   * Diody D1 a D2 jsou polarizovány v závěrném směru vůči sobě navzájem.
   * **Žádný proud nemůže přetékat mezi deskami.** I když je jedna deska napájena a druhá zcela vypnuta, na pinu EN napájené desky zůstává plných **3,3 V** a deska okamžitě a bezpečně nabootuje.
2. **Stisk tlačítka RST (sepnuto na GND):**
   * Katody obou diod jsou spojeny se zemí.
   * Oběma diodami začne protékat malý proud z pull-up rezistorů (cca $0,3\text{ mA}$) směrem do země.
   * Úbytek napětí na Schottkyho diodě 1N5819 je při tomto mikroproudu pouhých cca $0,2\text{ V}$.
   * Napětí na pinech `EN` obou mikrokontrolérů klesne na $0,2\text{ V}$, což je hluboko pod prahovou hodnotou $V_{IL} \le 0,8\text{ V}$.
   * **Oba mikrokontroléry se současně a čistě zresetují.**

---

## 5. Výsledek ověření v praxi
Po implementaci oddělovacích Schottkyho diod:
* Napětí na pinu `EN` obou desek v klidu dosahuje plných **3,28 V až 3,30 V** nezávisle na tom, jakým způsobem jsou desky napájeny (samostatné USB, adaptér nebo centrální step-down měniče).
* Spodní panel okamžitě po zapnutí inicializuje I2C sběrnici i OLED displej.
* Horní panel spolehlivě bootuje, inicializuje SPI TFT displej, zobrazuje `BOOT: OK` a provádí UART handshake.
* Stisk tlačítka RST provede korektní synchronní reset obou systémů.
