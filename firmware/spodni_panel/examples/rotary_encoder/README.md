# Příklad: Rotační Enkodér (KY-040)

Tento příklad demonstruje přesné čtení inkrementálního rotačního enkodéru (např. modul **KY-040**) s integrovaným tlačítkem. Kód využívá **hardwarové přerušení (ISR)** a softwarový filtr zákmitů (debouncing).

---

## Hardwarové zapojení

| Pin na Enkodéru | Funkce | ESP32 DevKit Pin | Poznámka |
| :--- | :--- | :--- | :--- |
| **GND** | Zem | **GND** | Společná zem |
| **+ / VCC** | Napájení | **3.3V** | Napájení 3.3V |
| **CLK / A** | Hodinový signál | **GPIO 32** | Vstup s interním PULL-UPem |
| **DT / B** | Datový signál | **GPIO 4** | Vstup s interním PULL-UPem |
| **SW** | Tlačítko | **GPIO 18** | Vstup s interním PULL-UPem (spíná proti GND) |

---

## Jak to funguje
1. **Kvadraturní enkodér**: Otáčením hřídelky generuje enkodér dva fázově posunuté obdélníkové signály (CLK a DT). Podle toho, který signál změní stav dříve, mikrokontrolér pozná směr otáčení (CW = po směru, CCW = proti směru).
2. **Hardwarové přerušení (`attachInterrupt`)**: Při každé klesající hraně na pinu `CLK` se okamžitě vyvolá obslužná funkce `handleEncoderISR()`, což zaručuje, že nepřehlédneme žádný krok ani při velmi rychlém otáčení.
3. **Debouncing (odrušení zámitů)**: V přerušení se pomocí `micros()` kontroluje minimální časový odstup (2 ms) mezi pulzy, čímž se eliminují mechanické zákmity kontaktů.
4. **Reset tlačítkem**: Stisk hřídelky (tlačítko SW) vyvolá přerušení `handleButtonISR()`, které vynuluje počítadlo polohy zpět na `0`.
