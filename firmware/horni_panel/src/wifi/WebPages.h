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
    <!-- Nastavení kódování znaků na UTF-8 (aby fungovala česká diakritika: háčky a čárky) -->
    <meta charset="UTF-8">
    
    <!-- Nastavení měřítka zobrazení na mobilu: šířka odpovídá zařízení, zakázáno nechtěné přibližování -->
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    
    <title>ESP-Demo-Box</title>
    
    <!-- Odkaz na externí soubor s kaskádovými styly (vzhled, barvy, písmo) -->
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <!-- ========================================================================= -->
    <!-- HORNY LIŠTA (HEADER) S LOGEM A INDIKÁTOREM STAVU SPOJENÍ -->
    <!-- ========================================================================= -->
    <header class="app-header">
        <div class="logo-area">
            <span class="logo-icon">⚡</span>
            <div>
                <h1>ESP-Demo-Box</h1>
                <p class="subtitle">Bezdrátový řídicí panel</p>
            </div>
        </div>
        
        <!-- Odznáček stavu WebSocketu: zelená = Online, žlutá = Připojování, červená = Odpojeno -->
        <div id="status-badge" class="status-badge connecting">
            <span class="dot"></span>
            <span id="status-text">Připojování...</span>
        </div>
    </header>

    <!-- ========================================================================= -->
    <!-- NAVIGAČNÍ TLAČÍTKA PRO PŘEPÍNÁNÍ ZÁLOŽEK -->
    <!-- ========================================================================= -->
    <nav class="tab-bar">
        <!-- Po kliknutí zavolá JavaScriptovou funkci switchTab() pro zobrazení danej sekce -->
        <button class="tab-btn active" onclick="switchTab('telemetry')">📊 Telemetrie</button>
        <button class="tab-btn" onclick="switchTab('control')">🎛️ Ovládání</button>
        <button class="tab-btn" onclick="switchTab('system')">⚙️ Systém</button>
    </nav>

    <!-- ========================================================================= -->
    <!-- HLAVNÍ OBSAH (KONTEJNER SE ZÁLOŽKAMI) -->
    <!-- ========================================================================= -->
    <main class="container">

        <!-- --------------------------------------------------------------------- -->
        <!-- ZÁLOŽKA 1: TELEMETRIE (ŽIVÉ ZOBRAZENÍ HODNOT ZE SENZORŮ) -->
        <!-- --------------------------------------------------------------------- -->
        <section id="tab-telemetry" class="tab-content active">
            
            <!-- Karta: Teplota a Vlhkost (DHT11) -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">🌡️</span>
                    <h3>Klima a Prostředí</h3>
                </div>
                <div class="grid-2">
                    <div class="metric-box">
                        <span class="metric-label">Teplota</span>
                        <!-- ID pro dynamický přepis z JavaScriptu: val-temp -->
                        <span class="metric-value"><span id="val-temp">--</span> °C</span>
                    </div>
                    <div class="metric-box">
                        <span class="metric-label">Vlhkost</span>
                        <!-- ID pro dynamický přepis z JavaScriptu: val-hum -->
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
                    <span>IR Překážka:</span>
                    <span id="val-ir-obs" class="tag">Není</span>
                </div>
            </div>

            <!-- Karta: Wi-Fi Telemetrie a CSI Radar -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">📡</span>
                    <h3>Wi-Fi & CSI Pohybový Radar</h3>
                </div>
                <div class="grid-2">
                    <div class="metric-box">
                        <span class="metric-label">Vzdálenost mobilu</span>
                        <span class="metric-value" id="val-wifi-dist">-- m</span>
                    </div>
                    <div class="metric-box">
                        <span class="metric-label">CSI Signál / Odchylka</span>
                        <span class="metric-value" id="val-csi-metric">--</span>
                    </div>
                </div>
                <div class="sub-metric mt-2">
                    <span>Stav detekce:</span>
                    <span id="val-csi-motion" class="tag">KLID</span>
                </div>
                <div class="text-center mt-2">
                    <button class="action-btn" onclick="calibrateCsi()">🔄 Znovu kalibrovat radar (5s)</button>
                </div>
            </div>

            <!-- Karta: Pohybový senzor IMU (Gyroskop a Akcelerometr LSM6DS3) -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">🧭</span>
                    <h3>Pohyb & IMU (LSM6DS3)</h3>
                </div>
                <div class="imu-container">
                    <div>
                        <div class="imu-title">Akcelerometr [g]</div>
                        <div class="grid-3 text-center">
                            <div class="imu-val">X: <b id="val-ax">0.00</b></div>
                            <div class="imu-val">Y: <b id="val-ay">0.00</b></div>
                            <div class="imu-val">Z: <b id="val-az">0.00</b></div>
                        </div>
                    </div>
                    <div class="mt-2">
                        <div class="imu-title">Gyroskop [°/s]</div>
                        <div class="grid-3 text-center">
                            <div class="imu-val">X: <b id="val-gx">0.00</b></div>
                            <div class="imu-val">Y: <b id="val-gy">0.00</b></div>
                            <div class="imu-val">Z: <b id="val-gz">0.00</b></div>
                        </div>
                    </div>
                </div>
            </div>

            <!-- Karta: Světlo (Fotorezistory) a Barevný senzor (TCS34725) -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">🎨</span>
                    <h3>Světlo & Barva</h3>
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
                <!-- Náhled změřené barvy se dynamicky vybarvuje v JavaScriptu -->
                <div class="color-preview-box mt-2">
                    <div id="color-preview" class="color-circle"></div>
                    <div class="color-text">
                        <span>R: <b id="val-cr">0</b>, G: <b id="val-cg">0</b>, B: <b id="val-cb">0</b></span>
                    </div>
                </div>
            </div>

            <!-- Karta: Fyzické vstupy (Joystick, Enkodér, Potenciometr, Tlačítka) -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">🕹️</span>
                    <h3>Vstupy & Tlačítka</h3>
                </div>
                <div class="inputs-grid">
                    <div><b>Joystick:</b> X=<span id="val-jx">0</span>, Y=<span id="val-jy">0</span> <span id="val-jbtn" class="tag">UP</span></div>
                    <div><b>Potenciometr:</b> <span id="val-pot">0</span></div>
                    <div><b>Enkodér:</b> <span id="val-enc">0</span> <span id="val-ebtn" class="tag">UP</span></div>
                    <div><b>Horní tlačítko:</b> <span id="val-btntop" class="tag">UP</span></div>
                    <div><b>Přepínače:</b> SW1: <span id="val-sw1">OFF</span>, SW2: <span id="val-sw2">OFF</span></div>
                    <div class="btn-group-down mt-1">
                        <b>Spodní tlačítka:</b>
                        <span id="btn-d1" class="key-pill">1</span>
                        <span id="btn-d2" class="key-pill">2</span>
                        <span id="btn-d3" class="key-pill">3</span>
                        <span id="btn-d4" class="key-pill">4</span>
                        <span id="btn-d5" class="key-pill">5</span>
                    </div>
                </div>
            </div>
        </section>

        <!-- --------------------------------------------------------------------- -->
        <!-- ZÁLOŽKA 2: OVLÁDÁNÍ (OVLÁDACÍ PRVKY PRO SERVA, LEDKY, MOTORY, MÓDY) -->
        <!-- --------------------------------------------------------------------- -->
        <section id="tab-control" class="tab-content">
            
            <!-- Výběr aktivního módu obrazovky na ESP32 -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">📱</span>
                    <h3>Režim obrazovky ESP</h3>
                </div>
                <div class="mode-grid">
                    <button class="mode-btn" onclick="setAppMode(0)">📋 Menu</button>
                    <button class="mode-btn" onclick="setAppMode(1)">🧭 Gyroskop</button>
                    <button class="mode-btn" onclick="setAppMode(2)">🐍 Snake</button>
                    <button class="mode-btn" onclick="setAppMode(3)">🐦 Flappy</button>
                    <button class="mode-btn" onclick="setAppMode(4)">🔢 2048</button>
                    <button class="mode-btn" onclick="setAppMode(5)">📐 Vzdálenost</button>
                    <button class="mode-btn" onclick="setAppMode(6)">📶 Wi-Fi QR</button>
                    <button class="mode-btn" onclick="setAppMode(7)">📡 CSI Radar</button>
                    <button class="mode-btn" onclick="setAppMode(8)">🦾 Serva</button>
                    <button class="mode-btn" onclick="setAppMode(9)">⚙️ Motor</button>
                    <button class="mode-btn" onclick="setAppMode(10)">🌈 Barvy</button>
                    <button class="mode-btn btn-sleep" onclick="setAppMode(11)">💤 Spánek</button>
                </div>
            </div>

            <!-- Přepínače 3x stavových LED diod -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">💡</span>
                    <h3>Stavové LED Diody</h3>
                </div>
                <div class="led-controls">
                    <div class="led-item">
                        <span>LED 1</span>
                        <label class="switch">
                            <input type="checkbox" id="ctrl-led1" onchange="toggleLed(1, this.checked)">
                            <span class="slider"></span>
                        </label>
                    </div>
                    <div class="led-item">
                        <span>LED 2</span>
                        <label class="switch">
                            <input type="checkbox" id="ctrl-led2" onchange="toggleLed(2, this.checked)">
                            <span class="slider"></span>
                        </label>
                    </div>
                    <div class="led-item">
                        <span>LED 3</span>
                        <label class="switch">
                            <input type="checkbox" id="ctrl-led3" onchange="toggleLed(3, this.checked)">
                            <span class="slider"></span>
                        </label>
                    </div>
                </div>
            </div>

            <!-- Ovládání RGB LED pásku WS2812B -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">✨</span>
                    <h3>RGB LED Pásek</h3>
                </div>
                <!-- Paleta barev (Color picker) -->
                <div class="control-row">
                    <label>Barva pásku:</label>
                    <input type="color" id="ctrl-rgb-color" value="#ff0000" onchange="updateRgbStrip()">
                </div>
                <!-- Posuvník jasu -->
                <div class="control-row mt-2">
                    <label>Jas: <b id="lbl-rgb-bright">60</b></label>
                    <input type="range" id="ctrl-rgb-bright" min="0" max="255" value="60" oninput="updateRgbBright(this.value)">
                </div>
                <!-- Rychlá tlačítka barevných předvoleb -->
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

            <!-- Posuvníky pro Serva a Motor -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">⚙️</span>
                    <h3>Pohony & Serva</h3>
                </div>
                <!-- Klasické servo 0 až 180 stupňů -->
                <div class="slider-group">
                    <div class="slider-header">
                        <span>Klasické Servo (0–180°):</span>
                        <b id="lbl-servo">90°</b>
                    </div>
                    <input type="range" id="ctrl-servo" min="0" max="180" value="90" oninput="onServoChange(this.value)">
                </div>

                <!-- Chytré servo -180 až +180 stupňů -->
                <div class="slider-group mt-2">
                    <div class="slider-header">
                        <span>Chytré Servo (-180° až +180°):</span>
                        <b id="lbl-smartservo">0°</b>
                    </div>
                    <input type="range" id="ctrl-smartservo" min="-180" max="180" value="0" oninput="onSmartServoChange(this.value)">
                </div>

                <!-- Rychlost a směr motoru (-255 až +255) -->
                <div class="slider-group mt-2">
                    <div class="slider-header">
                        <span>Motor / PWM Rychlost:</span>
                        <b id="lbl-motor">0</b>
                    </div>
                    <input type="range" id="ctrl-motor" min="-255" max="255" value="0" oninput="onMotorChange(this.value)">
                    <div class="text-center mt-1">
                        <button class="btn-sm" onclick="stopMotor()">⏹ Zastavit motor</button>
                    </div>
                </div>
            </div>

            <!-- Ovládání bzučáku -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">🔔</span>
                    <h3>Bzučák</h3>
                </div>
                <div class="buzzer-actions">
                    <button class="action-btn" onclick="beep(1000, 200)">🔊 Pípnout (1 kHz)</button>
                    <button class="action-btn" onclick="beep(2000, 200)">🔊 Pípnout (2 kHz)</button>
                    <button class="action-btn" onclick="beep(500, 400)">📢 Hluboký tón</button>
                </div>
            </div>

            <!-- Dálkový ovladač / 5 tlačítek do kříže (D-Pad) -->
            <div class="card">
                <div class="card-header">
                    <span class="card-icon">🎮</span>
                    <h3>Dálkový ovladač (5 tlačítek do kříže)</h3>
                </div>
                <div class="dpad-container">
                    <div class="dpad-row">
                        <button class="dpad-btn" 
                                onmousedown="sendBtn(0, true)" onmouseup="sendBtn(0, false)"
                                ontouchstart="sendBtn(0, true)" ontouchend="sendBtn(0, false)">
                            ▲<small>Tlačítko 1</small>
                        </button>
                    </div>
                    <div class="dpad-row">
                        <button class="dpad-btn" 
                                onmousedown="sendBtn(1, true)" onmouseup="sendBtn(1, false)"
                                ontouchstart="sendBtn(1, true)" ontouchend="sendBtn(1, false)">
                            ◀<small>Tlačítko 2</small>
                        </button>
                        <button class="dpad-btn dpad-center" 
                                onmousedown="sendBtn(2, true)" onmouseup="sendBtn(2, false)"
                                ontouchstart="sendBtn(2, true)" ontouchend="sendBtn(2, false)">
                            ●<small>OK / 3</small>
                        </button>
                        <button class="dpad-btn" 
                                onmousedown="sendBtn(3, true)" onmouseup="sendBtn(3, false)"
                                ontouchstart="sendBtn(3, true)" ontouchend="sendBtn(3, false)">
                            ▶<small>Tlačítko 4</small>
                        </button>
                    </div>
                    <div class="dpad-row">
                        <button class="dpad-btn" 
                                onmousedown="sendBtn(4, true)" onmouseup="sendBtn(4, false)"
                                ontouchstart="sendBtn(4, true)" ontouchend="sendBtn(4, false)">
                            ▼<small>Tlačítko 5</small>
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
                    <div class="sys-row"><span>Klienti WebSocket:</span><b id="sys-clients">1</b></div>
                    <div class="sys-row"><span>Wi-Fi SSID:</span><b>ESP-Demo-Box</b></div>
                    <div class="sys-row"><span>IP adresa:</span><b>192.168.4.1</b></div>
                </div>
            </div>
        </section>
    </main>

    <!-- Patička webu -->
    <footer class="app-footer">
        <span>ESP-Demo-Box © 2026</span>
    </footer>

    <!-- Načtení JavaScriptového skriptu pro spojení s WebSocketem -->
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
   Je navržen v moderním tmavém motivu (Dark Mode) pro skvělý vzhled na mobilu.
   ============================================================================= */

