#include "SensorManager.h"
#include "HardwareSetup.h"

extern volatile bool g_imuHwInterruptFired;
#include <Wire.h>

// Externí instance senzorů a periferií definované v HardwareSetup.cpp
#if defined(ENABLE_DHT) || defined(ENABLE_DHT22)
  #include <DHT.h>
  extern DHT dht;
#endif

#ifdef ENABLE_LSM6DS3
  #include <Adafruit_LSM6DS3.h>
  extern Adafruit_LSM6DS3 lsm6ds3;
#endif

#ifdef ENABLE_VL53L0X
  #include <VL53L0X.h>
  extern VL53L0X vl53;
#endif

#ifdef ENABLE_TCS34725
  #include <Adafruit_TCS34725.h>
  extern Adafruit_TCS34725 tcs;
#endif

#ifdef ENABLE_WS2812B
  #include <Adafruit_NeoPixel.h>
  extern Adafruit_NeoPixel strip;
#endif

#ifdef ENABLE_LCD1602
  #include <LiquidCrystal_I2C.h>
  extern LiquidCrystal_I2C lcd;
#endif

// Globální instance
SensorManager sensorManager;

SensorManager::SensorManager() {
}

// =============================================================================
// 1. ČTENÍ VŠECH SENZORŮ (INPUTS)
// =============================================================================

// Teplota a vlhkost (DHT)
void SensorManager::readDHT() {
#if defined(ENABLE_DHT) || defined(ENABLE_DHT22)
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
        globalState.updateTemperature(t, h);
    }
#endif
}

