
const activeTimers = {}; // UID -> TimeoutID
const lastScanTimes = {}; // UID -> timestamp
const DEBOUNCE_MS = 2000; // 2 Sekunden Entprellzeit

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
        console.log("WS verbunden!");
        if (reconnectTimer) {
            clearTimeout(reconnectTimer);
            reconnectTimer = null;
        }
        updateWSStatus(true);
    };

    socket.onclose = () => {
        console.log("WS getrennt, versuche erneut in 2s...");
        updateWSStatus(false);
        reconnectTimer = setTimeout(connectWS, RECONNECT_DELAY);
    };

    socket.onerror = (err) => {
        console.error("WebSocket-Fehler", err);
        socket.close(); // löst onclose aus
    };

    socket.onmessage = (event) => {
        const msg = JSON.parse(event.data);
        highlightUID(msg.uid);
        
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

// --- Highlight-Funktion ---
function highlightUID(uid) {
    
    const now = Date.now();

    if(lastScanTimes[uid] && now - lastScanTimes[uid] < DEBOUNCE_MS) return;
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
    if(tile) {
        tile.classList.add("active");
        activeTimers[uid] = setTimeout(() => tile.classList.remove("active"), 10000);
    } else {
        const popup = document.getElementById('unknown');
        if(popup){
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

    const CONFIG = await configRes.json();
    const filaments = await filamentsRes.json();

    const grid = document.getElementById("filamentGrid");
    grid.innerHTML = "";
    grid.style.display = 'grid';
    grid.style.gridTemplateColumns = `repeat(auto-fill, minmax(120px, 1fr))`; // dynamisch
    grid.style.gridGap = '10px';

    filaments.forEach(f => {
        const tile = document.createElement("div");
        tile.className = "tile";
        tile.dataset.uid = f.uid;
        tile.dataset.led = f.ledIndex;

        const vendorSpan = document.createElement("span");
        vendorSpan.className = "vendor";
        vendorSpan.textContent = f.vendor;

        const colorSpan = document.createElement("span");
        colorSpan.className = "color";
        colorSpan.textContent = f.color; // Farbe als Hintergrund

        const typeSpan = document.createElement("span");
        typeSpan.className = "type";
        typeSpan.textContent = f.type;

        tile.appendChild(vendorSpan);
        tile.appendChild(colorSpan);
        tile.appendChild(typeSpan);

        // Klick-Handler: LED über WebSocket aktivieren
        tile.onclick = () => {
            if (socket.readyState === WebSocket.OPEN) {
                socket.send(JSON.stringify({action: "highlightLED", uid: f.uid}));
                tile.classList.add("active");
                setTimeout(() => tile.classList.remove("active"), CONFIG.options.ledTimeout);
            }
            
        };

        grid.appendChild(tile);
    });
}




// --- Init ---
loadFilamentTiles();