/* --- 1. DEFINICE GLOBÁLNÍCH BAREVNÝCH PROMĚNNÝCH (:root) --- */
:root {
    --bg-color: #0f172a;       /* Tmavě modré pozadí celé stránky */
    --card-bg: #1e293b;        /* Světlejší tmavé pozadí jednotlivých karet */
    --card-border: #334155;    /* Barva jemného rámečku kolem karet */
    --primary: #38bdf8;        /* Hlavní zvýrazňovací azurová barva */
    --primary-hover: #0ea5e9;  /* Barva tlačítka při stisku */
    --accent: #6366f1;         /* Sekundární fialová barva */
    --text-main: #f8fafc;      /* Hlavní bílý text */
    --text-muted: #94a3b8;     /* Šedý doplňkový text */
    --success: #22c55e;        /* Zelená pro stav Online a úspěch */
    --danger: #ef4444;         /* Červená pro odpojení a zastavení */
    --warning: #f59e0b;        /* Žlutá pro varování a připojování */
    --radius: 12px;            /* Poloměr zakulacení rohů karet a tlačítek */
}

/* Reseting okrajů a výchozího nastavení prvků */
* {
    box-sizing: border-box;
    margin: 0;
    padding: 0;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    -webkit-tap-highlight-color: transparent; /* Vypne modré bliknutí při klepnutí prstem na mobilu */
}

