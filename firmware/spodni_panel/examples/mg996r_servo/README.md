# Příklad: Kontinuální servo MG996R (Standardní modelářské PWM)

Tento příklad demonstruje ovládání standardního kontinuálního modelářského serva **TowerPro MG996R** připojeného ke spodnímu panelu s ESP32. Servo se otáčí kontinuálně (o 360 stupňů) a šířka pulzu z mikrokontroléru neurčuje úhel otočení, nýbrž **směr a rychlost rotace**.

## Hardwarové zapojení

Standardní servo MG996R má 3vodičový kabel se standardním modelářským barevným kódováním:

| Vodič serva (Barva) | Funkce | ESP32 DevKit Pin | Poznámka |
| ------------------- | ------ | ---------------- | -------- |
| **Oranžová** (nebo žlutá) | **Signal** (PWM) | **GPIO 14**      | Signální řídicí vodič (PWM 50 Hz) |
| **Červená**             | **VCC** (Napájení) | **5V** / **Externí zdroj** | Napájení serva (doporučeno 5V až 6V) |
| **Hnědá** (nebo černá)   | **GND** (Zem)      | **GND**           | Společná zem |

*Upozornění: Servo MG996R je silný motor s kovovými převody. Při rozběhu nebo v zátěži může odebírat proudy přesahující **1.5 A**. Napájení přímo z 5V pinu desky ESP32 (napájené z USB počítače) může fungovat pouze pro pohyb naprázdno. Pro praktické nasazení se silně doporučuje použít **externí napájecí zdroj 5V až 6V (min. 2A)** se spojenou zemí (GND).*

---

## Použité knihovny
- **ESP32Servo** (madhephaestus/ESP32Servo)

Tato knihovna je spravována PlatformIO a automaticky se stáhne při kompilaci díky konfiguraci v `platformio.ini`.

## Jak to funguje
1. Klasické kontinuální servo se ovládá standardním PWM signálem s frekvencí **50 Hz** a šířkou pulzu obvykle od **1000 us do 2000 us**:
   - Pulz **1500 us** (v kódu `myServo.write(90)`) odpovídá **zastavení**.
   - Pulz **1000 us** (v kódu `myServo.write(0)`) odpovídá **plné rychlosti zpětného chodu**.
   - Pulz **2000 us** (v kódu `myServo.write(180)`) odpovídá **plné rychlosti dopředného chodu**.
   - Hodnoty mezi těmito hranicemi plynule regulují rychlost otáčení v daném směru.
2. Program v loopu střídá režimy: pomalý chod vpřed -> plná rychlost vpřed -> zastavení -> pomalý chod vzad -> plná rychlost vzad -> zastavení.
3. Informace o aktuální akci jsou posílány na sériový monitor (rychlost **115200 baudů**).
