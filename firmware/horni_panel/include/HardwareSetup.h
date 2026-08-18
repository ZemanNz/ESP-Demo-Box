#ifndef HARDWARE_SETUP_H
#define HARDWARE_SETUP_H

#include <Arduino.h>

// ---------------------------------------------------------
// Jednotlivé inicializační funkce pro konkrétní moduly
// ---------------------------------------------------------
bool setupDisplay();
bool setupSensors();
bool setupWiFi();
bool setupUART();

// ---------------------------------------------------------
// Hlavní sdružující funkce (tzv. "Master Setup")
// ---------------------------------------------------------
// Tuto jedinou funkci zavoláš v main.cpp
bool initializeAllHardware();

// Globální instance sériové linky pro komunikaci se spodním panelem (UART1)
extern HardwareSerial SerialESP;

#endif // HARDWARE_SETUP_H