body {
    background-color: var(--bg-color);
    color: var(--text-main);
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    padding-bottom: 20px;
}

/* --- 2. HORNÍ LIŠTA (HEADER) --- */
.app-header {
    background: var(--card-bg);
    border-bottom: 1px solid var(--card-border);
    padding: 14px 20px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    position: sticky; /* Lišta zůstane přišpendlená nahoře i při skrolování */
    top: 0;
    z-index: 50;
}

.logo-area {
    display: flex;
    align-items: center;
    gap: 12px;
}

.logo-icon {
    font-size: 24px;
    background: #0369a1;
    padding: 6px 10px;
    border-radius: 8px;
}

.app-header h1 {
    font-size: 1.15rem;
    font-weight: 700;
}

.subtitle {
    font-size: 0.75rem;
    color: var(--text-muted);
}

/* Odznáček stavu spojení (Připojování / Online / Odpojeno) */
.status-badge {
    display: flex;
    align-items: center;
    gap: 6px;
    font-size: 0.75rem;
    padding: 4px 10px;
    border-radius: 20px;
    background: #334155;
}

.status-badge .dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: var(--warning);
}

/* Stav Online (Zelené světlo) */
.status-badge.online {
    background: rgba(34, 197, 94, 0.15);
    color: var(--success);
}
.status-badge.online .dot {
    background: var(--success);
    box-shadow: 0 0 8px var(--success);
}

