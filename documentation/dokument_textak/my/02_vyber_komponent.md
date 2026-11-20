## 2 Výběr komponent

Při výběru komponent jsem se inspiroval předlohou z VUT, nakonec se však ukázalo, že část jejích senzorů není z pohledu praktického využití příliš zajímavá. Z toho důvodu jsem zvolil součástky výrazně interaktivnější, které najdou širší uplatnění v robotickém kroužku.

V kufříku jsem se snažil uplatnit jak jednoduché digitální a analogové senzory, tak i pokročilejší moduly komunikující po sběrnicích. Vzhledem k prostorovým možnostem kufru jsou periferie fyzicky rozděleny do dvou částí – horní panel ve víku kufru nese především zobrazovací prvky a senzory, zatímco spodní panel soustřeďuje ovládání a mechanické pohony.

### 2.1 Srovnání a výběr mikrokontrolérů

Kvůli velkému počtu potřebných pinů a snaze omezit kabeláž procházející ohybem kufru jsem použil dva samostatné mikrokontroléry, pro každý panel zvlášť. Vzorový demokit z VUT využívá pro řízení také dvě osmibitové vývojové desky značky Arduino – konkrétně Arduino UNO R3 v horní desce a větší Arduino MEGA 2560 ve spodní desce. Ačkoliv je tato kombinace pro základní ovládání periferií dostačující, pro náročnější grafiku, více současně běžících úloh (multitasking) a bezdrátové ovládání přes telefon již svými parametry nestačí. Z tohoto důvodu jsem pro svůj kufřík zvolil modernější 32bitové čipy z rodiny ESP32. Základní srovnání obou přístupů shrnuje Tabulka 2-2-1.


**Tabulka 2-2-1: Srovnání řídicích desek (ESP-Demo-Box vs. VUT Arduino demokit)**


| **Deska / role v kufru** | **Procesor a takt** | **Paměť (Flash / RAM)** | **I/O piny** | **Konektivita** |
| --- | --- | --- | --- | --- |
| ESP32-S3-DevKitC-1 (Horní panel) | 32bit dual-core Xtensa LX7, 240 MHz | 16 MB Flash, 512 KB SRAM + 8 MB PSRAM | 44 pinů modulu (cca 36 volných GPIO) | Wi-Fi 2,4 GHz, BLE 5.0 |
| ESP32-WROOM-32 DevKitC-V4 (Spodní panel) | 32bit dual-core Xtensa LX6, 240 MHz | 4 MB Flash, 520 KB SRAM | 38 pinů (34 programovatelných GPIO) | Wi-Fi 2,4 GHz, Bluetooth |
| Arduino UNO R3 (VUT – horní deska A) | 8bit AVR ATmega328P, 16 MHz | 32 KB Flash, 2 KB SRAM | 14 digitálních, 6 analogových | Bez Wi-Fi/BT (pouze UART) |
| Arduino MEGA 2560 (VUT – spodní deska B) | 8bit AVR ATmega2560, 16 MHz | 256 KB Flash, 8 KB SRAM | 54 digitálních, 16 analogových | Externí Ethernet shield W5100 |

Pro horní panel bylo potřeba zvolit výkonnější modul ESP32-S3-DevKitC-1 s více piny. Hlavním důvodem byl vysoký počet připojených periferií, které by se na základní desky ESP32 bez přídavných expandérů nevešly. Dalším důležitým faktorem byla nutnost velké operační paměti. Externí paměť PSRAM totiž umožňuje držet v paměti celý snímek pro 2,8" TFT displej (tzv. framebuffer), což zajišťuje plynulé vykreslování grafiky bez problikávání. 

Spodní panel pak řídí osvědčený modul ESP32-WROOM-32 (DevKitC-V4). Na tomto panelu není potřeba vykreslovat složitou grafiku ani hostovat webové rozhraní. Deska WROOM-32 nabízí dostatek pinů s hardwarovým PWM i analogově-digitálních převodníků (ADC) pro spolehlivé snímání joysticku, tlačítek, potenciometru, enkodéru a zároveň pro nezávislé řízení trojice servomotorů.

### 2.2 Přehled použitých komponent

Kompletní výčet všech součástek osazených na horním a spodním panelu včetně použitého komunikačního rozhraní a jejich funkce v kufříku uvádějí Tabulka 2-2 a Tabulka 2-3.

