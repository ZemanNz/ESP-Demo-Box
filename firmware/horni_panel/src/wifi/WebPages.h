#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include <Arduino.h>

// =============================================================================
// TENTO SOUBOR UKLÁDÁ CELÝ WEB PŘÍMO DO FLASH PAMĚTI ESP32 (PROGMEM)
// =============================================================================

// --- 1. HTML5 ŠABLONA DASHBOARDU ---
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="cs">
<head>
    <!-- Nastavení kódování znaků na UTF-8 (aby spolehlivě fungovala česká diakritika) -->
    <meta charset="UTF-8">
    
    <!-- Nastavení responzivního viewportu pro telefony, tablety i počítače -->
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    
    <title>ESP-Demo-Box | Řídicí panel</title>
    
    <!-- Odkaz na externí soubor stylů -->
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <!-- ========================================================================= -->
    <!-- HORNÍ LIŠTA (HEADER) S LOGEM, INDIKÁTOREM A TLAČÍTKEM SPÁNKU -->
    <!-- ========================================================================= -->
    <header class="app-header">
        <div class="logo-area">
            <span class="logo-icon">⚡</span>
            <div>
                <h1>ESP-Demo-Box</h1>
                <p class="subtitle">Bezdrátový řídicí panel</p>
            </div>
        </div>
        
        <div class="header-actions">
            <!-- Rychlé tlačítko pro uspání boxu -->
            <button class="quick-sleep-btn" onclick="setAppMode(13)" title="Uspat ESP-Demo-Box">
                💤 Uspat
            </button>
            <!-- Indikátor stavu WebSocket spojení (Online / Připojování / Odpojeno) -->
            <div id="status-badge" class="status-badge connecting">
                <span class="dot"></span>
                <span id="status-text">Připojování...</span>
            </div>
        </div>
    </header>

    <!-- ========================================================================= -->
    <!-- NAVIGAČNÍ ZÁLOŽKY (TELEMETRIE / OVLÁDÁNÍ / SYSTÉM) -->
    <!-- ========================================================================= -->
    <nav class="tab-bar">
        <button class="tab-btn active" onclick="switchTab('telemetry')">📊 Telemetrie</button>
        <button class="tab-btn" onclick="switchTab('control')">🎛️ Ovládání</button>
        <button class="tab-btn" onclick="switchTab('system')">⚙️ Systém</button>
    </nav>

    <!-- ========================================================================= -->
    <!-- HLAVNÍ OBSAH DASHBOARDU -->
    <!-- ========================================================================= -->
    <main class="container">

        <!-- --------------------------------------------------------------------- -->
        <!-- ZÁLOŽKA 1: TELEMETRIE (ŽIVÉ HODNOTY ZE VŠECH SENZORŮ + REÁLNÝ GRAF) -->
        <!-- --------------------------------------------------------------------- -->
        <section id="tab-telemetry" class="tab-content active">
            
            <!-- Živý graf senzorů v reálném čase -->
            <div class="card full-width">
                <div class="card-header">
                    <span class="card-icon">📈</span>
                    <h3>Historie měření v reálném čase</h3>
                </div>
                <div class="chart-tab-bar">
                    <button class="chart-btn active" onclick="setChartMetric('dist')">Vzdálenosti (Laser, Ultra, IR)</button>
                    <button class="chart-btn" onclick="setChartMetric('env')">Teplota & Vlhkost</button>
                    <button class="chart-btn" onclick="setChartMetric('imu')">Akcelerometr (X, Y, Z)</button>
                </div>
                <div class="chart-wrapper">
                    <canvas id="telemetry-chart" width="560" height="180"></canvas>
                </div>
                <div id="chart-legend" class="chart-legend"></div>
            </div>

            <!-- Karta: Teplota a Vlhkost (DHT11) -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">🌡️</span>
                    <h3>Klima a Prostředí</h3>
                </div>
                <div class="grid-2">
                    <div class="metric-box">
                        <span class="metric-label">Teplota</span>
                        <span class="metric-value"><span id="val-temp">--</span> °C</span>
                    </div>
                    <div class="metric-box">
                        <span class="metric-label">Vlhkost</span>
                        <span class="metric-value"><span id="val-hum">--</span> %</span>
                    </div>
                </div>
            </div>

            <!-- Karta: Senzory vzdálenosti (Laser ToF, Ultrazvuk, IR) -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">📏</span>
                    <h3>Senzory vzdálenosti</h3>
                </div>
                <div class="grid-3">
                    <div class="metric-box">
                        <span class="metric-label">Laser (ToF)</span>
                        <span class="metric-value"><span id="val-laser">--</span> mm</span>
                    </div>
                    <div class="metric-box">
                        <span class="metric-label">Ultrazvuk</span>
                        <span class="metric-value"><span id="val-ultra">--</span> cm</span>
                    </div>
                    <div class="metric-box">
                        <span class="metric-label">IR Senzor</span>
                        <span class="metric-value"><span id="val-ir">--</span> cm</span>
                    </div>
                </div>
                <div class="sub-metric mt-2">
                    <span>IR Digitální detekce:</span>
                    <span id="val-ir-obs" class="tag">Čisté</span>
                </div>
            </div>

            <!-- Karta: Wi-Fi Detekce Přítomnosti (CSI Radar) -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">📡</span>
                    <h3>Wi-Fi CSI Radar & Vzdálenost</h3>
                </div>
                <div class="grid-2">
                    <div class="metric-box">
                        <span class="metric-label">Detekce pohybu</span>
                        <span id="val-csi-motion" class="tag">KLID</span>
                    </div>
                    <div class="metric-box">
                        <span class="metric-label">Vzdálenost mobilu</span>
                        <span class="metric-value" id="val-wifi-dist">---</span>
                    </div>
                </div>
                <div class="sub-metric mt-2">
                    <span>Metrika aktivity signálu:</span>
                    <b id="val-csi-metric">0.00</b>
                </div>
                <div class="mt-2 text-center">
                    <button class="btn-sm" onclick="calibrateCsi()">🎯 Kalibrovat radar (5 s)</button>
                </div>
            </div>

            <!-- Karta: 6osé IMU (LSM6DS3) Gyroskop a Akcelerometr -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">🧭</span>
                    <h3>6-osé IMU (LSM6DS3)</h3>
                </div>
                <p class="section-subtitle">Zrychlení [g]</p>
                <div class="grid-3">
                    <div class="metric-box">
                        <span class="metric-label">Osa X</span>
                        <span class="metric-value" id="val-ax">0.00</span>
                    </div>
                    <div class="metric-box">
                        <span class="metric-label">Osa Y</span>
                        <span class="metric-value" id="val-ay">0.00</span>
                    </div>
                    <div class="metric-box">
                        <span class="metric-label">Osa Z</span>
                        <span class="metric-value" id="val-az">0.00</span>
                    </div>
                </div>
                <p class="section-subtitle mt-2">Úhlová rychlost [°/s]</p>
                <div class="grid-3">
                    <div class="metric-box">
                        <span class="metric-label">Gyro X</span>
                        <span class="metric-value" id="val-gx">0.0</span>
                    </div>
                    <div class="metric-box">
                        <span class="metric-label">Gyro Y</span>
                        <span class="metric-value" id="val-gy">0.0</span>
                    </div>
                    <div class="metric-box">
                        <span class="metric-label">Gyro Z</span>
                        <span class="metric-value" id="val-gz">0.0</span>
                    </div>
                </div>
            </div>

            <!-- Karta: Světelné a Barevné senzory (LDR + TCS34725) -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">🎨</span>
                    <h3>Osvětlení & Barvy</h3>
                </div>
                <div class="grid-2">
                    <div class="metric-box">
                        <span class="metric-label">Fotorezistor 1</span>
                        <span class="metric-value" id="val-photo1">--</span>
                    </div>
                    <div class="metric-box">
                        <span class="metric-label">Fotorezistor 2</span>
                        <span class="metric-value" id="val-photo2">--</span>
                    </div>
                </div>
                <div class="color-sensor-box mt-2">
                    <div id="color-preview" class="color-preview-circle"></div>
                    <div class="color-values">
                        <span>R: <b id="val-cr">0</b></span>
                        <span>G: <b id="val-cg">0</b></span>
                        <span>B: <b id="val-cb">0</b></span>
                    </div>
                </div>
            </div>

            <!-- Karta: Vstupy z dolního panelu (Potenciometr, Enkodér, Switche) -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">🎮</span>
                    <h3>Fyzické ovládací prvky</h3>
                </div>
                <div class="grid-2">
                    <div class="metric-box">
                        <span class="metric-label">Potenciometr</span>
                        <span class="metric-value" id="val-pot">--</span>
                    </div>
                    <div class="metric-box">
                        <span class="metric-label">Enkodér</span>
                        <span class="metric-value"><span id="val-enc">0</span> <small id="val-ebtn" class="tag">UP</small></span>
                    </div>
                </div>
                <div class="grid-2 mt-2">
                    <div class="sub-metric">
                        <span>Switch 1:</span>
                        <b id="val-sw1">OFF</b>
                    </div>
                    <div class="sub-metric">
                        <span>Switch 2:</span>
                        <b id="val-sw2">OFF</b>
                    </div>
                </div>
                <div class="sub-metric mt-1">
                    <span>Horní tlačítko:</span>
                    <span id="val-btntop" class="tag">UP</span>
                </div>
                <!-- Indikace 5 fyzických tlačítek dolního panelu -->
                <p class="section-subtitle mt-2">Stav 5 tlačítek na krabičce:</p>
                <div class="hw-buttons-row">
                    <span id="btn-d1" class="hw-btn-indicator">1 (IR)</span>
                    <span id="btn-d2" class="hw-btn-indicator">2 (ULT)</span>
                    <span id="btn-d3" class="hw-btn-indicator">3 (LAS)</span>
                    <span id="btn-d4" class="hw-btn-indicator">4 (UP)</span>
                    <span id="btn-d5" class="hw-btn-indicator">5 (DWN)</span>
                </div>
            </div>
        </section>

        <!-- --------------------------------------------------------------------- -->
        <!-- ZÁLOŽKA 2: OVLÁDÁNÍ (SENZORY, POHONY, RGB, BZUČÁK, MÓDY, JOYSTICK, D-PAD) -->
        <!-- --------------------------------------------------------------------- -->
        <section id="tab-control" class="tab-content">
            
            <!-- 1. Výběr aktivního senzoru vzdálenosti (Synchronizováno se 7-segmentem a tlačítky 1, 2, 3) -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">🎯</span>
                    <h3>Aktivní senzor vzdálenosti</h3>
                </div>
                <p class="card-desc">Přepíná senzor pro 7-segmentový displej a LED indikátory (stejně jako fyzická tlačítka 1, 2, 3):</p>
                <div class="sensor-selector-group">
                    <button id="sens-btn-ir" class="sensor-select-btn active" onclick="selectDistanceSensor('ir')">
                        <span class="sensor-dot dot-red"></span>
                        <span class="sensor-title">🔴 IR dálkoměr</span>
                        <span class="sensor-badge">D1 / Tlačítko 1</span>
                        <span class="sensor-live" id="sens-live-ir">-- cm</span>
                    </button>
                    <button id="sens-btn-ult" class="sensor-select-btn" onclick="selectDistanceSensor('ultrasonic')">
                        <span class="sensor-dot dot-green"></span>
                        <span class="sensor-title">🟢 Ultrazvuk</span>
                        <span class="sensor-badge">D4 / Tlačítko 2</span>
                        <span class="sensor-live" id="sens-live-ult">-- cm</span>
                    </button>
                    <button id="sens-btn-laser" class="sensor-select-btn" onclick="selectDistanceSensor('laser')">
                        <span class="sensor-dot dot-blue"></span>
                        <span class="sensor-title">🔵 Laser ToF</span>
                        <span class="sensor-badge">D5 / Tlačítko 3</span>
                        <span class="sensor-live" id="sens-live-laser">-- mm</span>
                    </button>
                </div>
            </div>

            <!-- 2. Pohony a Serva (Klasické servo 172..0°, Klepeta LX-16A, Lineární posuv) -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">🦾</span>
                    <h3>Pohony & Robotické rameno</h3>
                </div>

                <!-- 1. Klasické servo MG996R (172° vlevo až 0° vpravo) -->
                <div class="actuator-block">
                    <div class="slider-header">
                        <span class="actuator-name">📐 Klasické servo (Základna 172° vlevo – 0° vpravo):</span>
                        <b id="lbl-servo" class="actuator-value">90°</b>
                    </div>
                    <div class="dial-container">
                        <div class="dial-arc">
                            <div id="dial-needle" class="dial-needle" style="transform: rotate(0deg);"></div>
                        </div>
                    </div>
                    <!-- Posuvník: 0 vlevo (172°), 172 vpravo (0°) -->
                    <input type="range" id="ctrl-servo" min="0" max="172" value="82" oninput="onServoChange(this.value)">
                    <div class="preset-angles mt-1">
                        <button class="btn-micro" onclick="setServoAngle(172)">172° (Vlevo)</button>
                        <button class="btn-micro" onclick="setServoAngle(135)">135°</button>
                        <button class="btn-micro" onclick="setServoAngle(90)">90°</button>
                        <button class="btn-micro" onclick="setServoAngle(45)">45°</button>
                        <button class="btn-micro" onclick="setServoAngle(0)">0° (Vpravo)</button>
                    </div>
                </div>

                <!-- 2. Chytré servo LX-16A (Klepeta robotického ramene) -->
                <div class="actuator-block mt-3">
                    <div class="slider-header">
                        <span class="actuator-name">🦀 Chytré servo (Klepeta LX-16A):</span>
                        <b id="lbl-smartservo" class="actuator-value">0°</b>
                    </div>
                    <!-- Tlačítka pro rychlé otevření a sevření klepet -->
                    <div class="gripper-btn-row mt-1">
                        <button class="gripper-btn gripper-open" onclick="openGripper()">
                            <span class="gripper-icon">👐</span>
                            <span>OTEVŘÍT KLEPETA</span>
                        </button>
                        <button class="gripper-btn gripper-close" onclick="closeGripper()">
                            <span class="gripper-icon">🤏</span>
                            <span>SEVŘÍT KLEPETA</span>
                        </button>
                    </div>
                    <div class="slider-sub mt-2">
                        <span>Jemné doladění úhlu klepet (-120° až +120°):</span>
                    </div>
                    <input type="range" id="ctrl-smartservo" min="-120" max="120" value="0" oninput="onSmartServoChange(this.value)">
                </div>

                <!-- 3. Kontinuální servo pro lineární posuv (invertovaný směr dle požadavku) -->
                <div class="actuator-block mt-3">
                    <div class="slider-header">
                        <span class="actuator-name">🔄 Kontinuální servo (Lineární posuv):</span>
                        <b id="lbl-motor" class="actuator-value">0 %</b>
                    </div>
                    <input type="range" id="ctrl-motor" min="-100" max="100" value="0" oninput="onMotorChange(this.value)">
                    <div class="motor-btn-row mt-1">
                        <button class="btn-sm" onclick="setMotorSpeed(-60)">⏪ Vzad</button>
                        <button class="btn-sm btn-stop" onclick="stopMotor()">⏹ Zastavit</button>
                        <button class="btn-sm" onclick="setMotorSpeed(60)">⏩ Vpřed</button>
                    </div>
                </div>
            </div>

            <!-- 3. Ovládání RGB LED pásku WS2812B -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">✨</span>
                    <h3>RGB LED Pásek</h3>
                </div>
                <div class="control-row">
                    <label>Barva pásku:</label>
                    <input type="color" id="ctrl-rgb-color" value="#ff0000" onchange="updateRgbStrip()">
                </div>
                <div class="control-row mt-2">
                    <label>Jas: <b id="lbl-rgb-bright">60</b></label>
                    <input type="range" id="ctrl-rgb-bright" min="0" max="255" value="60" oninput="updateRgbBright(this.value)">
                </div>
                <div class="color-presets mt-2">
                    <button class="preset-btn" style="background:#ff0000;" onclick="setRgbPreset('#ff0000')"></button>
                    <button class="preset-btn" style="background:#00ff00;" onclick="setRgbPreset('#00ff00')"></button>
                    <button class="preset-btn" style="background:#0088ff;" onclick="setRgbPreset('#0088ff')"></button>
                    <button class="preset-btn" style="background:#ffaa00;" onclick="setRgbPreset('#ffaa00')"></button>
                    <button class="preset-btn" style="background:#ff00ff;" onclick="setRgbPreset('#ff00ff')"></button>
                    <button class="preset-btn" style="background:#ffffff;" onclick="setRgbPreset('#ffffff')"></button>
                    <button class="preset-btn" style="background:#000000;" onclick="setRgbPreset('#000000')"></button>
                </div>
            </div>

            <!-- 4. Bzučák -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">🔔</span>
                    <h3>Bzučák</h3>
                </div>
                <div class="buzzer-actions">
                    <button class="action-btn" onclick="beep(1000, 150)">🔊 Pípnout (1 kHz)</button>
                    <button class="action-btn" onclick="beep(2000, 150)">🔊 Pípnout (2 kHz)</button>
                    <button class="action-btn" onclick="beep(500, 300)">📢 Hluboký tón (500 Hz)</button>
                </div>
            </div>

            <!-- 5. Výběr aplikačního módu na displeji ESP (včetně módu Spánek) -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">🖥️</span>
                    <h3>Přepnout mód na displeji</h3>
                </div>
                <div class="modes-grid">
                    <button class="mode-btn" onclick="setAppMode(0)">🏠 Menu</button>
                    <button class="mode-btn" onclick="setAppMode(1)">🧭 Gyroskop</button>
                    <button class="mode-btn" onclick="setAppMode(2)">🐍 Hra Snake</button>
                    <button class="mode-btn" onclick="setAppMode(3)">🐦 Hra Flappy</button>
                    <button class="mode-btn" onclick="setAppMode(4)">🔢 Hra 2048</button>
                    <button class="mode-btn" onclick="setAppMode(5)">🧱 Hra Tetris</button>
                    <button class="mode-btn" onclick="setAppMode(6)">📏 Měření vzdál.</button>
                    <button class="mode-btn" onclick="setAppMode(7)">🛣️ Sledovač čáry</button>
                    <button class="mode-btn" onclick="setAppMode(8)">📶 Wi-Fi QR kód</button>
                    <button class="mode-btn" onclick="setAppMode(9)">📡 CSI Radar</button>
                    <button class="mode-btn" onclick="setAppMode(10)">🦾 Serva & Rameno</button>
                    <button class="mode-btn" onclick="setAppMode(12)">🎨 Barevný senzor</button>
                    <button class="mode-btn mode-sleep-btn" onclick="setAppMode(13)">💤 Uspat box (Spánek)</button>
                </div>
            </div>

            <!-- 6. Virtuální analogový joystick (přesunut dolů) -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">🕹️</span>
                    <h3>Virtuální Joystick</h3>
                </div>
                <p class="card-desc">Ovládání her (Snake, Tetris, 2048) a robotického ramene dotykem či myší:</p>
                <div class="joystick-wrapper">
                    <canvas id="joystick-canvas" width="220" height="220"></canvas>
                </div>
                <div class="joystick-readout">
                    <span>Osa X: <b id="joy-readout-x">0%</b></span>
                    <button id="joy-btn-trigger" class="joy-btn-pill" onmousedown="onJoyBtnPress(true)" onmouseup="onJoyBtnPress(false)" ontouchstart="onJoyBtnPress(true)" ontouchend="onJoyBtnPress(false)">
                        🔘 Tlačítko joysticku
                    </button>
                    <span>Osa Y: <b id="joy-readout-y">0%</b></span>
                </div>
            </div>

            <!-- 7. D-Pad ovladač / 5 čtvercových tlačítek do kříže (přesunut dolů) -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">🎮</span>
                    <h3>D-Pad ovladač (5 tlačítek do kříže)</h3>
                </div>
                <div class="dpad-container">
                    <div class="dpad-row">
                        <button class="dpad-btn" 
                                onmousedown="sendBtn(3, true)" onmouseup="sendBtn(3, false)"
                                ontouchstart="sendBtn(3, true)" ontouchend="sendBtn(3, false)">
                            ▲<small>4 (UP)</small>
                        </button>
                    </div>
                    <div class="dpad-row">
                        <button class="dpad-btn" 
                                onmousedown="sendBtn(0, true)" onmouseup="sendBtn(0, false)"
                                ontouchstart="sendBtn(0, true)" ontouchend="sendBtn(0, false)">
                            ◀<small>1 (LEFT)</small>
                        </button>
                        <button class="dpad-btn dpad-center" 
                                onmousedown="sendBtn(1, true)" onmouseup="sendBtn(1, false)"
                                ontouchstart="sendBtn(1, true)" ontouchend="sendBtn(1, false)">
                            ●<small>2 (OK)</small>
                        </button>
                        <button class="dpad-btn" 
                                onmousedown="sendBtn(2, true)" onmouseup="sendBtn(2, false)"
                                ontouchstart="sendBtn(2, true)" ontouchend="sendBtn(2, false)">
                            ▶<small>3 (RIGHT)</small>
                        </button>
                    </div>
                    <div class="dpad-row">
                        <button class="dpad-btn" 
                                onmousedown="sendBtn(4, true)" onmouseup="sendBtn(4, false)"
                                ontouchstart="sendBtn(4, true)" ontouchend="sendBtn(4, false)">
                            ▼<small>5 (DOWN)</small>
                        </button>
                    </div>
                </div>
            </div>
        </section>

        <!-- --------------------------------------------------------------------- -->
        <!-- ZÁLOŽKA 3: SYSTÉMOVÉ INFORMACE -->
        <!-- --------------------------------------------------------------------- -->
        <section id="tab-system" class="tab-content">
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">ℹ️</span>
                    <h3>Systémové informace</h3>
                </div>
                <div class="sys-info-table">
                    <div class="sys-row"><span>Aktuální mód ESP:</span><b id="sys-mode">--</b></div>
                    <div class="sys-row"><span>Uptime ESP32:</span><b id="sys-uptime">0s</b></div>
                    <div class="sys-row"><span>Volná paměť (Heap):</span><b id="sys-heap">-- KB</b></div>
                    <div class="sys-row"><span>Připojení klienti:</span><b id="sys-clients">1</b></div>
                    <div class="sys-row"><span>Wi-Fi SSID:</span><b>ESP-Demo-Box</b></div>
                    <div class="sys-row"><span>IP adresa:</span><b>192.168.4.1</b></div>
                </div>
            </div>
        </section>
    </main>

    <!-- Patička webu -->
    <footer class="app-footer">
        <span>ESP-Demo-Box © 2026 | Vyvinuto s důrazem na čistý kód a spolehlivost</span>
    </footer>

    <!-- Načtení JavaScriptového skriptu -->
    <script src="script.js"></script>
