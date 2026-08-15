# Problém: Rozdělení paměťového prostoru a práce s grafikou

## Popis situace
V tradičním pojetí programování začátečníci často považují veškerou fyzicky dostupnou paměť za jeden sdílený prostor, do kterého mohou ukládat proměnné a číst z něj napříč celým zařízením.

V architektuře výukového kufru jsou přítomny dva zcela nezávislé čipy (ESP32-S3 a ESP32), z nichž každý disponuje vlastní oddělenou operační pamětí (SRAM). Navíc samotný hlavní čip ESP32-S3 disponuje další externí pamětí (16 MB PSRAM). Je nutné přesně definovat, jaká data žijí ve které paměti.

## Architektonické řešení: Definice hranic a alokace
Základním architektonickým pravidlem je absolutní izolace sdíleného stavového prostoru.

1. **Izolace Master-Slave:** Globální stavový objekt (`SystemState`), který definuje, v jakém režimu se systém nachází a jaká jsou aktuální naměřená data, existuje **výlučně v operační paměti RAM na horním panelu** (Master). Spodní panel o tomto prostoru nemá žádné fyzické povědomí a přistupuje k němu výhradně formou zprostředkovaných UART paketů (požadavek na změnu).
2. **Eliminace fragmentace RAM pomocí PSRAM:** Běžná operační paměť mikrokontroléru má velmi omezenou kapacitu (cca 500 KB). Kdyby se rozsáhlé grafické prvky displeje (Framebuffery) nebo webové HTML stránky načítaly do běžných C++ proměnných typu `String`, došlo by k rychlé fragmentaci a vyčerpání haldy (Heap), což vede k pádu systému. Projekt proto vyčleňuje využití externí 16 MB velké PSRAM. Rozbalování JPG obrázků a renderování grafiky z LittleFS je směřováno prostřednictvím DMA (Direct Memory Access) striktně do této PSRAM, čímž zůstává rychlá interní SRAM zcela uvolněna pro rychlé vlákna FreeRTOS a matematické výpočty.