Vzhledem k velkému počtu použitých prvků jsem se v této kapitole rozhodl nerozepisovat každou součástku. V následující podkapitole se proto věnuji pouze několika vybraným komponentám, se kterými jsem při stavbě kufříku pracoval poprvé, nebo které jsou technicky zajímavé.


**Tabulka 2-2: Přehled komponent horního panelu (ESP32-S3)**


| **Komponenta** | **Typ / modul** | **Rozhraní** | **Účel v kufříku** |
| --- | --- | --- | --- |
| Řídicí mikrokontrolér | ESP32-S3-DevKitC-1 (N16R8) | USB / UART / SPI / I2C | Hlavní řídicí jednotka, GUI, senzory a Wi-Fi |
| Grafický displej | 2,8" TFT LCD (ST7789, 240×320 px) | SPI | Hlavní menu, vizualizace dat ze senzorů a hry |
| Znakový displej | LCD 1602 (16×2 znaků, PCF8574) | I2C (0x27) | Doplňkové textové a stavové informace |
| 7segmentový displej | 3místný LED displej s 74HC595 | 3× GPIO (posuvný registr) | Rychlé zobrazení číselných hodnot a skóre |
| RGB LED pásek | 8× adresovatelná LED WS2812B | 1× GPIO (digitální) | Barevné efekty, indikace stavu a světelný mód |
| Indikační LED diody | 3× jednobarevná LED (D1, D4, D5) | 3× GPIO | Optická indikace senzorů vzdálenosti |
| Akustický bzučák | Bzučák s tranzistorem 2N3906 | GPIO (PWM) | Zvuková odezva menu, her a Wi-Fi radaru |
| Senzor teploty a vlhkosti | DHT11 | 1vodičové digitální (GPIO) | Měření teploty a relativní vlhkosti vzduchu |
| Inerciální senzor (IMU) | LSM6DS3 (gyroskop + akcelerometr) | I2C (0x6A) + INT1 | Měření náklonu (3D kostka) a probouzení poklepáním |
| Laserový dálkoměr | VL53L0X (Time-of-Flight) | I2C (0x29) | Přesné bezkontaktní měření vzdálenosti |
| Ultrazvukový senzor | HC-SR04 (s děličem napětí) | 2× GPIO (TRIG / ECHO) | Měření vzdálenosti odrazem ultrazvukových vln |
| Analogový IR dálkoměr | IR senzor vzdálenosti (IR1) | Analogové (ADC) | Srovnávací měření vzdálenosti v IR spektru |
| Digitální IR detektor | IR senzor překážky (IR2) | Digitální (GPIO) | Binární detekce blízké překážky |
| Barevný senzor | TCS34725 s přisvětlovací LED | Oddělené I2C (0x29) + PWM | Snímání RGB složek barvy přiložených předmětů |
| Snímače osvětlení | 2× fotorezistor (dělič 10 kΩ) | 2× Analogové (ADC) | Měření intenzity okolního osvětlení ze dvou stran |
| Uživatelské tlačítko | Mikrospínač INFO | Digitální (GPIO) | Nápověda k aktuálnímu módu a probuzení ze spánku |

**Tabulka 2-3: Přehled komponent spodního panelu (ESP32-WROOM-32) a napájecí části**


| **Komponenta** | **Typ / modul** | **Rozhraní** | **Účel v kufříku** |
| --- | --- | --- | --- |
| Řídicí mikrokontrolér | ESP32-WROOM-32 (DevKitC-V4) | UART / I2C / PWM / ADC | Obsluha ovládacích prvků a řízení servomotorů |
| Sekundární displej | 0,96" OLED (SSD1306, 128×64 px) | I2C (0x3C) | Lokální stav panelu a telemetrie servomotorů |
| Analogový joystick | Dvouosý joystick s tlačítkem SW | 2× ADC + 1× GPIO | Ovládání menu, her a polohování mechanismů |
| Směrová tlačítka | 5× mikrospínač (SW1–SW5) | 5× GPIO | Křížové ovládání pro menu a hry |
| Rotační enkodér | Inkrementální enkodér (CLK, DT) | 2× GPIO | Krokové nastavování hodnot a parametrů |
| Potenciometr | Otočný lineární potenciometr | Analogové (ADC) | Plynulé nastavování úhlu klasického serva |
| Páčkové přepínače | 2× přepínač MTS-102 s LED | 2× GPIO | Přepínání funkcí (nástroj klepet / autopilot) |
| Klasické servo | MG996R (0–180°, kovové převody) | PWM | Přesné natáčení základny ramene |
| Kontinuální servo | 360° kontinuální PWM servo | PWM | Pohon lineárního posuvu s trapézovým šroubem T8 |
| Koncové spínače | 2× mikrospínač (MIN, MAX) | 2× GPIO | Bezpečnostní dorazy krajních poloh posuvu |
| Chytré sériové servo | Lewansoul LX-16A | Half-Duplex UART | Ovládání klepet se zpětnou telemetrií a AutoStop |
| RGB LED pásek | 8× adresovatelná LED WS2812B | 1× GPIO (digitální) | Spodní podsvícení a světelné efekty |
| Napájení a měření | 12 V DC, V/A metr DSN-VC288, 2× LM2596 | Silové větve 12 V / 5 V / 6,5 V | Stabilizované napájení a živé měření spotřeby |