</body>
</html>

)rawliteral";

// --- 2. KASKÁDOVÉ STYLY (CSS) ---
const char STYLE_CSS[] PROGMEM = R"rawliteral(
/* =============================================================================
   KASKÁDOVÉ STYLY (CSS) PRO OVLÁDACÍ PANEL ESP-DEMO-BOX
   =============================================================================
   Tento soubor určuje barvy, písmo, velikosti, rozložení karet a animace webu.
   Navrženo v moderním tmavém motivu (Dark Mode) inspirovaném TFT displejem ESP32.
   ============================================================================= */

/* --- 1. DEFINICE GLOBÁLNÍCH PROMĚNNÝCH (:root) --- */
:root {
    --bg-color: #0b0f19;         /* Hluboké tmavé pozadí celého webu */
    --card-bg: #131b2a;          /* Tmavě modrošedé pozadí karet */
    --card-hover: #172235;       /* Zvýrazněné pozadí při najetí myší */
    --card-border: #1e293b;      /* Jemný rámeček karet */
    --primary: #00d2ff;          /* Zářivá azurová barva (stejná jako TFT cyan) */
    --primary-glow: rgba(0, 210, 255, 0.25);
    --accent: #818cf8;           /* Fialovo-modrá pro sekundární akce */
    --text-main: #f1f5f9;        /* Bílý hlavní text */
    --text-muted: #94a3b8;       /* Tlumený šedý text pro popisky */
    --success: #10b981;          /* Zelená pro aktivní stavy a ultrazvuk */
    --danger: #ef4444;           /* Červená pro IR senzor a stop */
    --warning: #f59e0b;          /* Oranžová pro varování a klepeta */
    --radius: 12px;              /* Standardní zakulacení prvků */
    --transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
}

