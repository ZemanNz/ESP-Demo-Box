# Jak funguje WebSocket a Fyzika Wi-Fi Přenosu (ESP-Demo-Box)

Tento dokument vysvětluje **WebSocket i fyzikální podstatu Wi-Fi přenosu od naprostých základů**: proč vznikl, jak se inicializuje na ESP32 i na mobilu, jak fungují rádiové vlny na 2.4 GHz a jak probíhá balení, přenos a dekódování dat.

---

## 1. Příběh WebSocketu: Proč a jak vlastně vznikl?

### Starý svět (HTTP požadavky / 1995–2005)
Když vznikal internet, webové stránky byly statické (jako papírové noviny). Prohlížeč poslal dotaz (*"Dej mi stránku"*), server odpověděl a **spojení se okamžitě ukončilo**.

Když v letech 2005+ vznikly živé chatovací aplikace nebo burzovní grafy, vývojáři museli používat škaredý trik zvaný **AJAX Polling**:
> Mobil se každou vteřinu dokola ptal serveru: *"Máš novou zprávu? ... Máš novou zprávu?"*  
> Server 99 % času odpovídal: *"Ne, nemám ... Ne, nemám ..."*

Tento způsob hrozně přetěžoval síť i servery, protože každá otázka i odpověď vyžadovala vytvoření nového TCP spojení a odeslání stopek HTTP hlaviček (cca 500–800 bajtů).

### Vynález WebSocketu (Roku 2011 / Standard RFC 6455)
Skupina síťových inženýrů vymyslela nový protokol **WebSocket (`ws://` nebo šifrovaný `wss://`)**:
* **Myšlenka:** Zastavit neustále zaklepávání na dveře a vytvořit **trvalou otevřenou přímou linku (potrubí)** mezi klientem a serverem.
* **Jak začíná:** Spojení začne jako běžný HTTP požadavek na portu 80. Mobil ale pošle speciální přání: *"Chci upgradovat toto spojení na WebSocket."*
* **Handshake (Podání ruky):** Server odpoví síťovým kódem `101 Switching Protocols` (Přepínám protokoly).
* **Od této chvíle se spojení NEzavírá.** Zůstává otevřený plně duplexní (obousměrný) kanál nad protokolem TCP. Oba účastníci mohou kdykoliv bez ptaní poslat druhovému data. Režie zprávy klesla ze 700 bajtů na **pouhé 2 bajty**!

---

## 2. Jak se WebSocket inicializuje (ESP32 vs. Mobil)

```
+------------------------------------+          +------------------------------------+
|       ESP32 (C++ Backend)          |          |     MOBIL (JavaScript Frontend)    |
|                                    |  Wi-Fi   |                                    |
| 1. AsyncWebSocket ws("/ws");       | <======> | 1. ws = new WebSocket("ws://..."); |
| 2. ws.onEvent(onWebSocketEvent);   | WebSocket| 2. ws.onmessage = handleTelemetry; |
+------------------------------------+          +------------------------------------+
```

### A) Inicializace na ESP32 (C++ / `WebManager.cpp`)
1. **Deklarace objektu (`WebManager.h`):**
   ```cpp
   AsyncWebSocket ws("/ws"); // Vytvoříme objekt WebSocketu naslouchající na adrese "/ws"
   ```
2. **Registrace obsluhy událostí (`WebManager.cpp`):**
   ```cpp
   void WebManager::setupWebSocket() {
       // Říkáme ESP32: Kdykoliv se na kanálu /ws cokoliv stane, zavolej naši funkci onWebSocketEvent!
       ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, 
                         AwsEventType type, void *arg, uint8_t *data, size_t len) {
           this->onWebSocketEvent(server, client, type, arg, data, len);
       });
       server.addHandler(&ws); // Připojíme WebSocket k webovému serveru na portu 80
   }
   ```