### 2.3 Vybrané zajímavé komponenty

##### 2.3.1 2,8" barevný TFT displej (ST7789)

Hlavním zobrazovacím prvkem celého kufříku je 2,8palcový barevný TFT LCD displej s rozlišením 240×320 obrazových bodů a řadičem ST7789. Slouží jako hlavní uživatelské rozhraní pro výběr režimů, živé vykreslování grafů ze senzorů i pro her. Na rozdíl od menších znakových displejů využívá rychlejší sběrnici SPI, protože musí v krátkém čase přenášet velké objemy obrazových dat. Přestože je modul z výroby vybaven i odporovou dotykovou vrstvou, v kufříku ji záměrně nevyužívám – zabralo by to GPIO piny a bylo složité na naprogramování.

Při samotném programování grafiky a her jsem však narazil na zásadní problém s problikáváním a trháním obrazu, který mě přiměl nastudovat, jak přenos obrazu na TFT displej vlastně probíhá. Každý pixel je přenášen v 16bitovém barevném formátu (RGB565, tedy 2 bajty na bod), což znamená, že jeden celý snímek má velikost 320 × 240 × 2 = 153 600 bajtů (přibližně 153,6 KB). Při přímém kreslení na displej procesor nejprve poslal po sběrnici SPI příkaz ke smazání celé plochy a teprve potom dokresloval grafické objekty postupně. Lidské oko však stihlo zachytit krátký mezistav s prázdnou obrazovkou, což způsobovalo nepříjemné blikání.

Tento nedostatek jsem vyřešil metodou zvanou Double Buffering (dvojitý obrazový buffer). V paměti mikrokontroléru ESP32-S3 jsem vytvořil virtuální plátno o velikosti celého displeje, do kterého se celý nový snímek bleskově vykreslí nejprve pouze v paměti RAM. Až ve chvíli, kdy je obraz kompletní, odešle se na fyzický displej jako jeden plynulý datový blok. Tím problikávání zcela zmizelo a zároveň se naplno využila velká operační paměť zvoleného čipu ESP32-S3.

Při programování displeje bylo nejprve potřeba pochopit, jak přenos obrazu do modulu vůbec probíhá (viz Obrázek X). Mezi mikrokontrolérem ESP32-S3 a samotným sklem displeje sedí integrovaný řadič ST7789 s vlastní obrazovou pamětí (GRAM), do které se vejde celý snímek o velikosti 320×240 pixelů. Každý pixel nese 16bitovou barvu ve formátu RGB565 (2 bajty na bod), takže jeden celý snímek představuje přenos 153 600 bajtů (přes 1,2 milionu bitů). Procesor po sériové sběrnici SPI nejprve pomocí řídicího vodiče D/C (přepínač příkaz/data) vymezí obdélníkové okno a následně po jediném datovém vodiči v rytmu hodinového signálu SCK postupně odešle jednotlivé barvy. Řadič ST7789 tyto bajty ukládá do své paměti GRAM a z ní pak zcela nezávisle řádek po řádku budí matici tenkovrstvých TFT tranzistorů přímo na skle displeje.

Právě doba nutná pro přenos takového množství dat po sériové lince (desítky milisekund na celý snímek) způsobovala při přímém kreslení her nepříjemné problikávání. Pokud program nejprve smazal celou obrazovku na barvu pozadí a teprve potom posílal další příkazy pro vykreslení objektů, v paměti GRAM displeje byl mezitím krátký okamžik uložen jen prázdný podklad a řadič jej zobrazil na skle. Proto jsem pro plynulý obraz využil techniku Double Buffering – celý nový snímek se nejprve připraví v operační paměti RAM čipu ESP32-S3 a do paměti GRAM displeje se následně odešle najednou jako jediný hotový blok.






//////////////////////////////////