/* Resetování stylů */
* {
    box-sizing: border-box;
    margin: 0;
    padding: 0;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    -webkit-tap-highlight-color: transparent;
}

body {
    background-color: var(--bg-color);
    color: var(--text-main);
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    padding-bottom: 24px;
}

/* --- 2. HORNÍ LIŠTA (HEADER) --- */
.app-header {
    background: #0d1524;
    border-bottom: 1px solid var(--card-border);
    padding: 12px 18px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    position: sticky;
    top: 0;
    z-index: 100;
}

.logo-area {
    display: flex;
    align-items: center;
    gap: 10px;
}

.logo-icon {
    font-size: 1.6rem;
    filter: drop-shadow(0 0 8px var(--primary));
}

.logo-area h1 {
    font-size: 1.15rem;
    font-weight: 700;
    letter-spacing: 0.5px;
    color: #ffffff;
}

.subtitle {
    font-size: 0.75rem;
    color: var(--primary);
}

.header-actions {
    display: flex;
    align-items: center;
    gap: 10px;
}

.quick-sleep-btn {
    padding: 6px 12px;
    background: rgba(99, 102, 241, 0.18);
    border: 1px solid rgba(99, 102, 241, 0.45);
    color: #c7d2fe;
    border-radius: 20px;
    font-size: 0.78rem;
    font-weight: 700;
    cursor: pointer;
    transition: var(--transition);
}

.quick-sleep-btn:hover {
    background: rgba(99, 102, 241, 0.35);
    color: #ffffff;
    box-shadow: 0 0 10px rgba(99, 102, 241, 0.5);
}

.quick-sleep-btn:active {
    transform: scale(0.95);
}

/* Indikátor stavu WebSocketu */
.status-badge {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    padding: 5px 12px;
    border-radius: 20px;
    font-size: 0.8rem;
    font-weight: 600;
    border: 1px solid transparent;
}

.status-badge .dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    animation: pulse 1.8s infinite;
}

