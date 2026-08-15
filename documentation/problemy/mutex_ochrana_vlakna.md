# Problém: Vzájemné vyloučení při zpracování dat (Race Condition)

## Popis situace
Mikrokontrolér ESP32-S3 je vybaven dvoujádrovým procesorem (Core 0 a Core 1). Pro zajištění vysokého výkonu byl systém rozdělen pomocí operačního systému FreeRTOS do nezávislých vláken (Tasks), přičemž některá vlákna běží na nultém jádře (zpracování Wi-Fi, poslech UARTu) a jiná paralelně na prvním jádře (vykreslování TFT displeje, čtení senzorů).

Všechna tato vlákna ale přistupují ke sdílené paměti – globálnímu objektu `SystemState`. Pokud by vlákno z prvního jádra začalo z paměti číst pozici kurzoru pro účely vykreslení menu na displeji v naprosto stejné mikrosekundě, ve které vlákno z nultého jádra přijme příkaz z UARTu a rozhodne se tuto paměť přepsat, došlo by ke kolizi (tzv. Data Corruption) a k destrukci integrity dat, což by způsobilo okamžitý pád čipu (Core Panic).

## Architektonické řešení: Ochrana paměti pomocí Mutexu
Základním chybovým předpokladem laické veřejnosti je, že existuje mechanismus, který "zamyká" samotnou fyzickou paměť RAM. To je omyl. Zabezpečení proti kolizi dat zajišťuje koncept **Mutexu** (Mutual Exclusion – Vzájemné vyloučení), což je softwarová kooperativní dohoda implementovaná na úrovni plánovače operačního systému (OS Scheduler).

1. **Princip klíče:** Mutex funguje jako jeden exkluzivní "klíč". Kdykoliv chce jakékoliv vlákno v systému (ať už z Core 0 nebo Core 1) přistoupit ke sdílené proměnné, musí si pomocí funkce `xSemaphoreTake()` tento klíč od OS vyžádat.
2. **Uspání konkurenčního vlákna:** Pokud si klíč uzme vlákno na Core 0 za účelem zápisu, a v tentýž moment o něj požádá vlákno na Core 1, operační systém mu ho nevydá. FreeRTOS proces ve vlákně na Core 1 **zcela uspí (zablokuje)**. Nedojde tak k žádnému souběžnému zápisu.
3. **Obnovení běhu:** Jakmile Core 0 ukončí zápis a klíč odevzdá funkcí `xSemaphoreGive()`, operační systém okamžitě probudí uspávací vlákno na Core 1 a umožní mu bezpečně přečíst již zapsaná, platná data. Tím je dosaženo absolutní stability i při vysokém stupni paralelismu bez ohrožení systémové RAM.
