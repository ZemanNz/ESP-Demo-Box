# Příklad: Analogový Potenciometr

Tento příklad demonstruje měření polohy a napětí z běžného otočného potenciometru (např. 10kΩ) připojeného ke spodnímu panelu s ESP32-WROOM.

---

## Hardwarové zapojení

| Vývod na Potenciometru | Funkce | ESP32 DevKit Pin | Poznámka |
| :--- | :--- | :--- | :--- |
| **Krajní vývod 1** | Zem | **GND** | Společná zem |
| **Prostřední vývod** | Jezdec / Signál | **GPIO 34** | Analogový vstup (ADC1_CHANNEL_6) |
| **Krajní vývod 2** | Napájení | **3.3V** | Napájení 3.3V |

---

## Jak to funguje
1. **Otočný dělíč napětí**: Potenciometr funguje jako nastavitelný odporový dělič napětí. Otáčením hřídelky plynule měníme napětí na prostředním pinu od 0 V do 3.3 V.
2. **Převod ADC**: ESP32 převádí analogové napětí na digitální číslo v rozsahu **0 až 4095** (12-bitové rozlišení ADC).
3. **Výpočet napětí a grafika**: Program v C++ přepočítává naměřené číslo na přesné napětí v Voltech (`voltage = rawValue * (3.3 / 4095.0)`) a v Sériovém monitoru zobrazuje textový progress bar `[==========          ]`.