```
### 2.3.2 3místný 7segmentový displej s posuvnými registry 74HC595
```
**Čemu se věnovat (odrážky):**

- K čemu v kufru slouží (rychlé zobrazení hodnot, vzdálenosti, skóre u her).
- Jak funguje uvnitř (74hc595\_7segment/README.md): princip **8bitového posuvného registru (SIPO)** – sériově po 1 bitu dovnitř, hodinový signál posouvá, signál **Latch (západka)** překlopí všech 8 bitů najednou na výstupy; **kaskádové řetězení (Daisy Chaining)** přes pin `Q7'`, díky kterému 3 vodiče ovládají všech 24 segmentů tří číslic.
- Výhoda a osobní zkušenost: **statické buzení** (nebliká a po zápisu nezatěžuje procesor jako multiplexování) + zjištění rozdílu mezi **společnou anodou a společnou katodou** (logika rozsvícení `0` vs. `1` při tvorbě hexadecimální tabulky znaků) a kolize pinu `GPIO 17`, kdy bylo nutné přesunout `LATCH` na bezpečný `GPIO 46`.

**Delší nástřel textu:**

Jako doplňkový výstup pro rychlé zobrazení číselných hodnot nebo skóre jsem na horní panel umístil třímístný sedmisegmentový LED displej řízený trojicí obvodů 74HC595. S tímto typem řízení jsem se při práci na kufříku setkal poprvé a zpětně ho hodnotím jako velmi užitečnou zkušenost pro pochopení číslicové techniky. Každá ze tří číslic se skládá ze sedmi segmentů (označených A až G) a desetinné tečky, což dohromady představuje 24 samostatných LED diod. Připojit každou z nich na vlastní pin mikrokontroléru by bylo naprosto neefektivní.

Řešením jsou právě osmibitové sériově-paralelní posuvné registry 74HC595 zapojené za sebou do kaskády. Procesor do prvního čipu posílá bity postupně po jediném datovém vodiči v rytmu hodinového signálu. Jakmile do čipu dorazí devátý bit, první bit přeteče přes speciální výstupní pin `Q7'` do dalšího registru v řadě. Po odeslání všech tří bajtů vyšle mikrokontrolér krátký pulz na řídicí pin západky (*Latch*), čímž se všechna data naráz překlopí na výstupní piny k jednotlivým segmentům. K řízení všech 24 LED tak stačí pouhé tři vodiče z ESP32. Velkou výhodou oproti klasickému multiplexování (kde procesor musí číslice neustále dokola přepínat) je tzv. statické buzení – jakmile se číslo pošle, registry jeho stav drží samy bez další zátěže procesoru a displej vůbec nebliká.

Z praktického hlediska mě při programování překvapily dvě věci. Nejprve bylo potřeba zjistit, že tento modul využívá zapojení se společnou anodou, takže se jednotlivé segmenty rozsvěcují logickou nulou namísto jedničky a tabulku znaků v binárním zápisu bylo nutné invertovat. Druhou zkušeností bylo zjištění kolize pinů během zapojování – původně zvolený pin pro signál *Latch* (`GPIO 17`) byl potřeba uvolnit pro hardwarovou komunikaci UART mezi panely, takže jsem jej přemapoval na bezpečný `GPIO 46`.

---

```
### 2.3.3 Inerciální senzor LSM6DS3 (gyroskop a akcelerometr)
```
**Čemu se věnovat (odrážky):**

- Co to je a k čemu slouží (6osý IMU na sběrnici I2C: naklánění víka, 3D kostka, detekce otřesu a **probouzení dotykem / poklepáním**).
- Jak funguje uvnitř (MEMS princip: akcelerometr měří lineární zrychlení včetně gravitace podle výchylky mikroskopické hmoty; gyroskop měří úhlovou rychlost pomocí Coriolisovy síly na kmitajících strukturách) + proč nestačí jen jeden z nich (drift gyroskopu vs. citlivost akcelerometru na otřesy →\rightarrow→ **komplementární filtr**).
- Zkušenost ze složky `problemy` (08\_usporny\_rezim\_light\_sleep\_a\_wake\_up.md): interní hardwarový filtr v čipu odečítá gravitaci 1 g1\text{ g}1 g a posílá impuls na pin `INT1` při poklepání na víko; konkrétní **problémy při ladění**:
    - knihovna nejdřív posílala na `INT1` signál *Data-Ready* 100× za sekundu, takže se ESP32 po uspání okamžitě samo probudilo (nutnost přímého zápisu do registrů `TAP_CFG` a `WAKE_UP_THS`),
    - původně zvolený pin `GPIO 36` způsobil okamžitý pád systému (`TG1WDT`), protože u `ESP32-S3` s Octal PSRAM je blok pinů 33–37 vyhrazen interní paměti →\rightarrow→ přesunuto na `GPIO 45`.