/* Stav Odpojeno (Červené světlo) */
.status-badge.offline {
    background: rgba(239, 68, 68, 0.15);
    color: var(--danger);
}
.status-badge.offline .dot {
    background: var(--danger);
}

/* --- 3. LIŠTA ZÁLOŽEK --- */
.tab-bar {
    display: flex;
    background: #111827;
    border-bottom: 1px solid var(--card-border);
    padding: 4px;
    gap: 4px;
}

.tab-btn {
    flex: 1;
    background: transparent;
    border: none;
    color: var(--text-muted);
    padding: 10px 8px;
    font-size: 0.85rem;
    font-weight: 600;
    border-radius: 8px;
    cursor: pointer;
    transition: all 0.2s ease;
}

/* Aktivní záložka */
.tab-btn.active {
    background: var(--card-bg);
    color: var(--primary);
    box-shadow: 0 2px 8px rgba(0,0,0,0.3);
}

/* --- 4. KARTY A HLAVNÍ OBSAH --- */
.container {
    padding: 16px;
    flex: 1;
    max-width: 600px;
    margin: 0 auto;
    width: 100%;
}

.tab-content {
    display: none; /* Neaktivní záložky skryjeme */
    flex-direction: column;
    gap: 14px;
}

.tab-content.active {
    display: flex; /* Aktivní záložku zobrazíme */
}

/* Vzhled samostatné karty */
.card {
    background: var(--card-bg);
    border: 1px solid var(--card-border);
    border-radius: var(--radius);
    padding: 16px;
    box-shadow: 0 4px 12px rgba(0,0,0,0.15);
}

.card-header {
    display: flex;
    align-items: center;
    gap: 8px;
    margin-bottom: 12px;
}

.card-header h3 {
    font-size: 0.95rem;
    font-weight: 600;
    color: var(--text-main);
}

.card-icon {
    font-size: 1.1rem;
}

/* --- 5. ROZLOŽENÍ DATOVÝCH POLÍ (GRID) --- */
.grid-2 {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
}

.grid-3 {
    display: grid;
    grid-template-columns: 1fr 1fr 1fr;
    gap: 8px;
}

.metric-box {
    background: rgba(15, 23, 42, 0.6);
    border: 1px solid rgba(255,255,255,0.05);
    border-radius: 8px;
    padding: 10px;
    text-align: center;
}

.metric-label {
    display: block;
    font-size: 0.72rem;
    color: var(--text-muted);
    margin-bottom: 4px;
}

.metric-value {
    font-size: 1.15rem;
    font-weight: 700;
    color: var(--primary);
}

.sub-metric {
    font-size: 0.8rem;
    color: var(--text-muted);
    display: flex;
    align-items: center;
    justify-content: space-between;
}

