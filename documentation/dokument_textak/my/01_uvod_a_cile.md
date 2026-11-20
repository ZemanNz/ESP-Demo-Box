# Zadání

Hlavním cílem této maturitní práce je navrhnout, zkonstruovat, elektricky zapojit a naprogramovat přenosný výukový a prezentační kufřík ESP-Demo-Box.

Pro splnění hlavního cíle jsem si stanovil následující dílčí cíle:

- **Mechanická konstrukce:** Vymodelovat v CAD programu Onshape systém upevnění všech součástek s ohledem na modularitu během sestavení. Tyto díly vytisknout na 3D tiskárně, osadit součástkami a upevnit ke kostře kufru.
- **Elektrický návrh:** Navrhnout přehledné elektrické schéma celého zařízení v programu KiCad, rozdělit napájecí větve, vytvořit rozšiřující propojovací desky (shieldy) pro oba mikrokontroléry ESP32 a propojit periferie pomocí vodičů typu DuPont.
- **Vytvoření softwaru:** Zprovoznit všechny připojené periferie a připravit pro ně samostatné ukázkové příklady. Navrhnout architekturu a hierarchii řízení mezi dvěma mikrokontroléry (horním a spodním panelem) a zprovoznit mezi nimi spolehlivou sériovou komunikaci (UART). Následně naprogramovat jednotlivé demonstrační módy, hry s autopilotem, webové rozhraní a celkové ovládání kufříku.

# Seznam symbolů a zkratek

**Symboly**

| Označení | Legenda | Jednotka |
| --- | --- | --- |
| f | Frekvence | [Hz] |
| I | Elektrický proud | [A] |
| P | Elektrický výkon | [W] |
| R | Elektrický odpor | [Ω] |
| t | Čas | [s] |
| T | Teplota | [°C] |
| U | Elektrické napětí | [V] |
| η | Účinnost | [%] |
| φ | Relativní vlhkost vzduchu | [%] |

**Zkratky**

| Označení | Legenda |
| --- | --- |
| ADC | Analogově-digitální převodník (Analog-to-Digital Converter) |
| BLE | Nízkoenergetický Bluetooth (Bluetooth Low Energy) |
| CAD | Počítačem podporované navrhování (Computer-Aided Design) |
| CSI | Informace o stavu kanálu Wi-Fi (Channel State Information) |
| DC | Stejnosměrný proud / napětí (Direct Current), u displeje řídicí pin Data/Command |
| GPIO | Univerzální vstupně-výstupní pin (General-Purpose Input/Output) |
| GRAM | Vnitřní grafická paměť řadiče displeje (Graphics Random-Access Memory) |
| GUI | Grafické uživatelské rozhraní (Graphical User Interface) |
| I2C | Sériová dvouvodičová sběrnice (Inter-Integrated Circuit) |
| IMU | Inerciální měřicí jednotka (Inertial Measurement Unit) |
| IR | Infračervené záření (Infrared) |
| LCD | Displej z tekutých krystalů (Liquid Crystal Display) |
| LED | Světelná dioda (Light-Emitting Diode) |
| MEMS | Mikroelektromechanické systémy (Micro-Electro-Mechanical Systems) |
| MOSI | Datová linka SPI z nadřízeného do podřízeného zařízení (Master Out Slave In) |
| OLED | Displej s organickými světelnými diodami (Organic Light-Emitting Diode) |
| PCB | Deska plošných spojů (Printed Circuit Board) |
| PSRAM | Externí pseudostatická paměť RAM (Pseudo-Static Random-Access Memory) |
| PWM | Pulzně šířková modulace (Pulse-Width Modulation) |
| RAM | Operační paměť s přímým přístupem (Random-Access Memory) |
| RGB | Barevný model červená, zelená, modrá (Red, Green, Blue) |
| SCK | Hodinový signál sériové sběrnice (Serial Clock) |
| SPI | Sériové periferní rozhraní (Serial Peripheral Interface) |
| SRAM | Statická paměť RAM (Static Random-Access Memory) |
| TFT | Tenkovrstvý tranzistor (Thin-Film Transistor) |
| ToF | Měření vzdálenosti z doby letu paprsku (Time-of-Flight) |
| UART | Univerzální asynchronní přijímač a vysílač (Universal Asynchronous Receiver-Transmitter) |
| USB | Univerzální sériová sběrnice (Universal Serial Bus) |

# 1 Úvod

Mikrokontroléry, senzory a ovládací prvky dnes tvoří základ většiny moderních elektronických zařízení – od elektrického zubního kartáčku přes mikrovlnnou troubu až po autonomní auta. Aby však tyto součástky spolehlivě plnily svůj účel, je nutné nejen správně navrhnout hardwarové osazení či elektrické zapojení, ale také způsob získávání dat a práce s nimi.

Téma návrhu a sestrojení výukového a prezentačního kufříku s čipy ESP32 jsem si pro svou maturitní práci zvolil kvůli dlouhodobému zájmu o mikrokontroléry, elektroniku a software. Předlohou mé maturitní práce je bakalářská práce z Vysokého učení technického v Brně (VUT). Vytvořený demokit zde sloužil jako prezentační projekt používaný na dnech otevřených dveří.

Na základě této předlohy vznikl nápad vytvořit pro robotický kroužek a školu vlastní, technologicky pokročilejší a interaktivnější kufřík, který bude lehce přenositelný a po připojení okamžitě funkční. Své využití najde na dnech otevřených dveří či na veletrzích středních škol. Kufřík nemá sloužit pouze na prezentaci, ale také jako ukázka využití různých periferií, jejichž zapojení a kód jsou zdokumentovány. Vedlejším cílem je prohloubit mé znalosti v oblasti elektrotechniky a robotiky.