.status-badge.online {
    background: rgba(16, 185, 129, 0.15);
    border-color: rgba(16, 185, 129, 0.4);
    color: #34d399;
}
.status-badge.online .dot { background: #10b981; }

.status-badge.connecting {
    background: rgba(245, 158, 11, 0.15);
    border-color: rgba(245, 158, 11, 0.4);
    color: #fbbf24;
}
.status-badge.connecting .dot { background: #f59e0b; }

.status-badge.offline {
    background: rgba(239, 68, 68, 0.15);
    border-color: rgba(239, 68, 68, 0.4);
    color: #f87171;
}
.status-badge.offline .dot { background: #ef4444; }

@keyframes pulse {
    0%, 100% { opacity: 1; transform: scale(1); }
    50% { opacity: 0.5; transform: scale(0.85); }
}

/* --- 3. NAVIGAČNÍ ZÁLOŽKY --- */
.tab-bar {
    display: flex;
    background: #0f1827;
    border-bottom: 1px solid var(--card-border);
    position: sticky;
    top: 57px;
    z-index: 90;
}

.tab-btn {
    flex: 1;
    padding: 12px 10px;
    background: transparent;
    border: none;
    border-bottom: 3px solid transparent;
    color: var(--text-muted);
    font-size: 0.92rem;
    font-weight: 600;
    cursor: pointer;
    transition: var(--transition);
}

.tab-btn.active {
    color: var(--primary);
    border-bottom-color: var(--primary);
    background: rgba(0, 210, 255, 0.05);
}

/* --- 4. HLAVNÍ OBSAH A KARTY --- */
.container {
    max-width: 900px;
    width: 100%;
    margin: 0 auto;
    padding: 16px;
    flex: 1;
}

.tab-content {
    display: none;
    flex-direction: column;
    gap: 16px;
}

.tab-content.active {
    display: flex;
}

.card {
    background: var(--card-bg);
    border: 1px solid var(--card-border);
    border-radius: var(--radius);
    padding: 16px;
    box-shadow: 0 4px 14px rgba(0, 0, 0, 0.25);
    transition: var(--transition);
}

.card.full-width {
    width: 100%;
}

.card-header {
    display: flex;
    align-items: center;
    gap: 10px;
    margin-bottom: 12px;
}

.card-header h3 {
    font-size: 1.05rem;
    font-weight: 700;
    color: #ffffff;
}

.card-icon {
    font-size: 1.25rem;
}

.card-desc {
    font-size: 0.82rem;
    color: var(--text-muted);
    margin-bottom: 12px;
}

.section-subtitle {
    font-size: 0.78rem;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    color: var(--text-muted);
    margin-bottom: 6px;
}

/* Mřížky (Grid) */
.grid-2 {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 10px;
}

.grid-3 {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 10px;
}

/* Boxy pro jednotlivé metriky */
.metric-box {
    background: rgba(15, 23, 42, 0.6);
    border: 1px solid rgba(255, 255, 255, 0.05);
    border-radius: 8px;
    padding: 10px;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    text-align: center;
}

.metric-label {
    font-size: 0.75rem;
    color: var(--text-muted);
    margin-bottom: 4px;
}

.metric-value {
    font-size: 1.2rem;
    font-weight: 700;
    color: var(--primary);
    font-variant-numeric: tabular-nums;
}

.sub-metric {
    display: flex;
    align-items: center;
    justify-content: space-between;
    font-size: 0.85rem;
    color: var(--text-muted);
    padding: 6px 0;
    border-top: 1px solid rgba(255, 255, 255, 0.05);
}

.tag {
    padding: 3px 8px;
    border-radius: 6px;
    font-size: 0.75rem;
    font-weight: 700;
    background: rgba(255, 255, 255, 0.08);
    color: var(--text-muted);
}

.tag.active {
    background: var(--danger);
    color: #ffffff;
    box-shadow: 0 0 10px rgba(239, 68, 68, 0.5);
}

/* --- 5. ŽIVÝ GRAF SENZORŮ --- */
.chart-tab-bar {
    display: flex;
    gap: 8px;
    margin-bottom: 10px;
    flex-wrap: wrap;
}

.chart-btn {
    padding: 6px 12px;
    background: rgba(255, 255, 255, 0.05);
    border: 1px solid var(--card-border);
    color: var(--text-muted);
    border-radius: 6px;
    font-size: 0.8rem;
    font-weight: 600;
    cursor: pointer;
    transition: var(--transition);
}

.chart-btn.active {
    background: var(--primary);
    border-color: var(--primary);
    color: #000000;
    font-weight: 700;
}

.chart-wrapper {
    width: 100%;
    background: #080c14;
    border: 1px solid rgba(255, 255, 255, 0.07);
    border-radius: 8px;
    overflow: hidden;
    position: relative;
}

#telemetry-chart {
    display: block;
    width: 100%;
    height: 180px;
}

.chart-legend {
    display: flex;
    gap: 16px;
    justify-content: center;
    margin-top: 8px;
    font-size: 0.78rem;
    color: var(--text-muted);
}

.legend-item {
    display: flex;
    align-items: center;
    gap: 6px;
}

.legend-color {
    width: 10px;
    height: 10px;
    border-radius: 2px;
}

/* --- 6. PŘEPÍNAČ AKTIVNÍHO SENZORU VZDÁLENOSTI --- */
.sensor-selector-group {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 10px;
}

.sensor-select-btn {
    background: #0d1522;
    border: 2px solid var(--card-border);
    border-radius: 10px;
    padding: 12px 8px;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
    cursor: pointer;
    transition: var(--transition);
    color: var(--text-main);
    text-align: center;
}

.sensor-select-btn:hover {
    background: #141f32;
    border-color: rgba(255, 255, 255, 0.2);
}

.sensor-select-btn.active {
    background: rgba(0, 210, 255, 0.12);
    border-color: var(--primary);
    box-shadow: 0 0 14px var(--primary-glow);
}

.sensor-dot {
    width: 10px;
    height: 10px;
    border-radius: 50%;
    margin-bottom: 2px;
}

.dot-red { background: #ef4444; box-shadow: 0 0 8px #ef4444; }
.dot-green { background: #10b981; box-shadow: 0 0 8px #10b981; }
.dot-blue { background: #38bdf8; box-shadow: 0 0 8px #38bdf8; }

.sensor-title {
    font-size: 0.85rem;
    font-weight: 700;
}

.sensor-badge {
    font-size: 0.68rem;
    color: var(--text-muted);
}

.sensor-live {
    font-size: 0.95rem;
    font-weight: 700;
    color: var(--primary);
    margin-top: 4px;
}

/* --- 7. POHONY & ROBOTICKÉ RAMENO --- */
.actuator-block {
    background: rgba(15, 23, 42, 0.5);
    border: 1px solid rgba(255, 255, 255, 0.05);
    border-radius: 10px;
    padding: 14px;
}

.slider-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 8px;
}

.actuator-name {
    font-size: 0.88rem;
    font-weight: 600;
    color: var(--text-main);
}

.actuator-value {
    font-size: 1.1rem;
    color: var(--primary);
    font-variant-numeric: tabular-nums;
}

/* Půlkruhový volič (Arc dial) */
.dial-container {
    display: flex;
    justify-content: center;
    margin: 6px 0 10px;
}

.dial-arc {
    width: 120px;
    height: 60px;
    border-top-left-radius: 60px;
    border-top-right-radius: 60px;
    border: 6px solid #1e293b;
    border-bottom: 0;
    position: relative;
    background: radial-gradient(circle at bottom, rgba(0, 210, 255, 0.1) 0%, transparent 70%);
}

.dial-needle {
    position: absolute;
    bottom: 0;
    left: 50%;
    width: 3px;
    height: 52px;
    background: var(--primary);
    transform-origin: bottom center;
    border-radius: 2px;
    transition: transform 0.05s ease-out;
    box-shadow: 0 0 8px var(--primary);
}

.preset-angles {
    display: flex;
    justify-content: space-between;
    gap: 6px;
}

.btn-micro {
    flex: 1;
    padding: 5px;
    background: rgba(255, 255, 255, 0.06);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 6px;
    color: var(--text-muted);
    font-size: 0.75rem;
    cursor: pointer;
    font-weight: 600;
    transition: var(--transition);
}

.btn-micro:hover {
    background: rgba(0, 210, 255, 0.15);
    border-color: var(--primary);
    color: #ffffff;
}

/* Klepeta robotického ramene */
.gripper-btn-row {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 10px;
}

.gripper-btn {
    padding: 12px 10px;
    border-radius: 8px;
    border: 2px solid transparent;
    font-size: 0.85rem;
    font-weight: 700;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    cursor: pointer;
    transition: var(--transition);
}

.gripper-open {
    background: rgba(245, 158, 11, 0.15);
    border-color: rgba(245, 158, 11, 0.4);
    color: #fbbf24;
}

.gripper-open:active {
    background: #f59e0b;
    color: #000000;
}

.gripper-close {
    background: rgba(16, 185, 129, 0.15);
    border-color: rgba(16, 185, 129, 0.4);
    color: #34d399;
}

.gripper-close:active {
    background: #10b981;
    color: #000000;
}

.gripper-icon {
    font-size: 1.2rem;
}

.motor-btn-row {
    display: flex;
    gap: 8px;
}

.btn-stop {
    background: rgba(239, 68, 68, 0.2) !important;
    border-color: var(--danger) !important;
    color: #fca5a5 !important;
}

/* Slidery */
input[type=range] {
    -webkit-appearance: none;
    width: 100%;
    height: 8px;
    background: #1e293b;
    border-radius: 4px;
    outline: none;
}

input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 22px;
    height: 22px;
    border-radius: 50%;
    background: var(--primary);
    cursor: pointer;
    box-shadow: 0 0 10px var(--primary-glow);
    border: 2px solid #ffffff;
}

/* --- 8. VIRTUÁLNÍ JOYSTICK --- */
.joystick-wrapper {
    display: flex;
    justify-content: center;
    margin: 10px 0;
}

#joystick-canvas {
    background: #090e18;
    border: 2px solid var(--card-border);
    border-radius: 50%;
    box-shadow: inset 0 0 15px rgba(0, 0, 0, 0.6), 0 0 15px rgba(0, 210, 255, 0.1);
    touch-action: none;
    cursor: crosshair;
}

.joystick-readout {
    display: flex;
    justify-content: space-between;
    align-items: center;
    font-size: 0.85rem;
    color: var(--text-muted);
}

.joy-btn-pill {
    padding: 6px 14px;
    background: rgba(255, 255, 255, 0.08);
    border: 1px solid var(--card-border);
    border-radius: 20px;
    color: var(--text-main);
    font-size: 0.8rem;
    font-weight: 600;
    cursor: pointer;
    transition: var(--transition);
}

.joy-btn-pill:active, .joy-btn-pill.active {
    background: var(--primary);
    color: #000000;
    box-shadow: 0 0 10px var(--primary-glow);
}

/* --- 9. D-PAD OVLADAČ --- */
.dpad-container {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 8px;
    margin: 12px 0 6px;
}

.dpad-row {
    display: flex;
    gap: 8px;
}

.dpad-btn {
    width: 66px;
    height: 66px;
    background: #172235;
    border: 1px solid var(--card-border);
    border-radius: 12px;
    color: #ffffff;
    font-size: 1.15rem;
    font-weight: 700;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    cursor: pointer;
    user-select: none;
    transition: var(--transition);
}

.dpad-btn small {
    font-size: 0.58rem;
    font-weight: 500;
    color: var(--text-muted);
    line-height: 1.1;
    margin-top: 2px;
}

.dpad-btn:active {
    background: var(--primary);
    color: #000000;
    transform: scale(0.94);
}

.dpad-btn.dpad-center {
    background: #1e2c45;
    border-color: var(--primary);
}

/* --- 10. BAREVNÝ SENZOR & RGB PÁSEK --- */
.color-sensor-box {
    display: flex;
    align-items: center;
    gap: 14px;
    background: rgba(15, 23, 42, 0.6);
    padding: 10px;
    border-radius: 8px;
}

.color-preview-circle {
    width: 44px;
    height: 44px;
    border-radius: 50%;
    border: 2px solid rgba(255, 255, 255, 0.2);
    box-shadow: 0 0 10px rgba(0, 0, 0, 0.4);
    background: #000000;
}

.color-values {
    display: flex;
    gap: 12px;
    font-size: 0.85rem;
    color: var(--text-muted);
}

.control-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 10px;
}

.color-presets {
    display: flex;
    gap: 8px;
}

.preset-btn {
    flex: 1;
    height: 32px;
    border: 2px solid rgba(255, 255, 255, 0.2);
    border-radius: 6px;
    cursor: pointer;
    transition: transform 0.15s ease;
}

.preset-btn:active {
    transform: scale(0.92);
}

/* --- 11. BZUČÁK --- */
.buzzer-actions {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 8px;
}

.action-btn {
    padding: 12px 8px;
    background: #172235;
    border: 1px solid var(--card-border);
    border-radius: 8px;
    color: var(--text-main);
    font-size: 0.82rem;
    font-weight: 600;
    cursor: pointer;
    transition: var(--transition);
}

.action-btn:hover {
    background: #1e2d47;
    border-color: var(--primary);
}

.action-btn:active {
    background: var(--primary);
    color: #000000;
}

/* --- 12. PŘEPÍNAČ MÓDŮ --- */
.modes-grid {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 8px;
}

.mode-btn {
    padding: 10px 6px;
    background: #121a28;
    border: 1px solid var(--card-border);
    border-radius: 8px;
    color: var(--text-muted);
    font-size: 0.78rem;
    font-weight: 600;
    cursor: pointer;
    transition: var(--transition);
}

.mode-btn:hover {
    background: #182438;
    color: #ffffff;
}

.mode-btn.current {
    background: rgba(0, 210, 255, 0.15);
    border-color: var(--primary);
    color: var(--primary);
    font-weight: 700;
}

.mode-btn.mode-sleep-btn {
    background: rgba(99, 102, 241, 0.16);
    border-color: rgba(99, 102, 241, 0.45);
    color: #c7d2fe;
    font-weight: 700;
}

.mode-btn.mode-sleep-btn:hover {
    background: rgba(99, 102, 241, 0.35);
    color: #ffffff;
}

.mode-btn.mode-sleep-btn.current {
    background: rgba(99, 102, 241, 0.45);
    border-color: #818cf8;
    color: #ffffff;
    box-shadow: 0 0 10px rgba(99, 102, 241, 0.5);
}

/* Indikátory tlačítek na krabičce */
.hw-buttons-row {
    display: flex;
    gap: 6px;
}

.hw-btn-indicator {
    flex: 1;
    text-align: center;
    padding: 6px 2px;
    background: rgba(255, 255, 255, 0.05);
    border-radius: 6px;
    font-size: 0.72rem;
    color: var(--text-muted);
}

.hw-btn-indicator.active {
    background: var(--primary);
    color: #000000;
    font-weight: 700;
    box-shadow: 0 0 10px var(--primary-glow);
}

/* Systémové tabulky */
.sys-info-table {
    display: flex;
    flex-direction: column;
    gap: 8px;
}

.sys-row {
    display: flex;
    justify-content: space-between;
    padding: 8px 0;
    border-bottom: 1px solid rgba(255, 255, 255, 0.05);
    font-size: 0.88rem;
    color: var(--text-muted);
}

.sys-row b {
    color: var(--text-main);
}

/* Univerzální tlačítka */
.btn-sm {
    padding: 8px 12px;
    background: rgba(255, 255, 255, 0.08);
    border: 1px solid var(--card-border);
    border-radius: 6px;
    color: var(--text-main);
    font-size: 0.8rem;
    font-weight: 600;
    cursor: pointer;
    transition: var(--transition);
}

.btn-sm:hover {
    background: rgba(0, 210, 255, 0.2);
    border-color: var(--primary);
}

/* Pomocné třídy */
.mt-1 { margin-top: 6px; }
.mt-2 { margin-top: 12px; }
.mt-3 { margin-top: 16px; }
.text-center { text-align: center; }

/* Patička */
.app-footer {
    text-align: center;
    padding: 16px;
    font-size: 0.75rem;
    color: var(--text-muted);
    border-top: 1px solid var(--card-border);
    margin-top: auto;
}

/* Responzivní přizpůsobení pro mobily */
@media (max-width: 480px) {
    .grid-3 {
        grid-template-columns: repeat(3, 1fr);
    }
    .sensor-selector-group {
        grid-template-columns: repeat(3, 1fr);
    }
    .sensor-select-btn {
        padding: 8px 4px;
    }
    .sensor-title {
        font-size: 0.75rem;
    }
    .sensor-badge {
        font-size: 0.6rem;
    }
    .modes-grid {
        grid-template-columns: repeat(2, 1fr);
    }
    .buzzer-actions {
        grid-template-columns: 1fr;
    }
    .dpad-btn {
        width: 58px;
        height: 58px;
    }
}

)rawliteral";

