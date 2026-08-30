# Problém: Vkládání webového rozhraní do Flash paměti (`PROGMEM`), HTTP Caching a trvalá paměť

## Popis problému

Při tvorbě asynchronního webového serveru a WebSocket rozhraní pro **ESP-Demo-Box** vznikla potřeba vytvořit moderní a responzivní HTML5/CSS3/JS dashboard pro mobilní telefony. Objevily se následující zásadní otázky týkající se architektury paměti a přenosu dat:

1. **Vztah webových souborů a C++ kódu:** Jak přesně souvisí standardní webové soubory (`index.html`, `style.css`, `script.js`) s C++ hlavičkovým souborem `WebPages.h`? Jak se kód webu dostane do mikrokontroléru?
2. **Správa paměti v ESP32 (`PROGMEM`):** Jak funguje ukládání webových stránek v paměti mikrokontroléru, aby nedošlo k vyčerpání vzácné operační paměti RAM?
3. **Průběh síťového přenosu:** Jak přesně probíhá vyžádání a posílání webových souborů z ESP32 do mobilního telefonu po připojení na Wi-Fi?
4. **Klientská mezipaměť (HTTP Caching):** Jak zařídit, aby telefon nemusel při každém otevření stahovat velký CSS a JS soubor znovu z ESP32?
5. **Trvalé ukládání naměřených dat (Non-Volatile Storage):** Lze do Flash paměti ESP32 ukládat naměřené hodnoty (např. historii teploty), aby zůstaly zachovány i po vypnutí napájení?

---

## Rozbor a Řešení

### 1. Vztah webových souborů a `WebPages.h`

Kompilátor C++ (GCC) pro ESP32 neumí přímo kompilovat samostatné `.html`, `.css` nebo `.js` soubory jako kód. Zná pouze C++ datové typy (řetězce `const char[]`).

* **Soubory `index.html`, `style.css`, `script.js`:** Slouží jako zdrojové soubory pro pohodlnou vývojářskou práci ve VS Code (se syntaktickým zvýrazňováním a kontrolou syntaxe).
* **Soubor `WebPages.h`:** Obsahuje **přesné kopie textu** těchto souborů vložené do C++ konstanta řetězců pomocí C++ *Raw String Literal* (`R"rawliteral(...)rawliteral"`):

```cpp
// Ukázka z WebPages.h: HTML kód je vložen do C++ řetězce INDEX_HTML
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="cs">
<head>
    <link rel="stylesheet" href="style.css">
</head>
<body> ... </body>
</html>
)rawliteral";
```

Díky zápisu `R"rawliteral(...)"` si C++ nevšímá uvozovek ani nových řádků uvnitř HTML/CSS/JS a bere celý blok jako jeden čistý řetězec znaků.

---

### 2. Ukládání v paměti mikrokontroléru (`PROGMEM` / Flash vs. RAM)

ESP32-S3 disponuje dvěma typy paměti:
* **RAM (Internal SRAM - cca 512 KB):** Rychlá operační paměť pro proměnné, výpočty a zásobník úloh FreeRTOS.
* **Flash (External SPI Flash - 16 MB):** Trvalé uložení nahraného programu (firmware).

Kdybychom proměnné webu definovali bez makra `PROGMEM`, kompilátor by je při startu ESP32 zkopíroval z Flash do RAM paměti, což by spotřebovalo zbytečně desítky kilobajtů RAM.

Klíčové slovo **`PROGMEM`** (Program Memory) instruuje kompilátor:
> *"Tato data ponechej trvale uložená ve Flash paměti. Nenačítej je při startu do RAM. Až je bude síťový server potřebovat poslata mobilu, přečti je přímo z Flash."*

---

### 3. Průběh síťového přenosu z ESP32 do telefonu

Když se mobil připojí k Wi-Fi `ESP-Demo-Box` a otevře rozhraní, proběhne následující sekvence požadavků:

