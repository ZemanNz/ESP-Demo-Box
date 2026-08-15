# Problém: Vytížení sběrnice a latence při řízení (Edge Computing)

## Popis situace
V tradičním pojetí Master-Slave architektury funguje Slave (spodní panel) pouze jako pasivní sběrač dat. Neustále čte všechny senzory, joysticky a tlačítka a nepřetržitě je odesílá Masteru (hornímu panelu). Master data vyhodnotí a pošle zpět příkazy (např. "Otoč servem o 5 stupňů").

Tento přístup generuje závažné problémy:
1. **Zahlcení UART sběrnice:** Neustálé posílání stovek zpráv za vteřinu zatěžuje sběrnici zbytečným balastem i v době, kdy zařízení například jen "stojí" v menu.
2. **Vysoká latence (Zpoždění):** Dráha signálu (Potenciometr -> Slave -> UART -> Master -> Zpracování -> UART -> Slave -> Motor) způsobuje nechtěné záseky a prodlevy v plynulém pohybu.

## Architektonické řešení: Distribuovaná logika a obousměrná vazba
Systém nevyužívá spodní desku jen jako klávesnici, ale přesouvá část rozhodovací logiky přímo k senzorům a motorům – tento princip se označuje jako **Edge Computing**.

Toto je umožněno díky obousměrné (Full-Duplex) UART komunikaci. Kdykoliv dojde na horním (Master) panelu ke změně globálního stavu (např. uživatel vybere režim "Test motorů"), Master odešle tento stav dolů.

### Přínosy řešení v praxi:
* **Filtrování dat (Odlehčení sběrnice):** Pokud spodní panel (díky zprávě z vrchu) ví, že se systém nachází v režimu "Hlavní Menu", záměrně přestane odesílat data z analogového joysticku, protože ví, že by je Master stejně zahodil.
* **Hardwarová akcelerace s nulovou latencí:** Jakmile systém přejde do režimu "Test motorů", spodní panel začne interně mapovat vstup z potenciometru přímo na PWM výstup motoru. Řídicí smyčka běží lokálně na spodním čipu. K hornímu čipu se přes UART posílá už jen "informační občasník" o poloze serva za účelem překreslení grafiky na displeji. Zpoždění ovládání motorů je tak zcela eliminováno.
* **Autonomie OLED displeje:** Spodní panel může dynamicky měnit instrukce na svém lokálním displeji přesně na základě toho, jaký stav mu nadiktoval Master, čímž zlepšuje celkový UX (User Experience).