// --- 3. JAVASCRIPT OVLÁDÁNÍ A WEBSOCKET SPOJENÍ ---
const char SCRIPT_JS[] PROGMEM = R"rawliteral(
// =============================================================================
// JAVASCRIPT PRO OVLÁDACÍ PANEL ESP-DEMO-BOX
// =============================================================================
// Tento skript zajišťuje obousměrnou komunikaci přes WebSocket:
// 1. Přijímá živou telemetrii ze všech senzorů a vykresluje ji do DOMu a grafu.
// 2. Obsluhuje interaktivní prvky: výběr senzoru, serva, klepeta, virtuální joystick.
// =============================================================================

// --- GLOBÁLNÍ PROMĚNNÉ SPOJENÍ A STAVU ---
let ws = null;                  // Instance WebSocket klienta
let reconnectTimer = null;      // Časovač pro automatické znovuobnovení spojení
let currentAppMode = 0;         // Aktuální běžící mód na displeji ESP32

// Názvy jednotlivých režimů boxu
const modeNames = [
    "Hlavní menu", "Gyroskop", "Hra Snake", "Hra Flappy", "Hra 2048", "Hra Tetris",
    "Měření vzdálenosti", "Sledování čáry (IR)", "Wi-Fi QR spojení", "Wi-Fi CSI Radar",
    "Ovládání serv", "Dokumentace", "Barevný senzor", "Spánkový režim"
];

