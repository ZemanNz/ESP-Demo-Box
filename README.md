# ESP-Demo-Box

Tento projekt obsahuje kompletní podklady pro vývoj a konstrukci zařízení **ESP-Demo-Box**. Projekt je rozdělen do několika hlavních částí:

## Struktura projektu

- **`documentation/`**: Uživatelská a technická dokumentace k projektu.
- **`firmware/`**: Zdrojové kódy pro mikrokontroléry (PlatformIO projekty pro desky ESP32-S3).
  - **`horni_panel/`**: Firmware pro horní panel.
  - **`spodni_panel/`**: Firmware pro spodní panel.
- **`hardware/`**: Podklady pro výrobu a konstrukci.
  - **`3D/`**: 3D modely pro tisk.
  - **`Ele/`**: Schémata zapojení a plošné spoje.

## Vývoj (Firmware)

Firmware je vyvíjen v prostředí **PlatformIO** (ideálně jako rozšíření pro VS Code).

### Požadavky
- [VS Code](https://code.visualstudio.com/) s nainstalovaným rozšířením [PlatformIO IDE](https://platformio.org/platformio-ide).

### Otevření a kompilace
1. Otevřete konkrétní složku projektu ve `firmware/` (např. `horni_panel` nebo `spodni_panel`) v VS Code.
2. PlatformIO automaticky stáhne potřebné závislosti a nástroje pro ESP32-S3.
3. Pro sestavení projektu použijte tlačítko **Build** (ikonka zaškrtnutí) v dolní liště PlatformIO.
4. Pro nahrání firmwaru připojte desku přes USB a klikněte na **Upload** (ikonka šipky).