**Delší nástřel textu:**

Pro snímání náklonu a otřesů víka kufříku jsem zvolil šestiosý inerciální senzor (IMU) LSM6DS3 připojený po sběrnici I2C. Tento miniaturní MEMS obvod spojuje v jednom pouzdře tříosý akcelerometr a tříosý gyroskop. Z pohledu vnitřního principu pracuje akcelerometr s mikroskopickou pružně zavěšenou hmotou, jejíž výchylka při zrychlení mění elektrickou kapacitu a umožňuje měřit i stálý směr zemské gravitace. Naproti tomu gyroskop využívá kmitající mikromechanické struktury a působení Coriolisovy síly při rotaci, čímž velmi rychle a přesně snímá úhlovou rychlost otáčení.

Při implementaci vizuálního módu s rotující 3D kostkou se rychle ukázalo, proč se v praxi musí kombinovat oba senzory najednou. Samotný gyroskop reaguje okamžitě a plynule, ale při průběžném sčítání naměřených úhlů se v čase kumuluje chyba a obraz postupně ujíždí (tzv. *drift*). Akcelerometr sice dlouhodobě spolehlivě ví, kde je směr dolů k zemi, ale při každém sebemenším chvění produkuje silný šum. Proto jsem v kódu implementoval komplementární filtr, který s velkou váhou (98 %) věří rychlému gyroskopu a zbylými 2 % jej plynule koriguje podle gravitace z akcelerometru.

Největší výhodou tohoto čipu je však jeho interní hardwarový koprocesor napojený na přerušovací pin `INT1`, který jsem využil k probouzení kufříku ze spánku dotykem či lehkým poklepáním na víko. Senzor si sám pomocí vestavěného filtru odečte stálou tíhovou sílu (1 g1\text{ g}1 g) a hlídá pouze náhlé rázy. Cesta ke spolehlivému probuzení ale nebyla bez komplikací: zpočátku použitá knihovna na pin `INT1` omylem posílala signál o nových datech stokrát za sekundu, což procesor okamžitě po uspání znovu budilo – musel jsem proto nastavit práh citlivosti přímým zápisem do I2C registrů senzoru (`TAP_CFG`, `WAKE_UP_THS`). Navíc při prvním zapojení na pin `GPIO 36` systém havaroval a neustále se restartoval, protože u verze ESP32-S3 s Octal PSRAM jsou piny 33 až 37 obsazeny interní sběrnicí paměti; přerušení jsem proto přesunul na bezpečný pin `GPIO 45`.

---

```
### 2.3.4 Barevný RGB senzor TCS34725
```
**Čemu se věnovat (odrážky):**

- K čemu slouží (snímání barvy přiložených předmětů a přenos na LED pásek i displej).
- Jak funguje uvnitř (pole fotodiod s filtry R, G, B + nefiltrovaný kanál *Clear* pro celkový jas, plus **integrovaný IR filtr**, který odřízne neviditelné infračervené záření, aby barvy odpovídaly lidskému oku).
- Zkušenost z praxe (led\_strip\_color\_sensor/README.md + `config.h`): nutnost **normalizace podle kanálu Clear** + PWM řízení bílé přisvětlovací LED (bez přisvícení nebo normalizace se barvy měnily se vzdáleností předmětu), a technický problém se **shodnou pevnou I2C adresou `0x29`** jako u laserového dálkoměru `VL53L0X` →\rightarrow→ řešení oddělením na druhou I2C sběrnici ESP32-S3.

**Delší nástřel textu:**

Další součástkou, se kterou jsem se v praxi seznámil poprvé, je digitální barevný senzor TCS34725. Na rozdíl od jednoduchých fotorezistorů, které měří pouze celkovou intenzitu dopadajícího světla, obsahuje tento čip matici fotodiod překrytou červeným, zeleným, modrým a čirým (tzv. *Clear*) optickým filtrem. Důležitým konstrukčním prvkem uvnitř čipu je navíc integrovaný filtr infračerveného záření (IR). Běžné křemíkové fotodiody jsou na infračervenou složku velmi citlivé, což by bez tohoto filtru výrazně zkreslovalo naměřené odstíny oproti tomu, jak barvu reálně vnímá lidské oko.

