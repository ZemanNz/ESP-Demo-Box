#include <Arduino.h>
#include "config.h"
#include "SystemState.h"
#include "HardwareSetup.h"
#include "SensorManager.h"
#include "UartProtocol.h"

// ---------------------------------------------------------
// Fyzická instance Globálního Stavu spodního panelu
// ---------------------------------------------------------
SystemState globalState;

// ---------------------------------------------------------
// 1. Task: UART Komunikace s Horním panelem (Core 0)
// ---------------------------------------------------------
void Task_UART(void *pvParameters) {
    Serial.print("[TASK] Task_UART bezi na jadre: ");
    Serial.println(xPortGetCoreID());

#ifdef ENABLE_UART_TOP
    unsigned long lastSendTime = 0;
    TopToBottomPacket inPacket;
    BottomToTopPacket outPacket;

    for (;;) {
        unsigned long now = millis();

        // ---------------------------------------------------------
        // 1. PŘÍJEM PŘÍKAZŮ Z HORNÍHO PANELU (TOP -> BOTTOM)
        // ---------------------------------------------------------
        while (SerialTop.available() >= sizeof(TopToBottomPacket)) {
            // Kontrola startovního bajtu 0xAA
            if (SerialTop.peek() != UART_FRAME_START_TOP_TO_BOTTOM) {
                SerialTop.read(); // Zahodíme poškozený bajt a hledáme začátek
                continue;
            }

            // Přečtení celého paketu
            SerialTop.readBytes((uint8_t*)&inPacket, sizeof(TopToBottomPacket));

            // Výpočet a ověření kontrolního součtu
            uint8_t calculatedCRC = calculateChecksum(
                (const uint8_t*)&inPacket,
                sizeof(TopToBottomPacket) - 2
            );

            if (inPacket.endByte == UART_FRAME_END && inPacket.checksum == calculatedCRC) {
                // Uložíme příkazy do SystemState pro aplikaci na hardware
                globalState.applyTopPacket(inPacket);
            } else {
                Serial.println("[UART] Chyba kontrolniho souctu prikazu! Paket zahozen.");
            }
        }

        // ---------------------------------------------------------
        // 2. ODESÍLÁNÍ TELEMETRIE DO HORNÍHO PANELU (BOTTOM -> TOP)
        // ---------------------------------------------------------
        bool hasChanged = globalState.popNeedsTx();
        bool heartbeatTimeout = (now - lastSendTime >= 50); // Heartbeat / stream perioda 50 ms

        if (hasChanged || heartbeatTimeout) {
            lastSendTime = now;

            BottomInputs in = globalState.getInputs();

            outPacket.startByte = UART_FRAME_START_BOTTOM_TO_TOP; // 0x55
            outPacket.joyX = in.joyX;
            outPacket.joyY = in.joyY;
            outPacket.joyBtn = in.joyBtn;

            for (int i = 0; i < 5; i++) {
                outPacket.btnDown[i] = in.btnDown[i];
            }

            outPacket.encoderPos = in.encoderPos;
            outPacket.encoderDelta = in.encoderDelta;
            outPacket.encoderBtn = in.encoderBtn;

            outPacket.potentiometer = in.potentiometer;
            
            BottomOutputs out = globalState.getOutputs();
            outPacket.currentSmartServoAngle = out.targetSmartServoAngle;
            outPacket.currentServoAngle = out.targetServoAngle;
            outPacket.currentContinuousServo = out.targetContinuousServo;
            outPacket.currentMotorSpeed = out.targetMotorSpeed;

            for (int i = 0; i < 8; i++) {
                outPacket.ledStrip[i] = out.ledStrip[i];
            }
            outPacket.ledBrightness = out.ledBrightness;

            outPacket.switch1 = in.switch1;
            outPacket.switch2 = in.switch2;
            outPacket.endByte = UART_FRAME_END; // 0xFE

            outPacket.checksum = calculateChecksum(
                (const uint8_t*)&outPacket,
                sizeof(BottomToTopPacket) - 2
            );

            SerialTop.write((const uint8_t*)&outPacket, sizeof(BottomToTopPacket));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
#else
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif
}

// ---------------------------------------------------------
// 2. Task: Senzory, Vstupy a Výstupy (Core 1)
// ---------------------------------------------------------
void Task_Sensors(void *pvParameters) {
    Serial.print("[TASK] Task_Sensors bezi na jadre: ");
    Serial.println(xPortGetCoreID());
    byte delay = 20; // Perioda 20 ms (50 Hz)

    for (;;) {
        AppMode currentMode = globalState.getMode();

        switch (currentMode) {
        
            case MODE_SLEEP: {
                Serial.println("[SYSTEM] Dolni panel prechazi do Light Sleep...");

                // 1. Zhasneme OLED displej
                sensorManager.updateOled("", "");

                // 2. Zhasneme LED pásek
                uint32_t offLeds[8] = {0};
                sensorManager.setLedStrip(offLeds, 0);

                // 3. Zastavíme motory
                sensorManager.setMotor(0);
                sensorManager.setServoCont(0);

                // 4. Vyprázdníme sériové buffery před uspáním
                Serial.flush();
                #ifdef ENABLE_UART_TOP
                    SerialTop.flush();
                #endif

                #ifdef ENABLE_BUTTONS
                    gpio_wakeup_enable((gpio_num_t)PIN_BTN_1, GPIO_INTR_LOW_LEVEL);
                    gpio_wakeup_enable((gpio_num_t)PIN_BTN_2, GPIO_INTR_LOW_LEVEL);
                    gpio_wakeup_enable((gpio_num_t)PIN_BTN_3, GPIO_INTR_LOW_LEVEL);
                    gpio_wakeup_enable((gpio_num_t)PIN_BTN_4, GPIO_INTR_LOW_LEVEL);
                    gpio_wakeup_enable((gpio_num_t)PIN_BTN_5, GPIO_INTR_LOW_LEVEL);
                #endif

                #ifdef ENABLE_JOYSTICK
                    gpio_wakeup_enable((gpio_num_t)PIN_JOY_SW, GPIO_INTR_LOW_LEVEL); // Stisk joysticku
                #endif

                #ifdef ENABLE_ENCODER
                    gpio_wakeup_enable((gpio_num_t)PIN_ENC_CLK, GPIO_INTR_LOW_LEVEL); // Otočení enkodéru
                    gpio_wakeup_enable((gpio_num_t)PIN_ENC_DT,  GPIO_INTR_LOW_LEVEL);
                #endif

                #ifdef ENABLE_SWITCHES
                    // Přepnutí switchů do opačné polohy
                    gpio_wakeup_enable((gpio_num_t)PIN_SWITCH_1, digitalRead(PIN_SWITCH_1) == HIGH ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
                    gpio_wakeup_enable((gpio_num_t)PIN_SWITCH_2, digitalRead(PIN_SWITCH_2) == HIGH ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
                #endif

                esp_sleep_enable_gpio_wakeup();

                // 6. Nastavení probuzení po UARTu z horního panelu (UART1)
                #ifdef ENABLE_UART_TOP
                    esp_sleep_enable_uart_wakeup(1);
                #endif

                // 7. Samotný vstup do Light Sleep
                esp_light_sleep_start();

                // =====================================================
                // PROBUZENÍ! (Kód pokračuje zde po probuzení)
                // =====================================================
    
                esp_sleep_wakeup_cause_t duvod = esp_sleep_get_wakeup_cause();
                Serial.printf("[SYSTEM] Probudil jsem se z Light Sleep! (Duvod kod: %d)\n", (int)duvod);

                if (duvod != ESP_SLEEP_WAKEUP_UART) {
                    Serial.println("-> Probuzeno na DOLNIM panelu -> Probouzim HORNI panel!");
                    
                    #ifdef ENABLE_UART_TOP
                        // 1. Pošleme znak po UARTu, který okamžitě probudí horní panel
                        SerialTop.println("WAKEUP");
                    #endif

                    // 2. Přečteme vstupy a zvedneme příznak odeslání
                    sensorManager.updateAllInputs();
                    
                    // 3. Počkáme krátce (např. 150 ms), až horní panel odešle nový pracovní mód
                    unsigned long startWait = millis();
                    while (globalState.getMode() == MODE_SLEEP && millis() - startWait < 300) {
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                } 
                else {
                    Serial.println("-> Probuzeno z HORNIHO panelu (UART)");
                    // Krátká prodleva na přečtení příchozího paketu v Task_UART
                    vTaskDelay(pdMS_TO_TICKS(50));
                }

                break;
            }

            case MODE_MAIN_MENU:
            case MODE_SENSORS:
            case MODE_GAME_SNAKE:
            case MODE_GAME_FLAPPY:
            case MODE_2048:
            case MODE_VZDALENOST:
            case MODE_WIFI_SPOJENI:
            case MODE_SERVA:
            case MODE_MOTOR:
            case MODE_BAREVNY: 
                break;
   
        }
        
        vTaskDelay(pdMS_TO_TICKS(delay)); // Perioda 20 ms (50 Hz)
    }
}

// ---------------------------------------------------------
// Hlavní SETUP
// ---------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- ESP-Demo-Box: Start Spodniho Panelu ---");

    // 1. Inicializace hardwaru
    bool hwOk = initializeAllHardware();

    // 2. Synchronizační Boot Handshake s Horním panelem
    waitForHandshakeWithTop(hwOk, lastHardwareError);

    // 3. Spuštění FreeRTOS úloh
    xTaskCreatePinnedToCore(Task_UART, "UART_Bottom", 4096, NULL, 2, NULL, 0); // Core 0
    xTaskCreatePinnedToCore(Task_Sensors, "Sensors_Bottom", 4096, NULL, 1, NULL, 1); // Core 1

    Serial.println("[SYSTEM] Vsechna vlakna spodniho panelu uspesne spustena!");
}

// ---------------------------------------------------------
// Hlavní smyčka loop
// ---------------------------------------------------------
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}