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