1. **Požadavek 1 (HTML):** Mobil pošle `GET / HTTP/1.1`.
   * ESP32 v `WebManager.cpp` zavolá:  
     `request->beginResponse_P(200, "text/html", INDEX_HTML);`
   * Přečte `INDEX_HTML` z Flash paměti a odešle ho do mobilu.
2. **Parsing v mobilu:** Prohlížeč v mobilu přečte HTML a narazí na tagy `<link rel="stylesheet" href="style.css">` a `<script src="script.js"></script>`.
3. **Požadavek 2 (CSS):** Mobil pošle `GET /style.css HTTP/1.1`.
   * ESP32 odpoví zprávou obsahující `STYLE_CSS` z Flash.
4. **Požadavek 3 (JS):** Mobil pošle `GET /script.js HTTP/1.1`.
   * ESP32 odpoví zprávou obsahující `SCRIPT_JS` z Flash.
5. **Navázání WebSocketu:** `script.js` v mobilu spustí `new WebSocket("ws://192.168.4.1/ws")`. Od této chvíle se již HTTP soubory nestahují a obě strany si posílají pouze malé živé JSON balíčky o velikosti ~200 B.

---

### 4. Klientská mezipaměť (HTTP Caching)

Aby mobil nemusel při každém novém otevření webu stahovat CSS a JS soubory z ESP32 znovu, přibaluje ESP32 k HTTP odpovědi hlavičku **`Cache-Control`**:

```cpp
response->addHeader("Cache-Control", "public, max-age=86400"); // 86400 sekund = 24 hodin
```

* **Efekt:** Prohlížeč v mobilním telefonu si soubory `style.css` a `script.js` uloží do své lokální paměti.
* Při dalším načtení stránky mobil z ESP32 stáhne pouze lehoučké HTML a styly i skripty vytáhne ze své vlastní paměti. Tím se šetří přenosové pásmo Wi-Fi a CPU výkon ESP32.

---

### 5. Trvalé ukládání naměřených dat ve Flash paměti (NVS & LittleFS)

**Ano! Do Flash paměti ESP32 lze ukládat libovolná naměřená data (např. historii teplot, logy chyby, kalibraci), která zůstanou zachována i po kompletním vypnutí napájení.**

V prostředí ESP32/Arduino se pro trvalé ukládání používají 2 hlavním způsoby:

#### A) Knihovna `Preferences` (NVS - Non-Volatile Storage)
* Vhodné pro drobné hodnoty (nastavení, uložené módy, kalibrace, počítadla).
* Ukládá data jako dvojice *Klíč -> Hodnota*.
```cpp
#include <Preferences.h>
Preferences prefs;

// Zápis uložené teploty
prefs.begin("sensors", false); // Otevřít sekci "sensors" pro zápis
prefs.putFloat("lastTemp", 24.5);
prefs.end();

// Čtení po restartu/zapnutí
prefs.begin("sensors", true);  // Otevřít pouze pro čtení
float temp = prefs.getFloat("lastTemp", 0.0);
prefs.end();
```

#### B) Souborový systém `LittleFS`
* Vhodné pro věší množství strukturovaných dat (např. soubor `history.csv` s tisíci naměřenými teplotami).
```cpp
#include <LittleFS.h>

// Uložení jednoho řádku měření do souboru
File file = LittleFS.open("/history.csv", FILE_APPEND);
if (file) {
    file.printf("%lu,%.2f,%.2f\n", millis(), temp, hum);
    file.close();
}
```

> [!NOTE]
> **Pozor na opotřebení Flash paměti (Wear Leveling):** Flash paměť má omezený počet zápisových cyklů (cca 100 000 zápisů na sektor). Proto se naměřená data neukládají do Flash každou sekundu, ale ukládají se v bufferu v RAM a do Flash se zapisují např. jen jednou za hodinu nebo při vypínání systému.
