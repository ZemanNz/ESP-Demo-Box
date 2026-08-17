# Problém: Zarovnání paměti v binárním UART protokolu (Memory Alignment & Padding)

## Popis problému
Při návrhu mezideskové komunikace mezi dvěma mikrokontroléry (ESP32-S3 a ESP32-WROOM) bylo rozhodnuto přenášet stavové a příkazové balíčky pomocí přímého binárního přenosu C++ struktur přes sběrnici UART (`Serial.write((uint8_t*)&packet, sizeof(packet))`).

Při odesílání struktury obsahující smíšené datové typy (např. 1-bajtový `bool`, 2-bajtový `int16_t` a 4-bajtový `int32_t`) docházelo k chybám v interpretaci dat na straně příjemce – hodnoty proměnných byly posunuté o několik bajtů, tlačítka hlásila nesmyslné stavy a kontrolní součty neseděly.

---

## Příčina problému: Automatický Memory Padding 32bitového procesoru
Architektura procesorů Xtensa (ESP32) je 32bitová. Procesor pro maximalizaci rychlosti přístupu do paměti RAM vyžaduje, aby proměnné začínaly na paměťových adresách, které jsou násobkem jejich velikosti (tzv. **Data Alignment**):
* 1bajtový `bool` / `uint8_t` může začínat na libovolné adrese.
* 2bajtový `int16_t` musí začínat na sudé adrese (násobek 2).
* 4bajtový `int32_t` / `float` musí začínat na adrese dělitelné 4.

Pokud máme ve struktuře definované položky za sebou:
```cpp
struct NesrovnanaStruktura {
    uint8_t  startByte; // 1 bajt (adresa 0x00)
    // --> ZDE KOMPILÁTOR VLOŽÍ 3 NEVIDITELNÉ VÝPLŇOVÉ BAJTY (Padding)!
    int32_t  encoderPos; // 4 bajty (adresa 0x04)
    bool     btnState;   // 1 bajt (adresa 0x08)
    // --> ZDE KOMPILÁTOR VLOŽÍ 1 VÝPLŇOVÝ BAJT!
    uint16_t potValue;   // 2 bajty (adresa 0x0A)
};
```

### Důsledky pro UART přenos:
1. **Přenášení smetí:** `sizeof(NesrovnanaStruktura)` není $1 + 4 + 1 + 2 = 8$ bajtů, ale **12 bajtů**. Po sériové lince se tak posílají náhodné výplňové bajty z paměti RAM.
2. **Nekompatibilita:** Pokud by na druhém procesoru kompilátor zarovnal položky odlišně (např. jiná verze GCC nebo jiná architektura), data se posunou a hodnota `potValue` se přečte z výplňových bajtů namísto skutečných dat.

---

## Řešení: Vynucení 1bajtového zarovnání pomocí `#pragma pack(push, 1)`

Kompilátoru bylo explicitně nařízeno zakázat vkládání výplňových bajtů pro komunikační struktury v souboru `UartProtocol.h`.

```cpp
// 1. Uložíme stávající nastavení a nastavíme zarovnání na těsný 1 bajt
#pragma pack(push, 1)

struct TopToBottomPacket {
    uint8_t  startByte;              // 1 bajt
    uint8_t  currentMode;            // 1 bajt
    bool     overrideAutonomy;       // 1 bajt
    int16_t  targetSmartServoAngle;  // 2 bajty
    uint8_t  targetServoAngle;       // 1 bajt
    int8_t   targetContinuousServo;  // 1 bajt
    int16_t  targetMotorSpeed;       // 2 bajty
    uint32_t ledStrip[8];            // 32 bajtů
    uint8_t  ledBrightness;          // 1 bajt
    char     oledLine1[17];          // 17 bajtů
    char     oledLine2[17];          // 17 bajtů
    uint8_t  checksum;               // 1 bajt
    uint8_t  endByte;                // 1 bajt
};

// 2. Vrátíme standardní optimalizované zarovnání pro zbytek programu
#pragma pack(pop)
```

### Proč `#pragma pack` místo `__attribute__((packed))`?
Oba zápisy vedou ke stejnému výsledku, ale direktiva `#pragma pack(push, 1)` je standardizovaná napříč všemi moderními C/C++ kompilátory (GCC, Clang, MSVC), zatímco `__attribute__((packed))` je specifické rozšíření kompilátoru GCC.

---

## Ochrana proti posunu dat (Resynchronizace přes `peek()`)

Pro případ, že by se během běhu zařízení na lince ztratil bajt (např. odpojení konektoru), byla na straně přijímače implementována synchronizační logika:
```cpp
while (SerialESP.available() >= sizeof(BottomToTopPacket)) {
    // Podíváme se na první bajt v bufferu bez jeho smazání
    if (SerialESP.peek() != UART_FRAME_START_BOTTOM_TO_TOP) {
        SerialESP.read(); // Zahodíme vadný bajt a hledáme začátek 0x55
        continue;
    }
    // Načteme přesně definovaný počet bajtů přímo do struktury
    SerialESP.readBytes((uint8_t*)&inPacket, sizeof(BottomToTopPacket));
    ...
}
```

---

## Výsledný přínos pro projekt
* **100% předvídatelná velikost paketu:** Velikost odpovídá přesnému součtu proměnných.
* **Maximální propustnost:** Žádné přenášení výplňových bajtů po lince.
* **Stabilita:** Odolnost proti posunu rámců a okamžitá resynchronizace linky.
