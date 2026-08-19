let ws = null;
let reconnectTimer = null;
let currentAppMode = 0;

const modeNames = [
    "Hlavní menu", "Senzory", "Hra Snake", "Hra Flappy", "Hra 2048",
    "Měření vzdálenosti", "Wi-Fi spojení", "Ovládání serv", "Ovládání motoru",
    "Barevný senzor", "Spánkový režim"
];

// --- Spuštění při načtení stránky ---
window.addEventListener('load', () => {
    initWebSocket();
});

// --- Přepínání záložek ---
function switchTab(tabId) {
    document.querySelectorAll('.tab-btn').forEach(btn => btn.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(content => content.classList.remove('active'));

    const activeBtn = Array.from(document.querySelectorAll('.tab-btn')).find(b => b.getAttribute('onclick').includes(tabId));
    if (activeBtn) activeBtn.classList.add('active');

    const activeContent = document.getElementById(`tab-${tabId}`);
    if (activeContent) activeContent.classList.add('active');
}

// --- WebSocket komunikace ---
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
            console.error('WS Chyba:', err);
            ws.close();
        };

        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                handleTelemetry(data);
            } catch (e) {
                console.error('Chyba parsování zprávy:', e, event.data);
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

function sendCmd(cmd, payload = {}) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        const msg = JSON.stringify({ cmd, ...payload });
        ws.send(msg);
    }
}

// --- Zpracování telemetrie ---
function handleTelemetry(d) {
    // 1. Prostředí (DHT)
    if (d.t !== undefined) setText('val-temp', d.t > -100 ? d.t.toFixed(1) : '--');
    if (d.h !== undefined) setText('val-hum', d.h >= 0 ? d.h.toFixed(1) : '--');

    // 2. Vzdálenosti
    if (d.laser !== undefined) setText('val-laser', d.laser);
    if (d.ultra !== undefined) setText('val-ultra', d.ultra >= 0 ? d.ultra.toFixed(1) : '--');
    if (d.ir !== undefined) setText('val-ir', d.ir >= 0 ? d.ir.toFixed(1) : '--');
    
    if (d.irobs !== undefined) {
        const obsEl = document.getElementById('val-ir-obs');
        if (obsEl) {
            obsEl.innerText = d.irobs ? 'PŘEKÁŽKA!' : 'Čisté';
            obsEl.className = d.irobs ? 'tag active' : 'tag';
        }
    }

    // 3. IMU (LSM6DS3)
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
        if (colPrev) {
            colPrev.style.backgroundColor = `rgb(${d.cr}, ${d.cg}, ${d.cb})`;
        }
    }

    // 5. Vstupy a Tlačítka
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

    if (d.btnd && Array.isArray(d.btnd)) {
        for (let i = 0; i < 5; i++) {
            const btnEl = document.getElementById(`btn-d${i+1}`);
            if (btnEl) {
                if (d.btnd[i]) btnEl.classList.add('active');
                else btnEl.classList.remove('active');
            }
        }
    }

    // 6. Systém & Mód
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

    // Synchronizace stavu LED přepínačů, pokud uživatel zrovna nemění
    if (d.led1 !== undefined) setCheck('ctrl-led1', d.led1);
    if (d.led2 !== undefined) setCheck('ctrl-led2', d.led2);
    if (d.led3 !== undefined) setCheck('ctrl-led3', d.led3);
}

function setText(id, val) {
    const el = document.getElementById(id);
    if (el) el.innerText = val;
}

function setCheck(id, val) {
    const el = document.getElementById(id);
    if (el && document.activeElement !== el) el.checked = val;
}

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

// --- Ovládací funkce ---

function setAppMode(mode) {
    sendCmd('setMode', { mode: parseInt(mode) });
}

function toggleLed(id, state) {
    sendCmd('setLed', { id: parseInt(id), state: state });
}

// Pomocný omezovač frekvence volání (Throttle)
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

const onServoChange = throttle((val) => {
    document.getElementById('lbl-servo').innerText = `${val}°`;
    sendCmd('setServo', { val: parseInt(val) });
}, 50);

const onSmartServoChange = throttle((val) => {
    document.getElementById('lbl-smartservo').innerText = `${val}°`;
    sendCmd('setSmartServo', { val: parseInt(val) });
}, 50);

const onMotorChange = throttle((val) => {
    document.getElementById('lbl-motor').innerText = val;
    sendCmd('setMotor', { val: parseInt(val) });
}, 50);

function stopMotor() {
    document.getElementById('ctrl-motor').value = 0;
    document.getElementById('lbl-motor').innerText = 0;
    sendCmd('setMotor', { val: 0 });
}

function updateRgbStrip() {
    const hex = document.getElementById('ctrl-rgb-color').value;
    const r = parseInt(hex.substr(1,2), 16);
    const g = parseInt(hex.substr(3,2), 16);
    const b = parseInt(hex.substr(5,2), 16);
    const bright = parseInt(document.getElementById('ctrl-rgb-bright').value);
    sendCmd('setLedStrip', { r, g, b, bright });
}

const updateRgbBright = throttle((bright) => {
    document.getElementById('lbl-rgb-bright').innerText = bright;
    updateRgbStrip();
}, 50);

function setRgbPreset(hex) {
    document.getElementById('ctrl-rgb-color').value = hex;
    updateRgbStrip();
}

function beep(freq, durationMs) {
    sendCmd('beep', { freq: parseInt(freq), duration: parseInt(durationMs) });
}
