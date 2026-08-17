#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <Arduino.h>

// =============================================================================
//  DEFINICE MAGICKÝCH BAJTŮ PRO SYNCHRONIZACI RÁMCŮ
// =============================================================================
#define UART_FRAME_START_TOP_TO_BOTTOM    0xAA
#define UART_FRAME_START_BOTTOM_TO_TOP    0x55
#define UART_FRAME_END                    0xFE

// Zajistíme přesné 1-bajtové zarovnání v paměti bez výplňových bajtů (padding)
#pragma pack(push, 1)

// =============================================================================
//  1. PAKET: SHORA DOLŮ (TopToBottomPacket)
//  Horní panel posílá dolnímu: Mód, příkazy pro serva, motor, LED pásek i OLED
// =============================================================================
struct TopToBottomPacket {
    uint8_t  startByte;              // 0xAA
    uint8_t  currentMode;            // Aktuální AppMode (např. MODE_GAME_SNAKE, MODE_SERVA, ...)
    bool     overrideAutonomy;       // true = horní panel plně řídí periferie; false = dolní panel má autonomii

    // Příkazy pro serva a motor
    int16_t  targetSmartServoAngle;  // Požadovaný úhel chytrého serva LX-16A (0 až 240°)
    uint8_t  targetServoAngle;       // Požadovaný úhel klasického serva (0 až 180°)
    int8_t   targetContinuousServo;  // Požadovaná rychlost kontinuálního serva (-100 až +100 %)
    int16_t  targetMotorSpeed;       // Požadovaná rychlost motoru (0 až 255 PWM)

    // Příkaz pro 8x RGB LED pásek na dolním panelu
    uint32_t ledStrip[8];            // 24-bitové RGB barvy pro každou z 8 LED
    uint8_t  ledBrightness;          // Jas dolního pásku (0 až 255)

    // Text pro 0.96" OLED displej na dolním panelu (max 16 znaků na řádek + \0)
    char     oledLine1[17];
    char     oledLine2[17];

    uint8_t  checksum;               // Kontrolní součet (XOR všech předchozích bajtů paketu)
    uint8_t  endByte;                // 0xFE
};

// =============================================================================
//  2. PAKET: ZDOLA NAHORU (BottomToTopPacket)
//  Dolní panel posílá nahoru: Úplně všechny vstupy, přepínače i reálný stav serv
// =============================================================================
struct BottomToTopPacket {
    uint8_t  startByte;              // 0x55

    // Analogový Joystick (X, Y) a stisk tlačítka
    int16_t  joyX;                   // 0 až 4095
    int16_t  joyY;                   // 0 až 4095
    bool     joyBtn;                 // true = stisknuto

    // 5x Tlačítka na dolním panelu
    bool     btnDown[5];             // Tlačítka 1 až 5 (true = stisknuto)

    // Rotační enkodér
    int32_t  encoderPos;             // Absolutní pozice
    int16_t  encoderDelta;           // Změna od posledního paketu
    bool     encoderBtn;             // true = stisknuto

    // Potenciometr
    uint16_t potentiometer;          // 0 až 4095

    // Reálné stavy / zpětná vazba z akčních členů dolního panelu
    int16_t  currentSmartServoAngle; // Skutečně načtený úhel z chytrého serva LX-16A
    uint8_t  currentServoAngle;      // Aktuální úhel klasického serva
    int8_t   currentContinuousServo; // Aktuální stav kontinuálního serva
    int16_t  currentMotorSpeed;      // Aktuální rychlost motoru

    // Stav dolního 8x RGB LED pásku
    uint32_t ledStrip[8];            // Aktuální barvy svítící na dolním pásku
    uint8_t  ledBrightness;          // Aktuální jas

    // 2x Páčkové přepínače (Switche)
    bool     switch1;                // true = ON (HIGH)
    bool     switch2;                // true = ON (HIGH)

    uint8_t  checksum;               // Kontrolní součet (XOR všech předchozích bajtů paketu)
    uint8_t  endByte;                // 0xFE
};

#pragma pack(pop)

// =============================================================================
//  POMOCNÉ FUNKCE PRO VÝPOČET KONTROLNÍHO SOUČTU (CHECKSUM)
// =============================================================================

inline uint8_t calculateChecksum(const uint8_t *data, size_t length) {
    uint8_t crc = 0;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
    }
    return crc;
}

#endif // UART_PROTOCOL_H