// --- 1. SPUSŤ PO NAČTENÍ STRÁNKY ---
window.addEventListener('load', () => {
    initWebSocket();         // Otevřeme síťové spojení s ESP32
    initJoystick();          // Inicializujeme virtuální joystick na canvasu
    initTelemetryChart();    // Připravíme plátno pro živý graf
});

// --- 2. PŘEPÍNÁNÍ ZÁLOŽEK (Telemetrie / Ovládání / Systém) ---
function switchTab(tabId) {
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));

    const btn = Array.from(document.querySelectorAll('.tab-btn'))
                     .find(b => b.getAttribute('onclick')?.includes(tabId));
    if (btn) btn.classList.add('active');

    const content = document.getElementById(`tab-${tabId}`);
    if (content) content.classList.add('active');
}

// --- 3. SPRÁVA WEBSOCKET SPOJENÍ ---
function initWebSocket() {
    const wsUrl = `ws://${location.host}/ws`;
    const statusBadge = document.getElementById('status-badge');
    const statusText = document.getElementById('status-text');

    statusBadge.className = 'status-badge connecting';
    statusText.innerText = 'Připojování...';

    try {
        ws = new WebSocket(wsUrl);

        ws.onopen = () => {
            statusBadge.className = 'status-badge online';
            statusText.innerText = 'Online';
            if (reconnectTimer) {
                clearTimeout(reconnectTimer);
                reconnectTimer = null;
            }
        };

        ws.onclose = () => {
            statusBadge.className = 'status-badge offline';
            statusText.innerText = 'Odpojeno';
            scheduleReconnect();
        };

        ws.onerror = (err) => {
            console.error('[WS] Chyba spojení:', err);
            ws.close();
        };

        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                handleTelemetry(data);
            } catch (e) {
                console.error('[WS] Chyba parsování JSON:', e);
            }
        };
    } catch (e) {
        statusBadge.className = 'status-badge offline';
        statusText.innerText = 'Chyba';
        scheduleReconnect();
    }
}

function scheduleReconnect() {
    if (!reconnectTimer) {
        reconnectTimer = setTimeout(() => {
            reconnectTimer = null;
            initWebSocket();
        }, 2000);
    }
}

// Odeslání JSON příkazu přes WebSocket na ESP32
function sendCmd(cmd, payload = {}) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ cmd, ...payload }));
    }
}

// Pomocný omezovač volání (ochrana Wi-Fi před zahlcením)
function throttle(func, limit) {
    let inThrottle = false;
    return function(...args) {
        if (!inThrottle) {
            func.apply(this, args);
            inThrottle = true;
            setTimeout(() => inThrottle = false, limit);
        }
    };
}

// --- 4. ZPRACOVÁNÍ PŘÍCHOZÍCH TELEMETRICKÝCH DAT ---
function handleTelemetry(d) {
    // 1. Prostředí (Teplota a Vlhkost)
    if (d.t !== undefined) setText('val-temp', d.t > -100 ? d.t.toFixed(1) : '--');
    if (d.h !== undefined) setText('val-hum', d.h >= 0 ? d.h.toFixed(1) : '--');

    // 2. Vzdálenosti
    if (d.laser !== undefined) {
        setText('val-laser', d.laser);
        setText('sens-live-laser', `${d.laser} mm`);
    }
    if (d.ultra !== undefined) {
        const uStr = d.ultra >= 0 ? d.ultra.toFixed(1) : '--';
        setText('val-ultra', uStr);
        setText('sens-live-ult', `${uStr} cm`);
    }
    if (d.ir !== undefined) {
        const iStr = d.ir >= 0 ? d.ir.toFixed(1) : '--';
        setText('val-ir', iStr);
        setText('sens-live-ir', `${iStr} cm`);
    }

    // IR digitální překážka
    if (d.irobs !== undefined) {
        const obsEl = document.getElementById('val-ir-obs');
        if (obsEl) {
            obsEl.innerText = d.irobs ? 'PŘEKÁŽKA!' : 'Čisté';
            obsEl.className = d.irobs ? 'tag active' : 'tag';
        }
    }

    // Wi-Fi CSI Radar
    if (d.wifi_dist !== undefined) setText('val-wifi-dist', d.wifi_dist > 0.1 ? d.wifi_dist.toFixed(1) + ' m' : '---');
    if (d.csi_metric !== undefined) setText('val-csi-metric', d.csi_metric.toFixed(2));
    if (d.csi_motion !== undefined) {
        const motEl = document.getElementById('val-csi-motion');
        if (motEl) {
            motEl.innerText = d.csi_motion ? '🚨 POHYB!' : 'KLID';
            motEl.className = d.csi_motion ? 'tag active' : 'tag';
        }
    }

    // 3. IMU akcelerometr a gyroskop
    if (d.ax !== undefined) setText('val-ax', d.ax.toFixed(2));
    if (d.ay !== undefined) setText('val-ay', d.ay.toFixed(2));
    if (d.az !== undefined) setText('val-az', d.az.toFixed(2));
    if (d.gx !== undefined) setText('val-gx', d.gx.toFixed(1));
    if (d.gy !== undefined) setText('val-gy', d.gy.toFixed(1));
    if (d.gz !== undefined) setText('val-gz', d.gz.toFixed(1));

    // 4. Světlo a Barvy
    if (d.p1 !== undefined) setText('val-photo1', d.p1);
    if (d.p2 !== undefined) setText('val-photo2', d.p2);
    if (d.cr !== undefined && d.cg !== undefined && d.cb !== undefined) {
        setText('val-cr', d.cr);
        setText('val-cg', d.cg);
        setText('val-cb', d.cb);
        const colPrev = document.getElementById('color-preview');
        if (colPrev) colPrev.style.backgroundColor = `rgb(${d.cr}, ${d.cg}, ${d.cb})`;
    }

    // 5. Fyzické vstupy
    if (d.pot !== undefined) setText('val-pot', d.pot);
    if (d.enc !== undefined) setText('val-enc', d.enc);
    if (d.ebtn !== undefined) {
        const eb = document.getElementById('val-ebtn');
        if (eb) {
            eb.innerText = d.ebtn ? 'STISK' : 'UP';
            eb.className = d.ebtn ? 'tag active' : 'tag';
        }
    }
    if (d.btntop !== undefined) {
        const tb = document.getElementById('val-btntop');
        if (tb) {
            tb.innerText = d.btntop ? 'STISK' : 'UP';
            tb.className = d.btntop ? 'tag active' : 'tag';
        }
    }
    if (d.sw1 !== undefined) setText('val-sw1', d.sw1 ? 'ON' : 'OFF');
    if (d.sw2 !== undefined) setText('val-sw2', d.sw2 ? 'ON' : 'OFF');

    // 5 tlačítek na dolním panelu
    if (d.btnd && Array.isArray(d.btnd)) {
        for (let i = 0; i < 5; i++) {
            const btnEl = document.getElementById(`btn-d${i+1}`);
            if (btnEl) {
                if (d.btnd[i]) btnEl.classList.add('active');
                else btnEl.classList.remove('active');
            }
        }
    }

    // 6. Synchronizace aktivního senzoru vzdálenosti (podle LED 1, 2, 3)
    const isIr = d.led_ir !== undefined ? d.led_ir : d.led1;
    const isUlt = d.led_ult !== undefined ? d.led_ult : d.led2;
    const isLaser = d.led_laser !== undefined ? d.led_laser : d.led3;

    updateSensorSelectorVisual(isIr, isUlt, isLaser);

    // 7. Synchronizace pozic pohonů ze systému
    if (d.servo !== undefined && document.activeElement !== document.getElementById('ctrl-servo')) {
        updateServoDisplay(d.servo);
    }
    if (d.smartservo !== undefined && document.activeElement !== document.getElementById('ctrl-smartservo')) {
        setText('lbl-smartservo', `${d.smartservo}°`);
        const sEl = document.getElementById('ctrl-smartservo');
        if (sEl) sEl.value = d.smartservo;
    }
    if (d.motor !== undefined && document.activeElement !== document.getElementById('ctrl-motor')) {
        const displayMotor = -d.motor; // Přepočet invertovaného směru
        setText('lbl-motor', `${displayMotor} %`);
        const mEl = document.getElementById('ctrl-motor');
        if (mEl) mEl.value = displayMotor;
    }

    // 8. Systémové informace
    if (d.mode !== undefined) {
        currentAppMode = d.mode;
        setText('sys-mode', modeNames[d.mode] || `Mód ${d.mode}`);
        updateActiveModeButtons(d.mode);
    }
    if (d.uptime !== undefined) {
        const sec = Math.floor(d.uptime / 1000);
        const mins = Math.floor(sec / 60);
        const hrs = Math.floor(mins / 60);
        setText('sys-uptime', `${hrs}h ${mins%60}m ${sec%60}s`);
    }
    if (d.heap !== undefined) setText('sys-heap', `${(d.heap / 1024).toFixed(1)} KB`);
    if (d.clients !== undefined) setText('sys-clients', d.clients);

    // 9. Aktualizace dat v živém grafu
    pushTelemetryToChart(d);
}

