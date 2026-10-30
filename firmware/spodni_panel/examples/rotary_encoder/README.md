# Příklad: Rotační Enkodér (KY-040) s Grayovým stavovým automatem

Tento příklad demonstruje spolehlivé čtení inkrementálního rotačního enkodéru (**KY-040**) s integrovaným tlačítkem. Kód využívá **hardwarové přerušení (ISR)** a **stavový automat kvadraturního signálu (State Machine)**, který eliminuje mechanické zákmity kontaktů (bouncing).

---

## Hardwarové zapojení

| Pin na Enkodéru | Funkce | ESP32 DevKit Pin | Poznámka |
| :--- | :--- | :--- | :--- |
| **GND** | Zem | **GND** | Společná zem |
| **+ / VCC** | Napájení | **3.3V** | Napájení 3.3V |
| **CLK / A** | Hodinový signál | **GPIO 32** | Vstup s interním PULL-UPem |
| **DT / B** | Datový signál | **GPIO 4** | Vstup s interním PULL-UPem |
| **SW** | Tlačítko | **GPIO 2** | Vstup s interním PULL-UPem (spíná proti GND) |

---

## Jak to funguje
1. **Kvadraturní Grayův kód**: Otáčením hřídelky generuje enkodér dva fázově posunuté signály. V každém cvaknutí projdou signály 4 stavy.
2. **Přerušení na obou pinech (`CHANGE`)**: Sledujeme každou hranu na CLK i DT, takže mikrokontrolér neztratí žádný krok.
3. **Tabulkový stavový automat (State Machine)**: Tabulka validuje přesnou sekvenci přechodů. Jakýkoliv náhodný zákmit kontaktů (který by dříve způsobil skok zpátky) tabulka odfiltruje jako neplatný stav a ignoruje ho.
4. **1 cvaknutí = 1 krok**: Číslo pozice se změní přesně o `+1` nebo `-1` až po dosažení stabilní aretační polohy zubu.
