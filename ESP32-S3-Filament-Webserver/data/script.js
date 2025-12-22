
const activeTimers = {}; // UID -> TimeoutID
const lastScanTimes = {}; // UID -> timestamp
const DEBOUNCE_MS = 2000; // 2 Sekunden Entprellzeit

let CONFIG =null;
const DEFAULT_LED_TIMEOUT_MS = 5000; // Fallback, falls config.json fehlt
let socket = null;
let reconnectTimer = null;
const RECONNECT_DELAY = 2000; // 2 Sekunden

let wsStatus = false;

const wsStatusElement = document.getElementById("wsStatus");



// --- WebSocket-Verbindung ---
function connectWS() {
    if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) {
        // Bereits verbunden oder verbindend
        return;
    }

    socket = new WebSocket(`ws://${location.host}/ws`);

    socket.onopen = () => {

        //console.log("WS verbunden!");
        if (reconnectTimer) {
            clearTimeout(reconnectTimer);
            reconnectTimer = null;
        }
        updateWSStatus(true);
    };

    socket.onclose = () => {
        //console.log("WS getrennt, versuche erneut in 2s...");
        updateWSStatus(false);
        reconnectTimer = setTimeout(connectWS, RECONNECT_DELAY);
    };

    socket.onerror = (err) => {
        //console.error("WebSocket-Fehler", err);
        socket.close(); // löst onclose aus
    };

    socket.onmessage = (event) => {
        try {
            const msg = JSON.parse(event.data);
            if (msg && msg.uid) highlightUID(msg.uid);
    } catch (e) {
        // ignore
    }
};

}

// sofort verbinden
connectWS();


function updateWSStatus(connected) {
    if (!wsStatusElement) return;
    wsStatusElement.textContent = connected ? "verbunden" : "getrennt";
    wsStatusElement.classList.toggle("ws-connected", connected);
    wsStatusElement.classList.toggle("ws-disconnected", !connected);
    wsStatus = connected;
}

function getWebLedTimeoutMs() {
    const v =
        (CONFIG && CONFIG.options && typeof CONFIG.options.webLEDTimeout === "number")
            ? CONFIG.options.webLEDTimeout
            : (CONFIG && CONFIG.options && typeof CONFIG.options.ledTimeout === "number")
                ? CONFIG.options.ledTimeout
                : DEFAULT_LED_TIMEOUT_MS;

    // etwas absichern (min/max wie du willst)
    return Math.max(100, Math.min(600000, v)); // 100ms .. 10min
}

// --- Highlight-Funktion ---
function highlightUID(uid) {
    const now = Date.now();

    if (lastScanTimes[uid] && now - lastScanTimes[uid] < DEBOUNCE_MS) return;
    lastScanTimes[uid] = now;

    const grid = document.getElementById("filamentGrid");
    const tiles = grid.querySelectorAll(".tile");

    // Alle vorherigen Highlights entfernen
    tiles.forEach(t => t.classList.remove("active"));

    // Alle bisherigen Timer abbrechen
    for (const key in activeTimers) {
        clearTimeout(activeTimers[key]);
        delete activeTimers[key];
    }

    // Passende Kachel suchen
    const tile = Array.from(tiles).find(t => t.dataset.uid === uid);
    if (tile) {
        tile.classList.add("active");

        const timeoutMs = getWebLedTimeoutMs();
        activeTimers[uid] = setTimeout(() => tile.classList.remove("active"), timeoutMs);

    } else {
        const popup = document.getElementById('unknown');
        if (popup) {
            popup.textContent = "Unbekannter Tag: " + uid;
            popup.style.display = 'block';
            setTimeout(() => popup.style.display = 'none', 5000);
        }
    }
}

// --- Raster-Kacheln laden (nur belegte LEDs) ---
async function loadFilamentTiles() {
    const [configRes, filamentsRes] = await Promise.all([
        fetch('/config.json'),
        fetch('/filaments.json')
    ]);

    CONFIG = await configRes.json();
    const filaments = await filamentsRes.json();

    const grid = document.getElementById("filamentGrid");
    grid.innerHTML = "";

    filaments.forEach(f => {
        const tile = document.createElement("div");
        tile.className = "tile";
        tile.dataset.uid = f.uid;
        tile.dataset.led = f.ledIndex;

        const vendorSpan = document.createElement("div");
        vendorSpan.className = "vendor";
        vendorSpan.textContent = f.vendor;

        const colorSpan = document.createElement("div");
        colorSpan.className = "color";
        colorSpan.textContent = f.color; // Farbe als Hintergrund

        const typeSpan = document.createElement("div");
        typeSpan.className = "type";
        typeSpan.textContent = f.type;

        tile.appendChild(vendorSpan);
        tile.appendChild(colorSpan);
        tile.appendChild(typeSpan);

        // Klick-Handler: LED über WebSocket aktivieren
        tile.onclick = () => {
            if (socket.readyState === WebSocket.OPEN) {
                socket.send(JSON.stringify({ action: "highlightLED", uid: f.uid }));

                tile.classList.add("active");

                const timeoutMs = getWebLedTimeoutMs();
                setTimeout(() => tile.classList.remove("active"), timeoutMs);
            }
};


        grid.appendChild(tile);
    });
}

function getVersion() {
    fetch("/api/version")
    .then(r => r.json())
    .then(data => {
        document.getElementById("fwVersion").textContent = "FW-Version: " + data.firmware;
        document.getElementById("gitHash").textContent = "Git hash: " + data.git_hash;
        document.getElementById("build_date").textContent = "Build date: " + data.build_date;
    })
    .catch(err => console.error("Version fetch failed:", err));
}


// --- Init ---
loadFilamentTiles();
getVersion();