Pro spolehlivé měření je deska modulu vybavena i bílou přisvětlovací LED diodou, jejíž jas v kufříku reguluji pomocí PWM signálu, aby ve tmě neoslňovala návštěvníky a přitom zajistila stabilní podmínky pro odraz světla od měřeného předmětu. Během testování se ukázalo, že pouhé přečtení hodnot R, G a B nestačí – při oddálení předmětu od senzoru všechny hodnoty prudce klesnou. Proto je v programu vždy nutná normalizace každé barevné složky vůči celkovému jasu z kanálu *Clear*, díky čemuž zůstává rozpoznaná barva stabilní.

Z pohledu zapojení bylo zajímavým zjištěním, že senzor TCS34725 má z výroby pevně zakódovanou I2C adresu `0x29`, kterou nelze změnit. Naprosto stejnou výchozí adresu má ale i laserový senzor vzdálenosti VL53L0X osazený na stejném panelu. Aby nedocházelo ke kolizi na jedné sběrnici, využil jsem toho, že mikrokontrolér ESP32-S3 disponuje dvěma nezávislými hardwarovými I2C řadiči, a pro barevný senzor jsem vytvořil oddělenou druhou sběrnici.

---

```
### 2.3.5 Chytré sériové servo Lewansoul LX-16A
```
**Čemu se věnovat (odrážky):**

- K čemu slouží (pohon klepet na spodním panelu) a v čem se liší od běžného PWM serva (`MG996R` na stejném panelu).
- Jak funguje uvnitř: vestavěná řídicí elektronika se zpětnovazebním potenciometrem, čidlem teploty a napětí; komunikace pomocí paketů po **jednovodičové Half-Duplex UART sběrnici** (stejný drát posílá příkaz i přijímá odpověď, každé servo má vlastní ID).
- Zkušenost z praxe (lx16a\_servo/README.md + `HardwareSetup.cpp`):
    - možnost naprogramovat funkci **AutoStop** (jakmile se klepeta sevřou kolem kostky a rozdíl požadovaného a reálného úhlu přesáhne 10∘10^\circ10∘, servo se samo vypne, takže se nespálí jako běžné PWM servo),
    - praktické úskalí s napájením (při zátěži na 5 V z USB se mikrokontrolér resetoval proudovou špičkou →\rightarrow→ nutnost samostatné 6,5V větve) a nutnost prvotní konfigurace ID serva a inicializace UART před PWM časovači.

**Delší nástřel textu:**

Jako hlavní akční člen mechanismu klepet na spodním panelu jsem vybral chytré sériové servo Lewansoul LX-16A. Běžná modelářská serva (jako je druhé použité servo MG996R) jsou řízena pouze jednosměrným PWM signálem – mikrokontrolér zadá požadovaný úhel, ale vůbec neví, zda se hřídel do dané polohy skutečně dostala nebo zda do něčeho nenarazila, což může vést k přetížení a spálení motoru.

Chytré servo LX-16A tento problém řeší vlastní integrovanou řídicí elektronikou a obousměrnou paketovou komunikací přes jednovodičovou sběrnici *Half-Duplex UART*. Po jediném datovém kabelu tak v prvním okamžiku putuje příkaz z ESP32 k servu a vzápětí se směr přenosu otočí a servo pošle zpět svou aktuální telemetrii – tedy skutečně změřený úhel natočení, teplotu elektroniky i napájecí napětí. Díky adresování pomocí unikátního identifikačního čísla (ID) lze navíc na jediný datový pin řetězit více serv za sebou.

Právě schopnost zpětné vazby mi umožnila do kufříku naprogramovat bezpečnostní funkci *AutoStop*. Pokud klepeta při zavírání narazí na předmět a odchylka mezi požadovaným a reálným úhlem přesáhne 10∘10^\circ10∘ po dvě po sobě jdoucí měření, program servo okamžitě zastaví, čímž zabrání mechanickému poškození či přehřátí. Během prvních testů jsem se také přesvědčil o vysokých proudových špičkách tohoto pohonu – při pokusu o napájení z 5V USB větve docházelo pod zátěží k poklesu napětí a restartu ESP32, a proto je v kufru odděleně napájeno samostatným měničem na 6,5 V. Zároveň bylo nutné před použitím napsat jednoduchou inicializační rutinu, která si servo na sběrnici vyhledá a nastaví mu správné pracovní ID a hardwarové limity úhlu přímo do jeho paměti.