/* Štítek / Visačka (nástrojový stav) */
.tag {
    background: #334155;
    padding: 2px 8px;
    border-radius: 4px;
    font-size: 0.75rem;
    font-weight: 600;
    color: #fff;
}
.tag.active {
    background: var(--primary);
    color: #000;
}

.imu-title {
    font-size: 0.75rem;
    color: var(--text-muted);
    margin-bottom: 4px;
}

.imu-val {
    background: rgba(15, 23, 42, 0.6);
    padding: 6px;
    border-radius: 6px;
    font-size: 0.8rem;
}

.color-preview-box {
    display: flex;
    align-items: center;
    gap: 12px;
    background: rgba(15, 23, 42, 0.6);
    padding: 8px 12px;
    border-radius: 8px;
}

.color-circle {
    width: 32px;
    height: 32px;
    border-radius: 50%;
    border: 2px solid #fff;
    background: #000;
}

.color-text {
    font-size: 0.85rem;
}

.inputs-grid {
    display: flex;
    flex-direction: column;
    gap: 6px;
    font-size: 0.82rem;
    color: var(--text-muted);
}
.inputs-grid b {
    color: var(--text-main);
}

.key-pill {
    display: inline-block;
    width: 22px;
    height: 22px;
    line-height: 22px;
    text-align: center;
    border-radius: 4px;
    background: #334155;
    color: #94a3b8;
    font-size: 0.75rem;
    margin-left: 4px;
}
.key-pill.active {
    background: var(--primary);
    color: #000;
    font-weight: bold;
}

/* --- 6. STYLOVÁNÍ OVLÁDACÍCH TLAČÍTEK --- */
.mode-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 8px;
}

.mode-btn {
    background: #334155;
    border: 1px solid #475569;
    color: #fff;
    padding: 10px 8px;
    border-radius: 8px;
    font-size: 0.85rem;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.15s ease;
}
.mode-btn:active {
    background: var(--primary);
    color: #000;
    transform: scale(0.97);
}
.mode-btn.current {
    background: var(--primary);
    color: #000;
    border-color: #38bdf8;
    box-shadow: 0 0 10px rgba(56, 189, 248, 0.4);
}
.btn-sleep {
    grid-column: span 2;
    background: #451a03;
    border-color: #78350f;
    color: #fed7aa;
}

/* --- 7. DOTYKOVÉ PŘEPÍNAČE (SWITCHES FOR LEDS) --- */
.led-controls {
    display: flex;
    justify-content: space-around;
    padding: 6px 0;
}

.led-item {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 8px;
    font-size: 0.85rem;
    font-weight: 600;
}

.switch {
    position: relative;
    display: inline-block;
    width: 46px;
    height: 26px;
}
.switch input { opacity: 0; width: 0; height: 0; }
.slider {
    position: absolute;
    cursor: pointer;
    top: 0; left: 0; right: 0; bottom: 0;
    background-color: #334155;
    transition: .3s;
    border-radius: 26px;
}
.slider:before {
    position: absolute;
    content: "";
    height: 18px;
    width: 18px;
    left: 4px;
    bottom: 4px;
    background-color: white;
    transition: .3s;
    border-radius: 50%;
}
input:checked + .slider { background-color: var(--primary); }
input:checked + .slider:before { transform: translateX(20px); }

.control-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
}

input[type="color"] {
    border: none;
    width: 44px;
    height: 34px;
    border-radius: 6px;
    background: transparent;
    cursor: pointer;
}

input[type="range"] {
    width: 100%;
    margin-top: 6px;
    accent-color: var(--primary);
    height: 6px;
    border-radius: 3px;
    background: #334155;
    cursor: pointer;
}

.slider-group {
    background: rgba(15, 23, 42, 0.5);
    padding: 10px;
    border-radius: 8px;
}
.slider-header {
    display: flex;
    justify-content: space-between;
    font-size: 0.82rem;
    color: var(--text-muted);
}
.slider-header b {
    color: var(--primary);
}

.color-presets {
    display: flex;
    justify-content: space-between;
    gap: 6px;
}
.preset-btn {
    flex: 1;
    height: 28px;
    border-radius: 6px;
    border: 1px solid #475569;
    cursor: pointer;
}

.buzzer-actions {
    display: flex;
    flex-direction: column;
    gap: 8px;
}
.action-btn {
    background: #334155;
    border: 1px solid #475569;
    color: #fff;
    padding: 10px;
    border-radius: 8px;
    font-size: 0.85rem;
    font-weight: 600;
    cursor: pointer;
}
.action-btn:active {
    background: var(--accent);
}

.btn-sm {
    background: #ef4444;
    color: #fff;
    border: none;
    padding: 6px 12px;
    border-radius: 6px;
    font-size: 0.75rem;
    font-weight: bold;
    cursor: pointer;
}

/* --- 8. SYSTÉMOVÁ TABULKA --- */
.sys-info-table {
    display: flex;
    flex-direction: column;
    gap: 10px;
    font-size: 0.85rem;
}
.sys-row {
    display: flex;
    justify-content: space-between;
    border-bottom: 1px solid rgba(255,255,255,0.05);
    padding-bottom: 6px;
}
.sys-row span { color: var(--text-muted); }

