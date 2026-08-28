#ifndef HARDWARE_SETUP_H
#define HARDWARE_SETUP_H

#include <Arduino.h>
#include "config.h"

// ---------------------------------------------------------
// Jednotlivé inicializační funkce pro konkrétní moduly
// ---------------------------------------------------------
bool setupI2C();
bool setupDisplay();
bool setupServosAndMotors();
bool setupInputs();
bool setupLedStrip();
bool setupUART();

// ---------------------------------------------------------
// Hlavní sdružující funkce (Master Setup)
// ---------------------------------------------------------
bool initializeAllHardware();

// Synchronizační handshake s horním panelem
bool waitForHandshakeWithTop(bool hardwareOk, const String& errorMsg = "");

// Globální chybová hláška z inicializace
extern String lastHardwareError;

// Globální instance sériové linky pro komunikaci s horním panelem (UART1)
#ifdef ENABLE_UART_TOP
extern HardwareSerial SerialTop;
#endif

#endif // HARDWARE_SETUP_H