### B) Inicializace v Mobilu (JavaScript / `script.js`)
1. **Otevření spojení (`script.js`):**
   ```javascript
   const wsUrl = `ws://${location.host}/ws`; // Sestaví adresu: ws://192.168.4.1/ws
   ws = new WebSocket(wsUrl); // Mobil ihned odesílá žádost o spojení do ESP32
   ```
2. **Nastavení reaktorů na události (`script.js`):**
   ```javascript
   ws.onopen = () => { console.log("Online!"); };
   ws.onmessage = (event) => {
       const data = JSON.parse(event.data); // Rozbalíme JSON text
       handleTelemetry(data);               // Přepíšeme čísla v HTML
   };
   ws.onclose = () => { scheduleReconnect(); };
   ```

---

## 3. Fyzikální podstata přenosu: Od paměti k elektřině a rádiovým vlnám

Sledujme příklad: **ESP32 naměřilo teplotu 23.5 °C a posílá ji na mobil.**

### KROK 1: Z čísla v paměti na bajty (Nuly a jedničky)
V paměti RAM procesoru ESP32 vytvoříme textový řetězec: `"{"t":23.5}"`.  
Každý znak je v paměti zapsán pomocí 8 bitů (1 bajt):
* Znak `{` = `01111011`
* Znak `"` = `00100010`
* Znak `t` = `01110100`
* Znak `:` = `00111010`
* Znak `2` = `00110010`
* Znak `3` = `00110011`
* Znak `.` = `00101110`
* Znak `5` = `01110101`
* Znak `}` = `01111101`

V tranzistorech ESP32 znamená `0` napětí 0 V a `1` napětí 3.3 V.

---

### KROK 2: Fyzika v anténě ESP32 (Rozkmitání elektronů)
1. Síťový Wi-Fi čip vezme tyto nuly a jedničky a pustí je do vysokofrekvenčního vysílače (RF modemu).
2. Vysílač pustí do měděné antény **střídavý elektrický proud kmitající na frekvenci 2.4 GHz (2 412 000 000 kmitů za sekundu)**.
3. **Fyzikální děj v anténě:** V kovu antény jsou volné **elektrony** (záporné náboje). Střídavý proud nutí tyto elektrony v anténě kmitat sem a tam 2,4 miliardkrát za sekundu!

---

### KROK 3: Vznik Elektromagnetické vlny
Podle Maxwellových zákonů elektrodynamiky:
* Pohybující se elektrický náboj (elektron) vytváří **měnící se elektrické pole ($E$)**.
* Měnící se elektrické pole vyvolá **měnící se magnetické pole ($B$)**.
* Tato navzájem se svázaná pole se odtrhnou od antény a vyrazí do prostoru jako **ELEKTROMAGNETICKÁ VLNA**.

* **Rychlost:** Vlna letí vzduchem rychlostí světla ($300\ 000\ \text{km/s}$). Vzdálenost 2 metry uletí za neskutečných $6.7\text{ nanosekund}$.
* **Vlnová délka ($\lambda$):** Při frekvenci 2.4 GHz má jedna vlna délku celkem **12.4 cm**.

---

### KROK 4: Příjem v mobilu a dekódování
1. Vlna dorazí k mobilu a narazí do jeho kovové antény.
2. **Elektromagnetická indukce:** Vlna zatlačí na elektrony v anténě mobilu a **rozkmitá je ve stejném rytmu (2.4 GHz)**. V anténě mobilu vznikne mikro-proud.
3. Wi-Fi modem v mobilu tento mikroproud přečte, demoduluje nuly a jedničky a poskládá text `"{"t":23.5}"`.
4. JavaScript v mobilu přes `JSON.parse` přečte `23.5` a rozsvítí pixely na displeji telefonu!

---

## 4. Časté fyzikální otázky a záhady (Zajímavosti)

### ❓ Otázka 1: Jak to, že má ESP32 anténku dlouhou jen cca 3 cm, když celá vlna má 12.4 cm?
> **Odpověď:** Pro příjem a vysílání rádiového signálu nemusí mít anténa délku celé vlnové délky ($\lambda = 12.4\text{ cm}$).  
> Z fyziky rezonance se nejčastěji používá tzv. **čtvrtvlnná anténa ($\lambda / 4$)**!  
> Když spočítáme čtvrtinu z 12.4 cm:  
> $$\frac{12.4\text{ cm}}{4} = 3.1\text{ cm}$$  
> **Přesně 3.1 cm!** Délka 3.1 cm je fyzikálně ideální čtvrtvlnný rezonátor, ve kterém elektrony kmitají v nejefektivnějším rytmu.

---

### ❓ Otázka 2: Když je anténka na plošném spoji zohýbaná (klikatá / meandrová), nezinterferují ty vlny mezi sebou?
> **Odpověď:** Měděná cestička na ESP32 je klikatá (tzv. *Meander Line Antenna*), aby se 3.1 cm dlouhá anténa fyzicky vešla na malou destičku.  
> **Zatávky jsou matematicky přesně navržené!** Vzdálenosti mezi ohyby jsou spočítané tak, aby vyzařovaná elektromagnetická pole z sousedních ramen nepůsobila destruktivně (aby se navzájem nevynulovala), ale naopak se konstruktivně sčítala a vyzařovala všemi směry.

---

### ❓ Otázka 3: Když vlna letí tak extrémně rychle (rychlostí světla), jak ji mobil dokáže dekódovat?
> **Odpověď:** Vlna sice uletí 2 metry za 6.7 nanosekund, ale **přijímač v mobilu nečeká, až vlna dosvítí!**  
> Vlna přilétá jako **spojitá proudící vlna**. Jakmile dorazí k mobilu, indukuje v anténě střídavý proud kmitající na 2.4 GHz.  
> Uvnitř Wi-Fi čipu mobilu běží ultravysokorychlostní elektronické obvody (oscilátory a fázové závěsy PLL), které dokáží tento kmitající proud vzorkovat a číst skoky ve fázi a amplitudě v reálném čase kmit po kmitu. Mobil tak neposuzuje rychlost letu vlny, ale přímo **rytmus kmitání elektronů ve své vlastní anténě**.