// Zvýraznění aktivního senzoru vzdálenosti
function updateSensorSelectorVisual(isIr, isUlt, isLaser) {
    const btnIr = document.getElementById('sens-btn-ir');
    const btnUlt = document.getElementById('sens-btn-ult');
    const btnLaser = document.getElementById('sens-btn-laser');

    if (btnIr) btnIr.classList.toggle('active', !!isIr);
    if (btnUlt) btnUlt.classList.toggle('active', !!isUlt);
    if (btnLaser) btnLaser.classList.toggle('active', !!isLaser);
}

// Pomocná funkce pro bezpečný přepis textu v HTML elementu
function setText(id, val) {
    const el = document.getElementById(id);
    if (el) el.innerText = val;
}

// Zvýraznění aktivního módu v seznamu
function updateActiveModeButtons(mode) {
    document.querySelectorAll('.mode-btn').forEach(btn => {
        const oc = btn.getAttribute('onclick') || '';
        if (oc.includes(`(${mode})`)) btn.classList.add('current');
        else btn.classList.remove('current');
    });
}

// --- 5. VÝBĚR AKTIVNÍHO SENZORU VZDÁLENOSTI ---
function selectDistanceSensor(type) {
    sendCmd('selectSensor', { sensor: type });
}

// --- 6. OVLÁDÁNÍ POHONŮ & SERVA ---

// Aktualizace zobrazení úhlu klasického serva a ručičky voliče
// Fyzické servo má 172° vlevo a 0° vpravo
function updateServoDisplay(angle) {
    const clampedAngle = Math.max(0, Math.min(172, parseInt(angle)));
    setText('lbl-servo', `${clampedAngle}°`);
    
    // Slider pozice: vlevo (0) odpovídá 172°, vpravo (172) odpovídá 0°
    const sliderPos = 172 - clampedAngle;
    const slider = document.getElementById('ctrl-servo');
    if (slider) slider.value = sliderPos;

    // Rotace ručičky: -86° vlevo (pro 172°) až +86° vpravo (pro 0°)
    const rotDeg = ((sliderPos / 172) * 172) - 86;
    const needle = document.getElementById('dial-needle');
    if (needle) needle.style.transform = `rotate(${rotDeg}deg)`;
}

// Plynulá změna úhlu klasického serva ze slideru
// Posuvník je od 0 (vlevo = 172°) do 172 (vpravo = 0°)
const onServoChange = throttle((sliderVal) => {
    const angle = 172 - parseInt(sliderVal);
    updateServoDisplay(angle);
    sendCmd('setServo', { val: angle });
}, 50);

// Rychlé předvolby úhlu serva
function setServoAngle(angle) {
    updateServoDisplay(angle);
    sendCmd('setServo', { val: parseInt(angle) });
}

// Otevření klepet robotického ramene
function openGripper() {
    setText('lbl-smartservo', '-60°');
    const sEl = document.getElementById('ctrl-smartservo');
    if (sEl) sEl.value = -60;
    sendCmd('setSmartServo', { val: -60 });
}

// Sevření klepet robotického ramene
function closeGripper() {
    setText('lbl-smartservo', '60°');
    const sEl = document.getElementById('ctrl-smartservo');
    if (sEl) sEl.value = 60;
    sendCmd('setSmartServo', { val: 60 });
}

// Plynulá změna úhlu chytrého serva
const onSmartServoChange = throttle((val) => {
    setText('lbl-smartservo', `${val}°`);
    sendCmd('setSmartServo', { val: parseInt(val) });
}, 50);

// Plynulá změna rychlosti kontinuálního serva (invertovaný směr dle fyzické mechaniky)
const onMotorChange = throttle((val) => {
    const numVal = parseInt(val);
    setText('lbl-motor', `${numVal} %`);
    // Prohozený směr: kladné hodnoty posílají záporné do motoru a naopak
    sendCmd('setMotor', { val: -numVal });
}, 50);

// Rychlé nastavení rychlosti kontinuálního serva
function setMotorSpeed(val) {
    const numVal = parseInt(val);
    setText('lbl-motor', `${numVal} %`);
    const mEl = document.getElementById('ctrl-motor');
    if (mEl) mEl.value = numVal;
    sendCmd('setMotor', { val: -numVal });
}

// Okamžité zastavení kontinuálního serva
function stopMotor() {
    setMotorSpeed(0);
}

// --- 7. VIRTUÁLNÍ JOYSTICK (CANVAS INTERAKCE) ---
let joyCanvas = null;
let joyCtx = null;
let joyDragging = false;
let joyRadius = 0;
let joyCenter = { x: 0, y: 0 };
let joyPos = { x: 0, y: 0 };
let joyBtnState = false;

function initJoystick() {
    joyCanvas = document.getElementById('joystick-canvas');
    if (!joyCanvas) return;
    joyCtx = joyCanvas.getContext('2d');

    joyRadius = (joyCanvas.width / 2) - 20;
    joyCenter = { x: joyCanvas.width / 2, y: joyCanvas.height / 2 };
    joyPos = { x: joyCenter.x, y: joyCenter.y };

    drawJoystick();

    // Události myši
    joyCanvas.addEventListener('mousedown', (e) => startJoy(e.clientX, e.clientY));
    window.addEventListener('mousemove', (e) => { if (joyDragging) moveJoy(e.clientX, e.clientY); });
    window.addEventListener('mouseup', () => { if (joyDragging) stopJoy(); });

    // Události dotykové obrazovky (mobil/tablet)
    joyCanvas.addEventListener('touchstart', (e) => {
        if (e.touches.length > 0) {
            e.preventDefault();
            startJoy(e.touches[0].clientX, e.touches[0].clientY);
        }
    }, { passive: false });

    window.addEventListener('touchmove', (e) => {
        if (joyDragging && e.touches.length > 0) {
            moveJoy(e.touches[0].clientX, e.touches[0].clientY);
        }
    }, { passive: false });

    window.addEventListener('touchend', () => { if (joyDragging) stopJoy(); });
    window.addEventListener('touchcancel', () => { if (joyDragging) stopJoy(); });
}

function startJoy(clientX, clientY) {
    joyDragging = true;
    moveJoy(clientX, clientY);
}

function moveJoy(clientX, clientY) {
    const rect = joyCanvas.getBoundingClientRect();
    const touchX = clientX - rect.left;
    const touchY = clientY - rect.top;

    let dx = touchX - joyCenter.x;
    let dy = touchY - joyCenter.y;
    const dist = Math.sqrt(dx * dx + dy * dy);

    // Omezení maximální výchylky na poloměr kružnice
    if (dist > joyRadius) {
        dx = (dx / dist) * joyRadius;
        dy = (dy / dist) * joyRadius;
    }

    joyPos = { x: joyCenter.x + dx, y: joyCenter.y + dy };
    drawJoystick();

    // Normalizace na rozsah -1.0 až +1.0
    const normX = dx / joyRadius;
    const normY = dy / joyRadius;

    setText('joy-readout-x', `${Math.round(normX * 100)}%`);
    setText('joy-readout-y', `${Math.round(normY * 100)}%`);

    // Převod na hodnoty ADC (0 až 4095, střed 2048)
    const espX = Math.round(2048 + normX * 1800);
    const espY = Math.round(2048 + normY * 1800);

    sendJoyThrottled(espX, espY, joyBtnState);
}

function stopJoy() {
    joyDragging = false;
    joyPos = { x: joyCenter.x, y: joyCenter.y };
    drawJoystick();

    setText('joy-readout-x', '0%');
    setText('joy-readout-y', '0%');

    // Návrat do neutrálu
    sendCmd('setJoy', { x: 2048, y: 2048, btn: joyBtnState });
}

const sendJoyThrottled = throttle((x, y, btn) => {
    sendCmd('setJoy', { x, y, btn });
}, 40);

function onJoyBtnPress(pressed) {
    joyBtnState = pressed;
    const pill = document.getElementById('joy-btn-trigger');
    if (pill) pill.classList.toggle('active', pressed);

    const normX = (joyPos.x - joyCenter.x) / joyRadius;
    const normY = (joyPos.y - joyCenter.y) / joyRadius;
    const espX = Math.round(2048 + normX * 1800);
    const espY = Math.round(2048 + normY * 1800);

    sendCmd('setJoy', { x: espX, y: espY, btn: joyBtnState });
}