.mt-1 { margin-top: 6px; }
.mt-2 { margin-top: 10px; }
.text-center { text-align: center; }

/* --- 9. D-PAD DÁLKOVÝ OVLADAČ (5 TLAČÍTEK DO KŘÍŽE) --- */
.dpad-container {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 8px;
    margin: 15px auto 5px;
    max-width: 260px;
    user-select: none;
}

.dpad-row {
    display: flex;
    justify-content: center;
    gap: 8px;
    width: 100%;
}

.dpad-btn {
    width: 72px;
    height: 72px;
    border-radius: 16px;
    background: #334155;
    border: 2px solid #475569;
    color: var(--text-main);
    font-size: 1.25rem;
    font-weight: 700;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    cursor: pointer;
    transition: all 0.1s ease;
    touch-action: manipulation;
    box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.3);
}

.dpad-btn small {
    font-size: 0.65rem;
    font-weight: 500;
    color: var(--text-muted);
    margin-top: 2px;
}

.dpad-btn:active, .dpad-btn.active {
    background: var(--primary);
    border-color: var(--primary-hover);
    color: #0f172a;
    transform: scale(0.93);
    box-shadow: 0 0 15px rgba(56, 189, 248, 0.5);
}

.dpad-btn:active small, .dpad-btn.active small {
    color: #0f172a;
    font-weight: 700;
}

.dpad-center {
    background: #1e293b;
    border-color: var(--accent);
    color: var(--primary);
}

/* Patička */
.app-footer {
    text-align: center;
    font-size: 0.75rem;
    color: var(--text-muted);
    padding: 10px;
}

)rawliteral";

// --- 3. JAVASCRIPT KLIENT ---
const char SCRIPT_JS[] PROGMEM = R"rawliteral(
// =============================================================================
// JAVASCRIPT PRO OVLÁDACÍ PANEL ESP-DEMO-BOX
// =============================================================================

// Globální proměnné pro správu spojení a stavu
let ws = null;              // Proměnná držící otevřený WebSocket objekt
let reconnectTimer = null;  // Časovač pro automatické obnovení spojení při výpadku
let currentAppMode = 0;     // Číslo aktuálního aktivního režimu na ESP32

// Názvy jednotlivých módů pro hezké vypsání v záložce Systém
const modeNames = [
    "Hlavní menu", "Gyroskop", "Hra Snake", "Hra Flappy", "Hra 2048",
    "Měření vzdálenosti", "Wi-Fi QR spojení", "Wi-Fi CSI Radar", "Ovládání serv", "Ovládání motoru",
    "Barevný senzor", "Spánkový režim"
];

// --- 1. Spuštění při načtení stránky v prohlížeči ---
window.addEventListener('load', () => {
    initWebSocket(); // Ihned po otevření webu navážeme spojení s ESP32
});

// --- 2. Přepínání záložek (Telemetrie / Ovládání / Systém) ---
function switchTab(tabId) {
    // Odznačíme všechna tlačítka záložek a skryjeme všechny sekce
    document.querySelectorAll('.tab-btn').forEach(btn => btn.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(content => content.classList.remove('active'));

    // Najdeme kliknuté tlačítko a aktivujeme ho
    const activeBtn = Array.from(document.querySelectorAll('.tab-btn')).find(b => b.getAttribute('onclick').includes(tabId));
    if (activeBtn) activeBtn.classList.add('active');

    // Zobrazíme odpovídající sekci s obsahem
    const activeContent = document.getElementById(`tab-${tabId}`);
    if (activeContent) activeContent.classList.add('active');
}

// --- 3. Inicializace a správa WebSocket spojení ---
function initWebSocket() {
    // Sestavíme adresu: "ws://" + IP adresa v prohlížeči (např. 192.168.4.1) + "/ws"
    const wsUrl = `ws://${location.host}/ws`;
    const statusBadge = document.getElementById('status-badge');
    const statusText = document.getElementById('status-text');

    // Nastavíme odznáček stavu na "Připojování..."
    statusBadge.className = 'status-badge connecting';
    statusText.innerText = 'Připojování...';

    try {
        ws = new WebSocket(wsUrl); // Pokusíme se otevřít WebSocket linku

        // Událost: Spojení úspěšně navázáno
        ws.onopen = () => {
            statusBadge.className = 'status-badge online'; // Zelený odznáček
            statusText.innerText = 'Online';
            // Pokud běžel časovač pro znovupřipojení, zrušíme ho
            if (reconnectTimer) {
                clearTimeout(reconnectTimer);
                reconnectTimer = null;
            }
        };

        // Událost: Spojení se přerušilo (např. ESP32 se restartovalo nebo jsme daleko od Wi-Fi)
        ws.onclose = () => {
            statusBadge.className = 'status-badge offline'; // Červený odznáček
            statusText.innerText = 'Odpojeno';
            scheduleReconnect(); // Začneme se automaticky znovu připojovat
        };

        // Událost: Chyba spojení
        ws.onerror = (err) => {
            console.error('WS Chyba:', err);
            ws.close(); // Uzavřeme spojení a spustíme reconnect
        };

        // Událost: Z ESP32 dorazila nová zpráva (telemetrie)
        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data); // Převedeme text JSONu na JavaScriptový objekt
                handleTelemetry(data);               // Předáme data k vykreslení do HTML
            } catch (e) {
                console.error('Chyba parsovani zpravy:', e, event.data);
            }
        };
    } catch (e) {
        statusBadge.className = 'status-badge offline';
        statusText.innerText = 'Chyba';
        scheduleReconnect();
    }
}

