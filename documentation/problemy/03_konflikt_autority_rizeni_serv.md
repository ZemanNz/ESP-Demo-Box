# Problém: Konflikt autority řízení servomotorů v distribuovaném systému (Authority Race Condition)

## Popis problému
V našem projektu je hardware rozdělen na dva samostatné mikrokontroléry:
* **Horní panel (ESP32-S3):** Běží na něm stavový automat, hry a hlavní logika.
* **Dolní panel (ESP32-WROOM):** Jsou k němu fyzicky připojeny akční členy – klasické modelářské servo, chytré servo LX-16A, kontinuální servo a DC motor.

Při návrhu autonomního chování dolního panelu vyvstává zásadní architektonický problém: **Kdo má v daném okamžiku právo rozhodovat o poloze servomotorů?**

---

## Scénář vzniku kolize příkazů (Race Condition)

Představme si situaci v praxi:
1. **Autonomní činnost dolního panelu:** Dolní panel má v určitém režimu možnost reagovat lokálně na svůj analogový potenciometr a otáčet servem na úhel $120^\circ$.
2. **Pravidelná synchronizace z horního panelu:** Horní panel v rámci udržování spojení (tzv. *Heartbeat*) pravidelně každých 200 ms odesílá dolů stavový balíček `TopToBottomPacket`.
3. **Konflikt hodnot:** V tomto balíčku má horní panel uloženou poslední známou hodnotu úhlu serva z dřívější doby (např. $0^\circ$).
4. **Fyzický projev:** 
   * Dolní panel otočí servo na $120^\circ$.
   * O pár milisekund později dorazí z horního panelu příkaz $0^\circ$.
   * Dolní panel servo vrátí na $0^\circ$.
   * V dalším taktu lokální smyčky dolní panel servo opět otočí na $120^\circ$.

> **Výsledek:** Servomotor se dostane do stavu permanentního kmitání (**jittering / hunting**), dochází k mechanickému namáhání převodů, nadměrnému proudovému odběru a zahřívání motorku.

---

## Analýza podstaty problému

Tento jev je v robotice a distribuovaných řídicích systémech známý jako **Konflikt autority (Conflict of Authority)** neboli **Nedodržení jediného zdroje pravdy (Single Source of Truth violation)**.

K problému dochází proto, že o stejný fyzický akční člen usilují dva nezávislé řídicí procesy běžící na dvou oddělených procesorech bez přesně definované hierarchie řízení.

---

## Možné teoretické přístupy k řešení

Pro finální implementaci se zvažují následující přístupy:

### 1. Hierarchické řízení podle globálního módu (`AppMode`)
Autorita je pevně svázána s aktuálním stavem stavového automatu:
* V režimech her (`MODE_GAME_SNAKE`, `MODE_GAME_FLAPPY`) má dolní panel plnou autonomii – příkazy z horního panelu ignoruje a slouží pouze jako odesílatel telemetrie.
* V testovacích a manuálních režimech (`MODE_SERVA`, `MODE_MOTOR`) přebírá absolutní autoritu horní panel a dolní panel pouze vykonává příkazy.

### 2. Explicitní příznak převzetí řízení (`overrideAutonomy`)
V datovém paketu je vyhrazen boolean příznak:
* `overrideAutonomy = false` $\rightarrow$ Dolní panel řídí akční členy lokálně.
* `overrideAutonomy = true` $\rightarrow$ Dolní panel poslouchá hodnoty `targetServoAngle` z horního panelu.

### 3. Rezervovaná hodnota „Neřídit / Pasivní stav“ (Sentinel value)
Pokud horní panel nechce do řízení zasahovat, pošle v paketu neplatný úhel (např. `targetServoAngle = 255`, přičemž rozsah serva je $0–180^\circ$). Dolní panel tuto hodnotu interpretuje jako pokyn k zachování vlastní lokální logiky.

---

## Shrnutí pro text maturitní práce
Správné rozdělení autority je základním předpokladem stability distribuovaného systému. Bez jednoznačně definovaného rozhraní, které určuje, zda je akční člen řízen centrálně (Master) nebo lokálně (Slave), dochází k nebezpečným kolizím řídicích signálů.