function drawJoystick() {
    if (!joyCtx) return;
    joyCtx.clearRect(0, 0, joyCanvas.width, joyCanvas.height);

    // 1. Vnější kruhová dráha
    joyCtx.beginPath();
    joyCtx.arc(joyCenter.x, joyCenter.y, joyRadius, 0, Math.PI * 2);
    joyCtx.strokeStyle = 'rgba(255, 255, 255, 0.12)';
    joyCtx.lineWidth = 4;
    joyCtx.stroke();

    // 2. Osy kříže (zaměřovač)
    joyCtx.beginPath();
    joyCtx.strokeStyle = 'rgba(0, 210, 255, 0.15)';
    joyCtx.lineWidth = 1;
    joyCtx.moveTo(joyCenter.x - joyRadius, joyCenter.y);
    joyCtx.lineTo(joyCenter.x + joyRadius, joyCenter.y);
    joyCtx.moveTo(joyCenter.x, joyCenter.y - joyRadius);
    joyCtx.lineTo(joyCenter.x, joyCenter.y + joyRadius);
    joyCtx.stroke();

    // 3. Pohyblivý klobouček (Thumbstick)
    const stickRadius = 26;
    const grad = joyCtx.createRadialGradient(
        joyPos.x - 4, joyPos.y - 4, 4,
        joyPos.x, joyPos.y, stickRadius
    );
    grad.addColorStop(0, '#38bdf8');
    grad.addColorStop(1, '#0284c7');

    joyCtx.beginPath();
    joyCtx.arc(joyPos.x, joyPos.y, stickRadius, 0, Math.PI * 2);
    joyCtx.fillStyle = grad;
    joyCtx.shadowColor = 'rgba(0, 210, 255, 0.5)';
    joyCtx.shadowBlur = 12;
    joyCtx.fill();
    joyCtx.shadowBlur = 0;

    // Lem kloboučku
    joyCtx.strokeStyle = '#ffffff';
    joyCtx.lineWidth = 2;
    joyCtx.stroke();
}

// --- 8. ŽIVÝ GRAF SENZORŮ (ČISTÝ CANVAS BEZ EXTERNÍCH KNIHOVEN) ---
const CHART_MAX_POINTS = 35;
let chartActiveMetric = 'dist'; // 'dist', 'env', 'imu'

const chartData = {
    // Vzdálenosti
    laser: [],
    ultra: [],
    ir: [],
    // Prostředí
    temp: [],
    hum: [],
    // IMU
    ax: [],
    ay: [],
    az: []
};

function initTelemetryChart() {
    updateChartLegend();
    renderTelemetryChart();
}

function setChartMetric(metric) {
    chartActiveMetric = metric;
    document.querySelectorAll('.chart-btn').forEach(b => {
        const oc = b.getAttribute('onclick') || '';
        if (oc.includes(`'${metric}'`)) b.classList.add('active');
        else b.classList.remove('active');
    });
    updateChartLegend();
    renderTelemetryChart();
}

function updateChartLegend() {
    const legEl = document.getElementById('chart-legend');
    if (!legEl) return;

    if (chartActiveMetric === 'dist') {
        legEl.innerHTML = `
            <div class="legend-item"><span class="legend-color" style="background:#38bdf8;"></span><span>Laser (mm / 10)</span></div>
            <div class="legend-item"><span class="legend-color" style="background:#10b981;"></span><span>Ultrazvuk (cm)</span></div>
            <div class="legend-item"><span class="legend-color" style="background:#ef4444;"></span><span>IR senzor (cm)</span></div>
        `;
    } else if (chartActiveMetric === 'env') {
        legEl.innerHTML = `
            <div class="legend-item"><span class="legend-color" style="background:#f59e0b;"></span><span>Teplota (°C)</span></div>
            <div class="legend-item"><span class="legend-color" style="background:#38bdf8;"></span><span>Vlhkost (%)</span></div>
        `;
    } else if (chartActiveMetric === 'imu') {
        legEl.innerHTML = `
            <div class="legend-item"><span class="legend-color" style="background:#ef4444;"></span><span>Ax (g)</span></div>
            <div class="legend-item"><span class="legend-color" style="background:#10b981;"></span><span>Ay (g)</span></div>
            <div class="legend-item"><span class="legend-color" style="background:#38bdf8;"></span><span>Az (g)</span></div>
        `;
    }
}

function pushTelemetryToChart(d) {
    // Přidání nových bodů
    if (d.laser !== undefined) pushPoint(chartData.laser, d.laser / 10); // Laser škálujeme na cm
    if (d.ultra !== undefined) pushPoint(chartData.ultra, d.ultra);
    if (d.ir !== undefined) pushPoint(chartData.ir, d.ir);

    if (d.t !== undefined && d.t > -100) pushPoint(chartData.temp, d.t);
    if (d.h !== undefined && d.h >= 0) pushPoint(chartData.hum, d.h);

    if (d.ax !== undefined) pushPoint(chartData.ax, d.ax);
    if (d.ay !== undefined) pushPoint(chartData.ay, d.ay);
    if (d.az !== undefined) pushPoint(chartData.az, d.az);

    renderTelemetryChart();
}

function pushPoint(arr, val) {
    arr.push(val);
    if (arr.length > CHART_MAX_POINTS) arr.shift();
}

function renderTelemetryChart() {
    const canvas = document.getElementById('telemetry-chart');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const w = canvas.width;
    const h = canvas.height;

    ctx.clearRect(0, 0, w, h);

    // Mřížka pozadí
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
    ctx.lineWidth = 1;
    for (let y = 30; y < h; y += 35) {
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(w, y);
        ctx.stroke();
    }

    let series = [];
    let minY = 0;
    let maxY = 100;

    if (chartActiveMetric === 'dist') {
        series = [
            { data: chartData.laser, color: '#38bdf8' },
            { data: chartData.ultra, color: '#10b981' },
            { data: chartData.ir,    color: '#ef4444' }
        ];
        minY = 0;
        maxY = 120;
    } else if (chartActiveMetric === 'env') {
        series = [
            { data: chartData.temp, color: '#f59e0b' },
            { data: chartData.hum,  color: '#38bdf8' }
        ];
        minY = 0;
        maxY = 100;
    } else if (chartActiveMetric === 'imu') {
        series = [
            { data: chartData.ax, color: '#ef4444' },
            { data: chartData.ay, color: '#10b981' },
            { data: chartData.az, color: '#38bdf8' }
        ];
        minY = -2;
        maxY = 2;
    }

    // Vykreslení jednotlivých křivek
    series.forEach(s => {
        if (!s.data || s.data.length < 2) return;
        ctx.strokeStyle = s.color;
        ctx.lineWidth = 2.5;
        ctx.lineJoin = 'round';
        ctx.beginPath();

        const stepX = w / (CHART_MAX_POINTS - 1);
        const startOffset = CHART_MAX_POINTS - s.data.length;

        for (let i = 0; i < s.data.length; i++) {
            const x = (startOffset + i) * stepX;
            const val = s.data[i];
            const clampedVal = Math.max(minY, Math.min(maxY, val));
            const y = h - ((clampedVal - minY) / (maxY - minY)) * (h - 20) - 10;

            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();
    });
}

// --- 9. OSTATNÍ OVLÁDACÍ PRVKY ---

// Přepnutí herního módu na displeji ESP
function setAppMode(mode) {
    sendCmd('setMode', { mode: parseInt(mode) });
}

// Odeslání barvy a jasu na RGB pásek WS2812B
function updateRgbStrip() {
    const hex = document.getElementById('ctrl-rgb-color').value;
    const r = parseInt(hex.substr(1,2), 16);
    const g = parseInt(hex.substr(3,2), 16);
    const b = parseInt(hex.substr(5,2), 16);
    const bright = parseInt(document.getElementById('ctrl-rgb-bright').value);
    sendCmd('setLedStrip', { r, g, b, bright });
}

const updateRgbBright = throttle((bright) => {
    setText('lbl-rgb-bright', bright);
    updateRgbStrip();
}, 50);

function setRgbPreset(hex) {
    document.getElementById('ctrl-rgb-color').value = hex;
    updateRgbStrip();
}

// Pípnutí bzučákem
function beep(freq, durationMs) {
    sendCmd('beep', { freq: parseInt(freq), duration: parseInt(durationMs) });
}

// Spuštění kalibrace Wi-Fi CSI radaru
function calibrateCsi() {
    sendCmd('calibrateCsi');
}

// Virtuální stisk tlačítek D-Padu
function sendBtn(index, isPressed) {
    sendCmd('pressBtn', { idx: parseInt(index), pressed: isPressed });
}

)rawliteral";

#endif // WEB_PAGES_H