// Funkce pro naplánování znovupřipojení za 2 sekundy
function scheduleReconnect() {
    if (!reconnectTimer) {
        reconnectTimer = setTimeout(() => {
            reconnectTimer = null;
            initWebSocket(); // Zkusíme se znovu připojit
        }, 2000);
    }
}

// Odeslání JSON příkazu z webu do ESP32
function sendCmd(cmd, payload = {}) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        // Zabalíme příkaz a doplňující data do jednoho JSON řetězce
        const msg = JSON.stringify({ cmd, ...payload });
        ws.send(msg); // Odešleme přes WebSocket do ESP32
    }
}

// --- 4. Zpracování příchozích dat ze senzorů a přepsání hodnot v HTML ---
function handleTelemetry(d) {
    // 1. Prostředí (Teplota a Vlhkost)
    if (d.t !== undefined) setText('val-temp', d.t > -100 ? d.t.toFixed(1) : '--');
    if (d.h !== undefined) setText('val-hum', d.h >= 0 ? d.h.toFixed(1) : '--');

    // 2. Vzdálenosti
    if (d.laser !== undefined) setText('val-laser', d.laser);
    if (d.ultra !== undefined) setText('val-ultra', d.ultra >= 0 ? d.ultra.toFixed(1) : '--');
    if (d.ir !== undefined) setText('val-ir', d.ir >= 0 ? d.ir.toFixed(1) : '--');
    
    // Wi-Fi Telemetrie & CSI Radar
    if (d.wifi_dist !== undefined) setText('val-wifi-dist', d.wifi_dist > 0.1 ? d.wifi_dist.toFixed(1) + ' m' : '---');
    if (d.csi_metric !== undefined) setText('val-csi-metric', d.csi_metric.toFixed(2));
    if (d.csi_motion !== undefined) {
        const motEl = document.getElementById('val-csi-motion');
        if (motEl) {
            motEl.innerText = d.csi_motion ? '🚨 POHYB DETEKOVÁN!' : 'KLID';
            motEl.className = d.csi_motion ? 'tag active' : 'tag';
        }
    }
    
    // IR detekce překážky (změna barvy visačky)
    if (d.irobs !== undefined) {
        const obsEl = document.getElementById('val-ir-obs');
        if (obsEl) {
            obsEl.innerText = d.irobs ? 'PŘEKÁŽKA!' : 'Čisté';
            obsEl.className = d.irobs ? 'tag active' : 'tag';
        }
    }

    // 3. Gyroskop a Akcelerometr (LSM6DS3)
    if (d.ax !== undefined) setText('val-ax', d.ax.toFixed(2));
    if (d.ay !== undefined) setText('val-ay', d.ay.toFixed(2));
    if (d.az !== undefined) setText('val-az', d.az.toFixed(2));
    if (d.gx !== undefined) setText('val-gx', d.gx.toFixed(1));
    if (d.gy !== undefined) setText('val-gy', d.gy.toFixed(1));
    if (d.gz !== undefined) setText('val-gz', d.gz.toFixed(1));

    // 4. Světlo a Barva
    if (d.p1 !== undefined) setText('val-photo1', d.p1);
    if (d.p2 !== undefined) setText('val-photo2', d.p2);
    if (d.cr !== undefined && d.cg !== undefined && d.cb !== undefined) {
        setText('val-cr', d.cr);
        setText('val-cg', d.cg);
        setText('val-cb', d.cb);
        // Obarvíme náhledové kolečko skutečnou barvou, kterou senzor vidí
        const colPrev = document.getElementById('color-preview');
        if (colPrev) {
            colPrev.style.backgroundColor = `rgb(${d.cr}, ${d.cg}, ${d.cb})`;
        }
    }

    // 5. Vstupy, Tlačítka a Joystick
    if (d.jx !== undefined) setText('val-jx', d.jx);
    if (d.jy !== undefined) setText('val-jy', d.jy);
    if (d.jbtn !== undefined) {
        const jb = document.getElementById('val-jbtn');
        if (jb) {
            jb.innerText = d.jbtn ? 'STISK' : 'UP';
            jb.className = d.jbtn ? 'tag active' : 'tag';
        }
    }
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

    // 5 tlačítek na spodním panelu (rozsvícení modrého tlačítka při stisku)
    if (d.btnd && Array.isArray(d.btnd)) {
        for (let i = 0; i < 5; i++) {
            const btnEl = document.getElementById(`btn-d${i+1}`);
            if (btnEl) {
                if (d.btnd[i]) btnEl.classList.add('active');
                else btnEl.classList.remove('active');
            }
        }
    }

    // 6. Systém a Režim
    if (d.mode !== undefined) {
        currentAppMode = d.mode;
        setText('sys-mode', modeNames[d.mode] || `Mód ${d.mode}`);
        updateActiveModeButtons(d.mode); // Zvýrazníme tlačítko aktuálního režimu
    }
    if (d.uptime !== undefined) {
        // Přepočet milisekund na hodiny, minuty a sekundy
        const sec = Math.floor(d.uptime / 1000);
        const mins = Math.floor(sec / 60);
        const hrs = Math.floor(mins / 60);
        setText('sys-uptime', `${hrs}h ${mins%60}m ${sec%60}s`);
    }
    if (d.heap !== undefined) setText('sys-heap', `${(d.heap / 1024).toFixed(1)} KB`);
    if (d.clients !== undefined) setText('sys-clients', d.clients);

    // Synchronizace polohy přepínačů LED (pokud na ně uživatel zrovna nesahá)
    if (d.led1 !== undefined) setCheck('ctrl-led1', d.led1);
    if (d.led2 !== undefined) setCheck('ctrl-led2', d.led2);
    if (d.led3 !== undefined) setCheck('ctrl-led3', d.led3);
}

