# 1 Úvod

Mikrokontroléry, senzory a ovládací prvky dnes tvoří základ většiny moderních elektronických zařízení – od elektrického zubního kartáčku přes mikrovlnnou troubu až po autonomní auta. Aby však tyto součástky spolehlivě plnily svůj účel, je nutné nejen správně navrhnout hardwarové osazení či elektrické zapojení, ale také způsob získávání dat a práce s nimi.

Téma návrhu a sestrojení výukového a prezentačního kufříku s čipy ESP32 jsem si pro svou maturitní práci zvolil kvůli dlouhodobému zájmu o mikrokontroléry, elektroniku a software.

## 1.1 Motivace a původ projektu

Předlohou mé maturitní práce je bakalářská práce z Vysokého učení technického v Brně (VUT). Vytvořený demokit zde sloužil jako prezentační projekt používaný na dnech otevřených dveří.

Na základě této předlohy vznikl nápad vytvořit pro robotický kroužek a školu vlastní, technologicky pokročilejší a interaktivnější kufřík, který bude lehce přenositelný a po připojení okamžitě funkční. Své využití najde jak na dnech otevřených dveří či na veletrzích středních škol.

Kufřík nemá sloužit pouze na prezentaci, ale také jako ukázka využití různých periferií, jejichž zapojení a kód jsou zdokumentovány.

## 1.2 Stanovení cílů práce

Hlavním cílem této maturitní práce je navrhnout, zkonstruovat, elektricky zapojit a naprogramovat přenosný výukový a prezentační kufřík ESP-Demo-Box.

Pro splnění hlavního cíle jsem si stanovil následující dílčí cíle:

- Mechanická konstrukce
  - Vymodelovat v CAD programu Onshape systém upevnění všech součástek s ohledem na modularitu během sestavení. Tyto díly vytisknout na 3D tiskárně, osadit součástkami a upevnit ke kostře kufru.
- Elektrický návrh
  - Navrhnout přehledné elektrické schéma celého zařízení v programu KiCad, rozdělit napájecí větve, vytvořit rozšiřující propojovací desky (shieldy) pro oba mikrokontroléry ESP32 a propojit periferie pomocí vodičů typu DuPont.
- Vytvoření softwaru
  - Zprovoznit všechny připojené periferie a připravit pro ně samostatné ukázkové příklady. Navrhnout architekturu a hierarchii řízení mezi dvěma mikrokontroléry (horním a spodním panelem) a zprovoznit mezi nimi spolehlivou sériovou komunikaci (UART). Následně naprogramovat jednotlivé demonstrační módy, hry s autopilotem, webové rozhraní a celkové ovládání kufříku.

Vedlejším cílem je prohloubit mé znalosti v oblasti elektrotechniky a robotiky.
