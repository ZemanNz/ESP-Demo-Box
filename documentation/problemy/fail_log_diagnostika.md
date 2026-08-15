# Problém: Diagnostika a vizualizace kritických chyb hardwaru (Fail Log)

## Popis situace
Při vývoji složitých distribuovaných systémů s desítkami periferií (senzory, displeje, motory) představuje selhání i jediné komponenty vážný problém. Tradičně se mikrokontrolér při chybě senzoru buď "tiše" zasekne v nekonečné smyčce, nebo vypíše chybu do Sériového terminálu. V reálném nasazení ale uživatel (nebo hodnoticí komise) nemá připojený počítač s USB kabelem, aby si tento chybový log přečetl. Pokud zařízení po zapnutí nenaběhne, uživatel neví proč.

Navíc, pokud dojde k hardwarové chybě na spodním panelu (např. spálený servomotor), horní panel (Master) tuto informaci standardně vůbec nezjistí.

## Architektonické řešení: Unified Fail Log a vizualizace přes Handshake
Systém výukového kufru je navržen s důrazem na absolutní uživatelskou přívětivost (Fault-Tolerant UX) a automatickou diagnostiku. Veškeré chybové hlášky z obou čipů jsou centralizovány na hlavním 2.8" TFT displeji.

### 1. Modulární inicializace (Hardware Setup)
Každý čip obsahuje modulární inicializační blok (soubor `HardwareSetup`), který postupně testuje každý připojený senzor. Funkce vrací hodnotu `bool` (Úspěch / Selhání). Pokud jakákoliv periferie selže, hlavní procesor proces bootování okamžitě přeruší a zavolá funkci pro vykreslení speciální červené "Kritické obrazovky" na TFT displeji. Na ní se velkým písmem vypíše přesný důvod selhání (např. *„CHYBA: Senzor teploty DHT11 na pinu 3 neodpovídá!“*).

### 2. Delegování chyb ze spodního panelu (Slave -> Master Error Reporting)
Pokud dojde ke kritické závadě na spodním panelu, je k hlášení chyby využit dříve definovaný Handshake protokol:
*   Spodní panel při startu zjistí selhání (např. chytré servo LX-16A neodpovídá na ping).
*   Namísto obvyklé zprávy `[SYS_READY]` odešle přes UART chybový rámec: `[ERR_INIT, "CHYBA SERVA LX-16A"]`.
*   Horní panel (Master), který po nastartování čeká na potvrzení od spodní desky, tento chybový rámec zachytí.
*   Master okamžitě přeruší své vlastní bootování a na hlavním TFT displeji vykreslí obdrženou chybovou hlášku ve formátu: *„KRITICKÁ CHYBA SPODNÍHO PANELU: CHYBA SERVA LX-16A“*.

### Závěr a přínos
Tato diagnostická architektura (Fail Log) garantuje, že jakýkoliv defekt v zapojení nebo hardwaru na kterékoliv z desek je okamžitě, čitelně a vizuálně oznámen uživateli na primárním zobrazovacím zařízení. Eliminuje se tak nutnost připojovat systém k počítači za účelem čtení skrytých logů z terminálu.