// Pomocná funkce pro bezpečný přepis textu v elementu
function setText(id, val) {
    const el = document.getElementById(id);
    if (el) el.innerText = val;
}

// Pomocná funkce pro nastavení stavu checkboxu
function setCheck(id, val) {
    const el = document.getElementById(id);
    if (el && document.activeElement !== el) el.checked = val;
}

// Zvýrazní tlačítko právě aktivního módu v sekci Ovládání
function updateActiveModeButtons(mode) {
    document.querySelectorAll('.mode-btn').forEach(btn => {
        const oc = btn.getAttribute('onclick') || '';
        if (oc.includes(`(${mode})`)) {
            btn.classList.add('current');
        } else {
            btn.classList.remove('current');
        }
    });
}

// --- 5. Ovládací funkce volané z HTML prvků ---

// Přepnutí herního / systémového módu na displeji ESP
function setAppMode(mode) {
    sendCmd('setMode', { mode: parseInt(mode) });
}

// Zapnutí / vypnutí stavové LED
function toggleLed(id, state) {
    sendCmd('setLed', { id: parseInt(id), state: state });
}

// --- THROTTLE (Omezovač rychlosti): Chrání Wi-Fi před zahlcením při tahání slideru ---
function throttle(func, limit) {
    let inThrottle;
    return function() {
        const args = arguments;
        const context = this;
        if (!inThrottle) {
            func.apply(context, args);
            inThrottle = true;
            setTimeout(() => inThrottle = false, limit);
        }
    };
}

// Pohyb klasickým servem (max 20 zpráv za sekundu)
const onServoChange = throttle((val) => {
    document.getElementById('lbl-servo').innerText = `${val}°`;
    sendCmd('setServo', { val: parseInt(val) });
}, 50);

// Pohyb chytrým servem
const onSmartServoChange = throttle((val) => {
    document.getElementById('lbl-smartservo').innerText = `${val}°`;
    sendCmd('setSmartServo', { val: parseInt(val) });
}, 50);

// Změna rychlosti motoru
const onMotorChange = throttle((val) => {
    document.getElementById('lbl-motor').innerText = val;
    sendCmd('setMotor', { val: parseInt(val) });
}, 50);

// Tlačítko pro okamžité zastavení motoru na nulu
function stopMotor() {
    document.getElementById('ctrl-motor').value = 0;
    document.getElementById('lbl-motor').innerText = 0;
    sendCmd('setMotor', { val: 0 });
}

// Odeslání barvy a jasu na RGB pásek WS2812B
function updateRgbStrip() {
    const hex = document.getElementById('ctrl-rgb-color').value;
    const r = parseInt(hex.substr(1,2), 16); // Převod HEX na červenou (0-255)
    const g = parseInt(hex.substr(3,2), 16); // Převod HEX na zelenou (0-255)
    const b = parseInt(hex.substr(5,2), 16); // Převod HEX na modrou (0-255)
    const bright = parseInt(document.getElementById('ctrl-rgb-bright').value);
    sendCmd('setLedStrip', { r, g, b, bright });
}

// Plynulá změna jasu RGB pásku
const updateRgbBright = throttle((bright) => {
    document.getElementById('lbl-rgb-bright').innerText = bright;
    updateRgbStrip();
}, 50);

// Rychlé tlačítko barevné předvolby (červená, zelená, modrá atd.)
function setRgbPreset(hex) {
    document.getElementById('ctrl-rgb-color').value = hex;
    updateRgbStrip();
}

// Zazvonění bzučákem
function beep(freq, durationMs) {
    sendCmd('beep', { freq: parseInt(freq), duration: parseInt(durationMs) });
}

// Spuštění 5sekundové kalibrace Wi-Fi CSI radaru
function calibrateCsi() {
    sendCmd('calibrateCsi');
}

// Virtuální stisk tlačítka D-Padu (5 tlačítek do kříže)
function sendBtn(index, isPressed) {
    sendCmd('pressBtn', { idx: parseInt(index), pressed: isPressed });
}

)rawliteral";

#endif // WEB_PAGES_H
