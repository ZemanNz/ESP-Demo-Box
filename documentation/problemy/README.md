# Dokumentace problémů a jejich řešení – Maturitní projekt ESP-Demo-Box

Tato složka slouží jako přehledný registr technických problémů, na které jsme narazili během vývoje firmware a hardware pro maturitní projekt ESP-Demo-Box, a detailní popis jejich řešení.

## Seznam zdokumentovaných problémů

| ID | Název problému / Téma | Oblast | Odkaz na detailní popis |
| :--- | :--- | :--- | :--- |
| **01** | Problikávání TFT displeje ST7789 při vykreslování her | Grafika / SPI displej | [Zobrazit řešení (Double Buffering)](problikavani_displeje_double_buffering.md) |
| **02** | Zarovnání paměti v binárním UART protokolu (Memory Padding) | Komunikace / Paměť | [Zobrazit řešení (#pragma pack)](01_zarovnani_pameti_pragma_pack.md) |
| **03** | Zabezpečení integrity dat a rušení od motorů (XOR Checksum) | Komunikace / EMI | [Zobrazit řešení (XOR Checksum)](02_integrita_dat_xor_checksum.md) |
| **04** | Konflikt autority řízení servomotorů v distribuovaném systému | Řízení / Robotika | [Zobrazit rozbor (Authority Race)](03_konflikt_autority_rizeni_serv.md) |
| **05** | Hybridní UART přenos (Okamžitá změna + Záchranný Heartbeat) | Komunikace / FreeRTOS | [Zobrazit řešení (Hybridní přenos)](04_hybridni_uart_prenos_heartbeat.md) |
| **06** | Oddělení datového stavu a fyzických ovladačů (Architecture) | Architektura C++ | [Zobrazit řešení (SystemState vs SensorManager)](05_oddeleni_systemstate_a_sensormanager.md) |
| **07** | Řízení periferií dolního panelu čistě přes virtuální stav | Architektura / Abstrakce | [Zobrazit řešení (Virtuální stav)](06_rizeni_dolniho_panelu_pres_virtualni_stav.md) |
| **08** | Architektura Master-Slave mezi dvěma mikrokontroléry ESP32 | Systémový návrh | [Zobrazit rozbor (Master-Slave ESP32)](07_architektura_master_slave_esp32.md) |

---
*Všechny dokumenty obsahují detailní technické zdůvodnění, matematické a paměťové souvislosti a zdrojové kódy přímo použitelné do textové části maturitní práce.*
