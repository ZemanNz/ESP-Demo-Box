# Příklad: Analogový Joystick (Osa X, Y a tlačítko SW)

Tento příklad demonstruje čtení dvouosého analogového joysticku (např. modul **KY-023** nebo **HW-504**) s integrovaným spínacím tlačítkem, připojeného ke spodní desce ESP32-WROOM.

---

## Hardwarové zapojení

| Pin na Joysticku | Funkce | ESP32 DevKit Pin | Popis / Poznámka |
| :--- | :--- | :--- | :--- |
| **GND** | Zem | **GND** | Společná zem |
| **+5V / VCC** | Napájení | **3.3V** | Doporučeno 3.3V (aby analogový výstup nepřesáhl rozsah ADC) |
| **VRx / X** | Osa X | **GPIO 36 (VP)** | Analogový vstup osy X (ADC1_CHANNEL_0) |
| **VRy / Y** | Osa Y | **GPIO 39 (VN)** | Analogový vstup osy Y (ADC1_CHANNEL_3) |
| **SW / KEY** | Tlačítko | **GPIO 18** | Digitální vstup s interním PULL-UP (spíná proti GND) |

> **💡 Poznámka k pinům VP a VN:**  
> Na vývojové desce ESP32 jsou piny GPIO 36 a GPIO 39 označeny jako **VP** a **VN**. Jsou to čistě analogové vstupní piny převodníku ADC1 s minimálním rušením, ideální pro přesné čtení joysticku.

---

## Jak to funguje
1. **Analogové čtení (ADC)**: Funkce `analogRead()` vrací hodnotu v rozsahu **0 až 4095** (kde 0 odpovídá 0 V a 4095 odpovídá napětí 3.3 V). Ve klidové středové poloze je hodnota přibližně **2048**.
2. **Přepočet na procenta**: Pomocí funkce `map()` se naměřená hodnota 0–4095 přepočítává na relativní vychýlení v rozsahu **-100 % až +100 %**.
3. **Tlačítko (SW)**: Pin GPIO 18 je nastaven s interním pull-up rezistorem (`INPUT_PULLUP`). Při stisknutí páčky joysticku dolů se spínač sepne proti GND a hodnota přečtená pomocí `digitalRead()` klesne na `LOW`.
