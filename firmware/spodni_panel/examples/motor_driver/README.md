# Příklad: Řízení DC Motoru (PWM regulace)

Tento příklad demonstruje jednokanálovou regulaci otáček a výkonu stejnosměrného (DC) motoru nebo jiné PWM zátěže (např. výkonová LED, topné těleso) pomocí tranzistorového/MOSFET budiče připojeného ke spodní desce ESP32-WROOM.

---

## Hardwarové zapojení

| Vývod na Budiči motoru | Funkce | ESP32 DevKit Pin | Poznámka |
| :--- | :--- | :--- | :--- |
| **PWM / IN / Signál** | Vstup pro řízení | **GPIO 25** | Výstupní PWM pin |
| **GND** | Zem | **GND** | Společná zem (musí být propojena s ESP32) |
| **V+ / Power** | Zdroj napájení | **Externí zdroj** | Napájení motoru (dle jeho jmenovitého napětí) |

> **⚠️ Důležité upozornění pro napájení:**  
> Nikdy nepřipojujte DC motor přímo na výstupní GPIO pin ESP32! Výstupní piny mikrokontroléru snesou maximální proud 12 mA až 40 mA, zatímco motory odebírají stovky mA až jednotky Ampér. Vždy použijte odpovídající budič motoru (např. MOSFET modul L298N, DRV8833, IRF520 nebo tranzistorový spínač).

---

## Jak to funguje
1. **Pulzně-šířková modulace (PWM)**: Kód využívá vestavěnou funkci Arduino frameworku `analogWrite(25, duty)`.
2. **Rozsah střídy (Duty Cycle)**: Hodnota `duty` nabývá hodnot **0 až 255**:
   - `0` = 0 % střída (trvale vypnuto / 0 V)
   - `128` = 50 % střída (poloviční výkon)
   - `255` = 100 % střída (plný výkon / napájecí napětí)
3. **Plynulý náběh (Ramp-Up/Ramp-Down)**: V hlavní smyčce program postupně zvyšuje střídu v cyklu `for`, čímž docílí plynulého rozběhu motoru bez proudových nárazů.
