// ============================================================================
// Spot My Filament - script.js (WebIF Dashboard)
// Fix: schnelles Durchklicken -> ACK + Retry + "nur letzter Klick zählt"
// ============================================================================

const activeTimers = {};     // UID -> TimeoutID (UI Highlight)
const lastScanTimes = {};    // UID -> timestamp (Debounce für HighlightUID)
const DEBOUNCE_MS = 2000;    // 2 Sekunden Entprellzeit für highlightUID (NFC/WS Events)

let CONFIG = null;
const DEFAULT_LED_TIMEOUT_MS = 5000; // Fallback, falls config.json fehlt

let socket = null;
let reconnectTimer = null;
const RECONNECT_DELAY = 2000; // 2 Sekunden

let wsStatus = false;
const wsStatusElement = document.getElementById("wsStatus");

// ---------------- WebSocket ----------------
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

  return Math.max(100, Math.min(600000, v));
}

function connectWS() {
  if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) {
    return;
  }

  socket = new WebSocket(`ws://${location.host}/ws`);

  socket.onopen = () => {
    if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
    updateWSStatus(true);

    // falls gerade ein pending da ist (z.B. Reconnect), nochmal versuchen
    if (pending) scheduleAckRetry(0);
  };

  socket.onclose = () => {
    updateWSStatus(false);
    clearAckTimers(); // wichtig: ACK-Timer stoppen
    if (sendCoalesceTimer) { clearTimeout(sendCoalesceTimer); sendCoalesceTimer = null; }

    reconnectTimer = setTimeout(connectWS, RECONNECT_DELAY);
  };

  socket.onerror = () => {
    try { socket.close(); } catch {}
  };

  // >>> HIER ist der Block, den du meinst:
  socket.onmessage = (event) => {
    try {
      const msg = JSON.parse(event.data);

      // ACK für Klick
      if (msg.action === "ack" && pending && msg.seq === pending.seq) {
        pending = null;
        clearAckTimers();
        return;
      }

      // normale UID-Events (NFC + WebIF Broadcast)
      if (msg && msg.uid) {
        highlightUID(msg.uid); // NICHT bypassDebounce, weil das meist NFC/WS ist
      }

    } catch (e) {}
  };
}




// ---------------- ACK/Retry für "highlightLED" ----------------
let clickSeq = 0;
let pending = null;          // { seq, uid, tries }
let ackTimer = null;

const ACK_TIMEOUT_MS = 250;  // erst nach 250ms überhaupt an Retry denken
const ACK_RETRY_MS   = 180;  // Retry Abstand
const ACK_MAX_TRIES  = 4;

// "Durchklicken" entlasten: Klicks kurz sammeln, nur letzter wird gesendet
const CLICK_COALESCE_MS = 60;
let queuedUid = null;
let sendCoalesceTimer = null;

function clearAckTimers() {
  if (ackTimer) { clearTimeout(ackTimer); ackTimer = null; }
}

function scheduleAckTimeout() {
  clearAckTimers();
  ackTimer = setTimeout(() => {
    // ACK nicht gekommen -> jetzt erst mit Retries anfangen
    scheduleAckRetry(0);
  }, ACK_TIMEOUT_MS);
}

function scheduleAckRetry(delayMs = ACK_RETRY_MS) {
  clearAckTimers();
  ackTimer = setTimeout(sendPendingOnce, delayMs);
}

function sendPendingOnce() {
  if (!pending) return;
  if (!socket || socket.readyState !== WebSocket.OPEN) {
    scheduleAckRetry(ACK_RETRY_MS);
    return;
    }


  pending.tries++;

  try {
    socket.send(JSON.stringify({
      action: "highlightLED",
      uid: pending.uid,
      seq: pending.seq
    }));
  } catch (e) {}

  if (pending.tries >= ACK_MAX_TRIES) {
    pending = null;
    clearAckTimers();
    return;
  }

  scheduleAckRetry(ACK_RETRY_MS);
}

function flushCoalescedSend() {
  sendCoalesceTimer = null;
  if (!queuedUid) return;

  // UI sofort reagieren lassen (Timer neu starten!)
  highlightUID(queuedUid, { bypassDebounce: true });


  // wenn WS nicht offen: nur UI
  if (!socket || socket.readyState !== WebSocket.OPEN) {
    queuedUid = null;
    return;
  }

  const seq = ++clickSeq;
  pending = { seq, uid: queuedUid, tries: 0 };
  queuedUid = null;

  // einmal senden
  sendPendingOnce();

  // Retry erst wenn ACK nach ACK_TIMEOUT_MS nicht kam
  scheduleAckTimeout();
}

function sendHighlight(uid) {
  queuedUid = uid;

  // wenn gerade kein Coalesce läuft: sofort senden
  if (!sendCoalesceTimer) {
    flushCoalescedSend();
    // aber ein kurzes Fenster öffnen, um Folge-Klicks zu bündeln
    sendCoalesceTimer = setTimeout(() => {
      sendCoalesceTimer = null;
      // falls in der Zeit noch was reinkam -> senden
      if (queuedUid) flushCoalescedSend();
    }, CLICK_COALESCE_MS);
    return;
  }

  // Folge-Klick innerhalb des Fensters: nur UID updaten
}




// ---------------- Highlight-Funktion (UI) ----------------
function highlightUID(uid, opts = {}) {
  const { bypassDebounce = false } = opts;
  const now = Date.now();

  // Debounce nur für NFC/WS-Events, NICHT für Klicks
  if (!bypassDebounce) {
    if (lastScanTimes[uid] && now - lastScanTimes[uid] < DEBOUNCE_MS) return;
    lastScanTimes[uid] = now;
  }

  const grid = document.getElementById("filamentGrid");
  if (!grid) return;

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
    const popup = document.getElementById("unknown");
    if (popup) {
      popup.textContent = "Unbekannter Tag: " + uid;
      popup.style.display = "block";
      setTimeout(() => popup.style.display = "none", 5000);
    }
  }
}


// ---------------- Raster-Kacheln laden ----------------
async function loadFilamentTiles() {
  const [configRes, filamentsRes] = await Promise.all([
    fetch("/config.json"),
    fetch("/filaments.json")
  ]);

  CONFIG = await configRes.json();
  const filaments = await filamentsRes.json();

  const grid = document.getElementById("filamentGrid");
  if (!grid) return;

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
    colorSpan.textContent = f.color;

    const typeSpan = document.createElement("div");
    typeSpan.className = "type";
    typeSpan.textContent = f.type;

    tile.appendChild(vendorSpan);
    tile.appendChild(colorSpan);
    tile.appendChild(typeSpan);

    // Klick: sendHighlight (ACK/Retry) + UI sofort
    tile.onclick = () => sendHighlight(f.uid);

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


connectWS();
loadFilamentTiles();
getVersion();