// Ultrazvukový senzor (HC-SR04)
void SensorManager::readUltrasonic() {
#ifdef ENABLE_ULTRASONIC
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    delayMicroseconds(4);
    digitalWrite(PIN_ULTRASONIC_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    
    // Timeout snížen na 12000 us (~205 cm), aby se task nezasekával při volném prostoru
    long duration = pulseIn(PIN_ULTRASONIC_ECHO, HIGH, 12000UL);
    float distCm = (duration == 0) ? -1.0f : (duration * 0.0343f / 2.0f);
    globalState.updateUltrasonicDistance(distCm);
#endif
}

// Laserový dálkoměr VL53L0X
void SensorManager::readVL53L0X() {
#ifdef ENABLE_VL53L0X
    uint16_t distMm = vl53.readRangeContinuousMillimeters();
    if (!vl53.timeoutOccurred()) {
        globalState.updateLaserDistance(distMm);
    }
#endif
}

// Infračervený senzor vzdálenosti (IR1 - analog na GPIO 20)
// Kalibrační křivka z naměřených bodů:
//  230 -> 2 cm, 1000 -> 3 cm, 2300 -> 4 cm, 3150 -> 5 cm,
// 3550 -> 6 cm, 3730 -> 7 cm, 3850 -> 8 cm, 4040 -> 9 cm
void SensorManager::readIRDistance() {
#ifdef ENABLE_IR_SENSORS
    int raw = analogRead(PIN_IR1);
    float distCm = -1.0f;

    if (raw < 100) {
        distCm = -1.0f; // Mimo dosah / volno
    } else if (raw <= 230) {
        // 100 až 230 -> 1.0 až 2.0 cm
        distCm = 1.0f + ((float)(raw - 100) / 130.0f) * 1.0f;
    } else if (raw <= 1000) {
        // 230 až 1000 -> 2.0 až 3.0 cm
        distCm = 2.0f + ((float)(raw - 230) / 770.0f) * 1.0f;
    } else if (raw <= 2300) {
        // 1000 až 2300 -> 3.0 až 4.0 cm
        distCm = 3.0f + ((float)(raw - 1000) / 1300.0f) * 1.0f;
    } else if (raw <= 3150) {
        // 2300 až 3150 -> 4.0 až 5.0 cm
        distCm = 4.0f + ((float)(raw - 2300) / 850.0f) * 1.0f;
    } else if (raw <= 3550) {
        // 3150 až 3550 -> 5.0 až 6.0 cm
        distCm = 5.0f + ((float)(raw - 3150) / 400.0f) * 1.0f;
    } else if (raw <= 3730) {
        // 3550 až 3730 -> 6.0 až 7.0 cm
        distCm = 6.0f + ((float)(raw - 3550) / 180.0f) * 1.0f;
    } else if (raw <= 3850) {
        // 3730 až 3850 -> 7.0 až 8.0 cm
        distCm = 7.0f + ((float)(raw - 3730) / 120.0f) * 1.0f;
    } else if (raw <= 4040) {
        // 3850 až 4040 -> 8.0 až 9.0 cm
        distCm = 8.0f + ((float)(raw - 3850) / 190.0f) * 1.0f;
    } else {
        // > 4040 (např. 4095) -> mimo dosah / Out of range (jako u laseru > 8000)
        distCm = -1.0f;
    }

    globalState.updateIRDistance(distCm);
#endif
}

// Digitální IR senzor překážek (IR2 - GPIO 38)
void SensorManager::readIRObstacle() {
#ifdef ENABLE_IR_SENSORS
    bool obstacle = (digitalRead(PIN_IR2) == LOW); // LOW = detekována překážka
    globalState.updateIRObstacle(obstacle);
#endif
}

// Analogové fotorezistory
void SensorManager::readPhotoresistors() {
#ifdef ENABLE_PHOTORESISTORS
    uint16_t p1 = analogRead(PIN_PHOTO1);
    uint16_t p2 = analogRead(PIN_PHOTO2);
    globalState.updatePhotoresistors(p1, p2);
#endif
}

// Tlačítko na horním panelu (detekce krátkého stisku pro info a 1s podržení pro spánek)
void SensorManager::readTopButton() {
#ifdef ENABLE_BUTTONS
    bool isPressed = (digitalRead(PIN_BTN1) == LOW); // PULLUP -> LOW = stisknuto
    globalState.updateTopButton(isPressed);

    uint32_t now = millis();

    // A) Náběžná hrana stisku (uživatel právě zmáčkl tlačítko)
    if (isPressed && !lastTopBtnState) {
        topBtnPressStartTime = now;
        topBtnLongPressHandled = false;
    }

    // B) Tlačítko je drženo stisknuté (kontrola 1s na přechod do SLEEP)
    if (isPressed && !topBtnLongPressHandled) {
        if (now - topBtnPressStartTime >= 1000) { // 1 sekunda podržení
            topBtnLongPressHandled = true;
            Serial.println("[BTN] 1s podrzeni horniho tlacitka -> SLEEP");
            globalState.setInfoOverlay(false);
            globalState.setMode(MODE_SLEEP);
        }
    }

    // C) Sestupná hrana (uživatel tlačítko pustil)
    if (!isPressed && lastTopBtnState) {
        // Pokud to nebyl dlouhý stisk a trval aspoň 50 ms (debounce ochrana)
        if (!topBtnLongPressHandled && (now - topBtnPressStartTime >= 50)) {
            Serial.println("[BTN] Kratky stisk horniho tlacitka -> Toggle INFO OVERLAY");
            globalState.prepniInfoOverlay();
        }
    }

    lastTopBtnState = isPressed;
#endif
}

// Barevný senzor TCS34725
void SensorManager::readColorSensor() {
#ifdef ENABLE_TCS34725
    float r = 0, g = 0, b = 0;
    tcs.getRGB(&r, &g, &b);
    globalState.updateColor((uint16_t)r, (uint16_t)g, (uint16_t)b);
#endif
}

// =============================================================================
// KALIBRACE GYROSKOPU LSM6DS3 (odstranění offsetu/šumu v klidu + nulování gravitačního náklonu)
// =============================================================================
void SensorManager::calibrateIMU() {
#ifdef ENABLE_LSM6DS3
    Serial.println("\n[LSM6DS3] Zahajuji kalibraci IMU... Drzte kufr v klidu (1.5 sekundy).");
    delay(200);
    float sumGx = 0.0f, sumGy = 0.0f, sumGz = 0.0f;
    float sumAx = 0.0f, sumAy = 0.0f, sumAz = 0.0f;
    const int numSamples = 150;

    for (int i = 0; i < numSamples; i++) {
        sensors_event_t accel, gyro, temp;
        lsm6ds3.getEvent(&accel, &gyro, &temp);
        // gyro.gyro.* je v rad/s -> převod na deg/s pro kalibraci v dps
        sumGx += gyro.gyro.x * (float)RAD_TO_DEG;
        sumGy += gyro.gyro.y * (float)RAD_TO_DEG;
        sumGz += gyro.gyro.z * (float)RAD_TO_DEG;
        sumAx += accel.acceleration.x / 9.80665f;
        sumAy += accel.acceleration.y / 9.80665f;
        sumAz += accel.acceleration.z / 9.80665f;
        delay(10);
    }

    gyroBiasX = sumGx / (float)numSamples;
    gyroBiasY = sumGy / (float)numSamples;
    gyroBiasZ = sumGz / (float)numSamples;

    float initAx = sumAx / (float)numSamples;
    float initAy = sumAy / (float)numSamples;
    float initAz = sumAz / (float)numSamples;

    // Počáteční klidový sklon víka vůči gravitaci (referenční nula = 0.0°)
    accelInitAngleX = atan2f(initAz, -initAy) * (float)RAD_TO_DEG;
    accelInitAngleZ = atan2f(initAx, -initAy) * (float)RAD_TO_DEG;

    // Počáteční úhly: relativní natočení od startu je přesně 0.0°
    imuAngleX = 0.0f;
    imuAngleY = 0.0f;
    imuAngleZ = 0.0f;
    lastGx = 0.0f;
    lastGy = 0.0f;
    lastGz = 0.0f;
    lastImuTimeUs = micros();
    imuCalibrated = true;

    Serial.printf("[LSM6DS3] Kalibrace hotova!\n");
    Serial.printf("  Gyro Bias (deg/s):  X=%.2f | Y=%.2f | Z=%.2f\n", gyroBiasX, gyroBiasY, gyroBiasZ);
    Serial.printf("  Klidovy sklon (deg): X=%.1f° | Z=%.1f°\n\n", accelInitAngleX, accelInitAngleZ);
#endif
}

void SensorManager::resetIMUAngles(float x, float y, float z) {
#ifdef ENABLE_LSM6DS3
    sensors_event_t accel, gyro, temp;
    lsm6ds3.getEvent(&accel, &gyro, &temp);
    float ax = accel.acceleration.x / 9.80665f;
    float ay = accel.acceleration.y / 9.80665f;
    float az = accel.acceleration.z / 9.80665f;

    accelInitAngleX = (atan2f(az, -ay) * (float)RAD_TO_DEG) - x;
    accelInitAngleZ = (atan2f(ax, -ay) * (float)RAD_TO_DEG) - z;

    imuAngleX = x;
    imuAngleY = y;
    imuAngleZ = z;
    lastGx = 0.0f;
    lastGy = 0.0f;
    lastGz = 0.0f;
    lastImuTimeUs = micros();
#endif
}

// Čtení a komplementární filtr s gravitační korekcí pro svislý horní panel
void SensorManager::readIMU() {
#ifdef ENABLE_LSM6DS3
    if (!imuCalibrated) {
        calibrateIMU();
        return;
    }

    // 1. Přesný časový krok dt v sekundách z mikrosekund procesoru
    unsigned long nowMicros = micros();
    float dt = (float)(nowMicros - lastImuTimeUs) / 1000000.0f;
    lastImuTimeUs = nowMicros;

    // Ochrana před velkým skokem po blokující operaci (např. připojování Wi-Fi)
    if (dt <= 0.0f || dt > 0.15f) {
        lastGx = 0.0f;
        lastGy = 0.0f;
        lastGz = 0.0f;
        return;
    }

    // 2. Načtení dat ze senzoru LSM6DS3
    sensors_event_t accel, gyro, temp;
    lsm6ds3.getEvent(&accel, &gyro, &temp);

    // 3. Přepočet úhlové rychlosti z rad/s na deg/s (stupně za sekundu)
    float gx = (gyro.gyro.x * (float)RAD_TO_DEG) - gyroBiasX;
    float gy = (gyro.gyro.y * (float)RAD_TO_DEG) - gyroBiasY;
    float gz = (gyro.gyro.z * (float)RAD_TO_DEG) - gyroBiasZ;

    // 4. Jemný klidový filtr (0.20 deg/s) - zamezí driftu na stole,
    //    ale zachytí i ten nejpomalejší pohyb ruky
    const float STILL_THRESHOLD = 0.20f; // stupňů za sekundu
    if (fabsf(gx) < STILL_THRESHOLD) gx = 0.0f;
    if (fabsf(gy) < STILL_THRESHOLD) gy = 0.0f;
    if (fabsf(gz) < STILL_THRESHOLD) gz = 0.0f;

    // 5. Akcelerace v násobku gravitačního zrychlení [g]
    float ax_g = accel.acceleration.x / 9.80665f;
    float ay_g = accel.acceleration.y / 9.80665f;
    float az_g = accel.acceleration.z / 9.80665f;

    // Celkové zrychlení pro detekci klidu vs dynamického rázu
    float totalG = sqrtf(ax_g * ax_g + ay_g * ay_g + az_g * az_g);

    // 6. Komplementární filtr:
    // Pokud je celkové zrychlení blízké 1g (klidný nebo plynulý pohyb),
    // využijeme gravitační vektor k eliminaci driftu a zaručení návratu přesně na 0.0°!
    if (totalG > 0.70f && totalG < 1.30f) {
        // Skutečný fyzický sklon z akcelerometru (vztažený k referenční nule)
        float accelAngleX = (atan2f(az_g, -ay_g) * (float)RAD_TO_DEG) - accelInitAngleX;
        float accelAngleZ = (atan2f(ax_g, -ay_g) * (float)RAD_TO_DEG) - accelInitAngleZ;

        // Váha: 98% rychlé gyro, 2% stálý akcelerometr (nuluje drift, drží nulu při zavření/otevření)
        const float alpha = 0.98f;
        imuAngleX = alpha * (imuAngleX + 0.5f * (gx + lastGx) * dt) + (1.0f - alpha) * accelAngleX;
        imuAngleZ = alpha * (imuAngleZ + 0.5f * (gz + lastGz) * dt) + (1.0f - alpha) * accelAngleZ;
    } else {
        // Při prudkém rázu / pohybu se spoléháme čistě na gyroskop
        imuAngleX += 0.5f * (gx + lastGx) * dt;
        imuAngleZ += 0.5f * (gz + lastGz) * dt;
    }

    // Osa Y (rotace kolem svislé osy kufru - točna na stole) - čistá numerická integrace gyroskopu
    imuAngleY += 0.5f * (gy + lastGy) * dt;

    lastGx = gx;
    lastGy = gy;
    lastGz = gz;

    // 7. Detekce drbnutí / nárazu (přes hardwarový pin INT1, přerušení nebo přetížení)
    static unsigned long lastShockTime = 0;
    static float peakShockG = 1.0f;
    bool isShock = false;

    #ifdef PIN_IMU_INT
    bool intPinHigh = (digitalRead(PIN_IMU_INT) == HIGH);
    #else
    bool intPinHigh = false;
    #endif

    if (g_imuHwInterruptFired || intPinHigh || (totalG > 1.25f)) {
        g_imuHwInterruptFired = false;
        lastShockTime = millis();
        if (totalG > peakShockG) peakShockG = totalG;
    }

    if (millis() - lastShockTime < 1000) {
        isShock = true;
    } else {
        peakShockG = totalG;
    }

    // Uložíme do sdíleného SystemState (přístupného pro UI displej, web i telemetrii)
    globalState.updateIMU(ax_g, ay_g, az_g, imuAngleX, imuAngleY, imuAngleZ, isShock, peakShockG);
#endif
}

// Hromadné přečtení všech senzorů
void SensorManager::updateAllSensors() {
    readDHT();
    readUltrasonic();
    readVL53L0X();
    readIRDistance();
    readIRObstacle();
    readPhotoresistors();
    readTopButton();
    readColorSensor();
    readIMU();
}

// =============================================================================
// 2. OVLÁDÁNÍ VÝSTUPŮ (OUTPUTS) S AUTOMATICKÝM ZÁPISEM DO SYSTEMSTATE
// =============================================================================

// 3x LED diody
void SensorManager::setLeds(bool l1, bool l2, bool l3) {
#ifdef ENABLE_LEDS
    digitalWrite(PIN_LED1, l1 ? HIGH : LOW);
    digitalWrite(PIN_LED2, l2 ? HIGH : LOW);
    digitalWrite(PIN_LED3, l3 ? HIGH : LOW);
#endif
    globalState.updateLeds(l1, l2, l3); // Uloží stav do SensorData
}

void SensorManager::setLed1(bool on) {
    SensorData current = globalState.getSensorData();
    setLeds(on, current.led_ir, current.led_laser);
}

void SensorManager::setLed2(bool on) {
    SensorData current = globalState.getSensorData();
    setLeds(current.led_ult, on, current.led_laser);
}

void SensorManager::setLed3(bool on) {
    SensorData current = globalState.getSensorData();
    setLeds(current.led_ult, current.led_ir, on);
}

// 8-LED WS2812B RGB pásek
void SensorManager::setLedStrip(const uint32_t leds[8], uint8_t brightness) {
#ifdef ENABLE_WS2812B
    strip.setBrightness(brightness);
    for (int i = 0; i < 8; i++) {
        strip.setPixelColor(i, leds[i]);
    }
    strip.show();
#endif
    globalState.updateLedStripTop(leds, brightness); // Uloží stav do SensorData
}

void SensorManager::setLedStripColor(uint32_t color, uint8_t brightness) {
    uint32_t leds[8];
    for (int i = 0; i < 8; i++) {
        leds[i] = color;
    }
    setLedStrip(leds, brightness);
}

void SensorManager::setLedStripPixel(uint8_t index, uint32_t color) {
    if (index >= 8) return;
#ifdef ENABLE_WS2812B
    strip.setPixelColor(index, color);
    strip.show();
#endif
    globalState.updateLedStripTopLed(index, color); // Uloží stav do SensorData
}

// Pasivní bzučák
void SensorManager::setBuzzer(bool active, uint16_t freq) {
#ifdef ENABLE_BUZZER
    if (active && freq > 0) {
        tone(PIN_BUZZER, freq);
    } else {
        noTone(PIN_BUZZER);
    }
#endif
    globalState.updateBuzzer(active, freq); // Uloží stav do SensorData
}

void SensorManager::beep(uint16_t freq, uint32_t durationMs) {
#ifdef ENABLE_BUZZER
    tone(PIN_BUZZER, freq, durationMs);
#endif
    globalState.updateBuzzer(true, freq);
}

// Sedmisegmentový displej přes posuvný registr 74HC595
void SensorManager::set7Segment(int value) {
#ifdef ENABLE_74HC595
    static const byte segDigits[] = {
        0x3F, // 0
        0x06, // 1
        0x5B, // 2
        0x4F, // 3
        0x66, // 4
        0x6D, // 5
        0x7D, // 6
        0x07, // 7
        0x7F, // 8
        0x6F  // 9
    };
    digitalWrite(PIN_SEG_LATCH, LOW);

    if (value < 0) {
        // Zhasnout všechny číslice
        byte blank = SEG_COMMON_ANODE ? 0xFF : 0x00;
        for (int i = 0; i < SEG_NUM_DIGITS; i++) {
            shiftOut(PIN_SEG_DATA, PIN_SEG_CLK, MSBFIRST, blank);
        }
    } else {
        long temp = value;
        for (int i = 0; i < SEG_NUM_DIGITS; i++) {
            int digit = temp % 10;
            byte pattern = (digit >= 0 && digit <= 9) ? segDigits[digit] : 0x00;

            // Zhasínání úvodních nul (např. místo 005 zobrazíme "  5")
            if (temp == 0 && i > 0 && value != 0) {
                pattern = 0x00;
            }
            if (value == 0 && i == 0) {
                pattern = segDigits[0];
            }

            if (SEG_COMMON_ANODE) pattern = ~pattern;
            shiftOut(PIN_SEG_DATA, PIN_SEG_CLK, MSBFIRST, pattern);
            temp /= 10;
        }
    }
    digitalWrite(PIN_SEG_LATCH, HIGH);
#endif
    globalState.update7Segment(value); // Uloží číslo do SensorData
}

// I2C LCD 1602
void SensorManager::writeLCD1602(const char* line1, const char* line2) {
#ifdef ENABLE_LCD1602
    if (line1) {
        lcd.setCursor(0, 0);
        lcd.print("                ");
        lcd.setCursor(0, 0);
        lcd.print(line1);
    }
    if (line2) {
        lcd.setCursor(0, 1);
        lcd.print("                ");
        lcd.setCursor(0, 1);
        lcd.print(line2);
    }
#endif
}

void SensorManager::writeLCD1602(const String& line1, const String& line2) {
    writeLCD1602(line1.c_str(), line2.length() > 0 ? line2.c_str() : nullptr);
}

void SensorManager::setLCDBacklight(bool on) {
#ifdef ENABLE_LCD1602
    if (on) {
        lcd.backlight();
        lcd.display();
    } else {
        lcd.noBacklight();
        lcd.noDisplay();
    }
#endif
}
