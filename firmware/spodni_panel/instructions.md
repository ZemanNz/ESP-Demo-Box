# Pokyny a Prompt pro novou konverzaci (Spodní panel - ESP32)

Zkopírujte celý text níže a vložte jej jako úvodní zadání do nového chatu s AI asistentem.

---

Ahoj, v této konverzaci budeme pracovat na vývoji firmwaru pro **spodní panel** (složka `firmware/spodni_panel`). 

Mám připojenou desku **klasické ESP32 DevKit** (nikoliv ESP32-S3 jako u horního panelu).

Dodržuj prosím následující pravidla a postupy při vývoji a interakci:

1. **Komunikace**: Vždy se mnou komunikuj v češtině.
2. **PlatformIO nastavení**: 
   - Používáme klasickou desku ESP32 (v `platformio.ini` by měla být nastavená jako `board = esp32dev`).
   - Pokud budeme instalovat nové knihovny, vždy je přidej do `lib_deps` v `platformio.ini`.
3. **Kompilace a nahrávání**:
   - Pokud budeme pracovat na příkladech (např. ve složce `examples/`), kompiluj a nahrávej je tak, že dočasně na začátek `platformio.ini` přidáš blok:
     ```ini
     [platformio]
     src_dir = examples/nazev_prikladu
     ```
     Poté spusť nahrávání pomocí příkazu `~/.platformio/penv/bin/pio run -t upload`.
   - **DŮLEŽITÉ**: Ihned po úspěšném nahrání tento dočasný blok `[platformio]` z `platformio.ini` odstraň a ulož soubor, aby projekt zůstal čistý a ukazoval na výchozí `src/main.cpp`.
4. **Git a GitHub (Automatické odesílání)**:
   - Po každé úspěšné změně, odladění nebo nahrání kódu na desku automaticky proveď Git commit a push.
   - Neptej se mě na povolení, rovnou spusť příkazy:
     - `git add .` (případně specifikuj změněné soubory)
     - `git commit -m "Stručný a výstižný popis změn v češtině"`
     - `git push`
5. **Efektivita**: Neprohledávej zbytečně soubory v celém počítači, soustřeď se pouze na adresář `firmware/spodni_panel` a jeho okolí v tomto projektu.

Nyní mi potvrď, že rozumíš zadání, a zkontroluj aktuální stav složky `firmware/spodni_panel`, zda v ní máme inicializovaný projekt PlatformIO.
