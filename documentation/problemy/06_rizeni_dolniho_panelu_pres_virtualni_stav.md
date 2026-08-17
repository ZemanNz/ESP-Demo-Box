# Problém: Řízení periferií dolního panelu čistě přes virtuální stav v SystemState

## Popis problému
Horní panel (ESP32-S3) slouží jako hlavní mozek zařízení, kde běží uživatelské rozhraní, menu i hry. Uživatel však potřebuje z těchto aplikací ovládat periferie, ke kterým **z horní desky nevedou žádné fyzické vodiče** – například:
* Změnit úhel servomotoru na dolním panelu.
* Nastavit rychlost DC motoru.
* Změnit barvu nebo jas 8-LED WS2812B pásku na dolní desce.
* Vypsat text na 0.96" OLED displej dolního panelu.

Pokud by každá hra nebo položka menu musela napřímo otevírat sériovou linku, sestavovat UART pakety a řešit komunikační chyby, došlo by k extrémnímu provázání kódu (**Tight Coupling**) a aplikační logika by byla zahlcena nízkoúrovňovou komunikací.

---

## Řešení: Abstrakce hardware přes virtuální stavový model (Decoupling)

Řešením bylo zavést **koncept virtuálního hardwarového stavu** uvnitř centrální struktury `SensorData`.

Pro herní logiku nebo menu se periferie dolního panelu tváří úplně stejně jako lokální proměnné:
1. Aplikační kód zavolá standardní setter v `SystemState`.
2. Setter zapíše hodnotu do virtuálního stavu v paměti RAM a **automaticky zvedne příznak `bottomNeedsTx = true`**.
3. O samotné fyzické doručení po drátech UART se transparentně na pozadí postará FreeRTOS úloha `Task_UART` běžící na jádře Core 0.

---

## Architektonické schéma toku dat

```text
[ Herní logika / Menu na Core 1 ]
               |
               | 1. Volání: globalState.updateServo(90);
               v
[ SystemState (Stav v RAM) ]
   - sensors.servoAngle = 90;
   - bottomNeedsTx = true;  <--- Automatické zvednutí příznaku
               |
               | 2. Přečtení příznaku ve smyčce (Core 0)
               v
[ Task_UART na Core 0 ]
   - Sestavení paketu TopToBottomPacket
   - Výpočet kontrolního součtu
   - SerialESP.write(...)
               |
               | 3. Fyzický přenos UART (2.6 ms)
               v
[ Dolní panel (ESP32-WROOM) ]
   - Nastavení PWM signálu pro servo na 90°
```

---

## Ukázka implementace v kódu

### 1. Setter v `SystemState.h`:
```cpp
void updateServo(uint8_t angle) {
    if (xSemaphoreTake(stateMutex, (TickType_t)10) == pdTRUE) {
        sensors.servoAngle = angle;
        bottomNeedsTx = true; // Automaticky upozorníme UART vlákno
        xSemaphoreGive(stateMutex);
    }
}
```

### 2. Použití v aplikační vrstvě (např. v menu nebo hře):
```cpp
// Uživatel pohnul kurzorem v menu servomotorů:
globalState.updateServo(120); 

// Kód hry nemusí vědět, že servo je připojeno na jiném čipu přes UART!
```

---

## Výhody tohoto přístupu pro projekt

1. **Čistota kódu (Separation of Concerns):** Grafické rozhraní a herní logika se starají pouze o data, nikoliv o přenosové protokoly a baudové rychlosti.
2. **Okamžitá zpětná vazba pro UI:** Displej na horním panelu okamžitě zobrazí novou hodnotu úhlu serva z paměti RAM (latence < 0,1 µs), zatímco UART přenos probíhá asynchronně na pozadí.
3. **Snadná simulace:** V testovacím režimu bez připojeného dolního panelu funguje celé grafické menu beze změny kódu.
