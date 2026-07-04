# Příklad: Sedmisegmentový displej s posuvnými registry 74HC595 (Static Drive)

Tento příklad demonstruje připojení a ovládání vícemístného sedmisegmentového LED displeje řízeného kaskádou posuvných registrů **74HC595** k mikrokontroléru **ESP32-S3**.

Program používá softwarové SPI (bit-banging) na libovolných GPIO pinech bez potřeby externích knihoven a každých 500 ms inkrementuje běžící počítadlo.

---

## Propojení (Zapojení)

Displej je připojen pomocí 3 datových linek:

| Pin Displeje | ESP32-S3 Pin | Popis |
| ------------ | ------------ | ----- |
| **VCC**      | `5V` (nebo `3V3`*) | Napájení (pro jasnější LED doporučujeme 5V větev) |
| **GND**      | `GND`        | Společná zem |
| **DIO / DS (Data)** | `GPIO 7` | Vstup sériových dat (Data Input / Serial Data) |
| **SCLK / SH_CP (Clock)** | `GPIO 3` | Hodiny posuvu (Shift Register Clock) |
| **RCLK / ST_CP (Latch)** | `GPIO 4` | Latch / Zkopírování dat na výstupy (Storage Register Clock) |

*\* Poznámka k napájení: Většina 74HC595 modulů s LED displeji funguje i na 3.3 V z ESP32-S3, ale pro vyšší jas doporučujeme 5 V.*

---

## Princip fungování posuvného registru 74HC595 (Maturitní obhajoba)

Při obhajobě maturitního projektu můžeš vysvětlit následující technické principy:

1. **Sériově-Paralelní převodník (SISO/SIPO)**:
   * 74HC595 je 8-bitový posuvný registr se záchytným registrem (latch).
   * Přijímá data sériově po jednom bitu (přes pin **DS / DIO**) a na povel hodinového signálu (**SH_CP / SCLK**) posune data o jednu pozici dál.
   * Jakmile odešleme všech 8 bitů (celý bajt), vyšleme pulz na pin **ST_CP / RCLK (Latch)**, který zkopíruje stav z posuvného registru na 8 výstupních pinů (Q0–Q7) naráz. Tím se zabrání "blikání" segmentů během posouvání dat.
2. **Kaskádové zapojení (Daisy Chaining)**:
   * Posuvný registr má výstupní pin **Q7'** (Serial Out). Když do čipu pošleme 9. bit, 1. bit "přeteče" ven přes tento pin.
   * Propojením pinu Q7' prvního čipu na pin DS druhého čipu zapojíme registry do kaskády. To nám umožňuje ovládat např. 3 číslice (3 čipy = 24 výstupů) pomocí stále stejných 3 vodičů z ESP32-S3.
3. **Statické buzení (Static Drive)**:
   * Na rozdíl od dynamického buzení (multiplexování), kde procesor musí neustále střídat rozsvěcování jednotlivých číslic, u statického buzení drží výstupy 74HC595 stálé napětí.
   * Procesor pošle data pouze tehdy, když se hodnota mění. Displej nebliká a nezatěžuje procesor.

---

## Binární kód, Desetinná tečka a Hexadecimální převod

Pro ovládání segmentů číslic posíláme do registru **jeden bajt (8 bitů)** pro každou číslici.

### Schéma segmentů displeje
Každá číslice se skládá z následujících segmentů označených písmeny **A** až **G** a desetinné tečky **DP**:
```text
      --- A ---
     |         |
     F         B
     |         |
      --- G ---
     |         |
     E         C
     |         |
      --- D ---   [DP] (tečka)
```

### Mapování bitů v bajtu
Každý bit v odesílaném bajtu ovládá konkrétní segment na displeji:

| Bit pozice | Bit 7 | Bit 6 | Bit 5 | Bit 4 | Bit 3 | Bit 2 | Bit 1 | Bit 0 |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Segment** | **DP** (tečka) | **G** | **F** | **E** | **D** | **C** | **B** | **A** |

*   **Bit 0 (LSB - nejnižší bit):** Ovládá segment **A** (horní vodorovná čárka).
*   **Bit 7 (MSB - nejvyšší bit):** Ovládá desetinnou tečku **DP**.

### Co znamená prefix `0b` a `0x`?
V programování se používají předpony (prefixy) pro rozlišení číselných soustav:
*   **`0b` (Binary)** – Značí dvojkovou (binární) soustavu. Např. `0b01111111` je zápis osmi konkrétních jedniček a nul.
*   **`0x` (Hexadecimal)** – Značí šestnáctkovou soustavu. Např. `0x7F` je zkrácený zápis pro stejnou hodnotu.

### Jak převést binární kód (`0b`) na šestnáctkový (`0x`)?
Převod provedeme tak, že 8bitový bajt rozdělíme napůl na dvě čtveřice bitů (nibbly) a každou čtveřici převedeme na jeden šestnáctkový znak (0-9, A-F):

1.  **Číslo 8 bez tečky** (`0b01111111`):
    *   Levá polovina: `0111` $\rightarrow$ V desítkové soustavě 7 $\rightarrow$ Hexadecimálně **`7`**
    *   Pravá polovina: `1111` $\rightarrow$ V desítkové soustavě 15 $\rightarrow$ Hexadecimálně **`F`**
    *   Dohromady: **`0x7F`**
2.  **Číslo 8 s tečkou** (`0b11111111`):
    *   Levá polovina: `1111` $\rightarrow$ V desítkové soustavě 15 $\rightarrow$ Hexadecimálně **`F`**
    *   Pravá polovina: `1111` $\rightarrow$ V desítkové soustavě 15 $\rightarrow$ Hexadecimálně **`F`**
    *   Dohromady: **`0xFF`**

### Rozdíl mezi 8 a 8 s tečkou:
Rozdíl je pouze v nejvyšším bitu (Bit 7 / DP), který leží úplně vlevo:
*   **8 bez tečky:** `0b01111111` (nejvyšší bit = `0` $\rightarrow$ tečka nesvítí) $\rightarrow$ **`0x7F`**
*   **8 s tečkou:** `0b11111111` (nejvyšší bit = `1` $\rightarrow$ tečka svítí) $\rightarrow$ **`0xFF`**

*(Poznámka: U displeje se Společnou Anodou se tato logika invertuje, takže u 8 s tečkou svítí všechny segmenty při přivedení nul, tzn. binárně `0b00000000` / `0x00`).*

---

## Nastavení typu displeje v kódu

Většina levných čínských modulů s 74HC595 používá LED displeje se **Společnou Anodou (Common Anode)**. Pro rozsvícení segmentu je potřeba přivést logickou nulu (`0`).
Některé moduly ale mohou mít **Společnou Katodu (Common Cathode)**, kde se segmenty rozsvěcují logickou jedničkou (`1`).

V kódu toto chování snadno přepneš změnou konstanty:
```cpp
const bool COMMON_ANODE = true; // Nastav na false, pokud segmenty svítí opačně.
```

---

## Jak spustit tento příklad

1. Propojte displej podle tabulky zapojení (přepojte CS na GPIO 4).
2. Zkopírujte kód ze souboru `main.cpp` v této složce a nahraďte jím kód v `src/main.cpp`.
3. Nahrajte program do ESP32-S3.
4. Počítadlo začne běžet od nuly a každých 500 ms se zvýší o 1.
