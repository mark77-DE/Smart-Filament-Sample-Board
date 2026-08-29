// ============================================================================
// Spot My Filament - script.js (WebIF Dashboard)
// Fix: schnelles Durchklicken -> ACK + Retry + "nur letzter Klick zählt"
// ============================================================================

const activeTimers = {};     // UID -> TimeoutID (UI Highlight)
const lastScanTimes = {};    // UID -> timestamp (Debounce für HighlightUID)
const DEBOUNCE_MS = 2000;    // 2 Sekunden Entprellzeit für highlightUID (NFC/WS Events)

let CONFIGV2 = null;
const DEFAULT_LED_TIMEOUT_MS = 5000; // Fallback, falls config_v2.json fehlt

let socket = null;
let reconnectTimer = null;
const RECONNECT_DELAY = 2000; // 2 Sekunden

let wsLastHeartbeat = 0;
let wsWatchdogTimer = null;

let reconnectDelay = 1000;
const RECONNECT_MAX = 10000;

const WS_TIMEOUT_MS = 5000;
const WS_CHECK_INTERVAL = 1000;

let wsStatus = false;
const wsStatusElement = document.getElementById("wsStatus");

const selectLanguageSelect = document.getElementById('langSelect');

let countFilaments = 1;

//Filter vorbereiten
let FILAMENTS = [];
let activeFilters = {
  vendor: "",
  color: "",
  type: ""
};

let activeSort = {
  key: "",
  dir: "asc" // asc | desc
};


// ---------------- WebSocket ----------------
function updateWSStatus(connected) {
  if (!wsStatusElement) return;
  wsStatusElement.textContent = connected ? t("websocket_connected") : t("websocket_disconnected");
  wsStatusElement.classList.toggle("ws-connected", connected);
  wsStatusElement.classList.toggle("ws-disconnected", !connected);
  wsStatus = connected;
}

function getWebLedTimeoutMs() {
  const v =
    (CONFIGV2 && CONFIGV2.system && typeof CONFIGV2.system.webLEDTimeout === "number")
      ? CONFIGV2.system.webLEDTimeout
      : (CONFIGV2 && CONFIGV2.led && typeof CONFIGV2.led.timeout === "number")
        ? CONFIGV2.led.timeout
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

    reconnectDelay = 1000;
    updateWSStatus(true);

    wsLastHeartbeat = Date.now();   // wichtig!
    startWSWatchdog();

    if (pending) scheduleAckRetry(0);
  };

  socket.onclose = () => {

    updateWSStatus(false);

    clearAckTimers();

    if (sendCoalesceTimer) {
      clearTimeout(sendCoalesceTimer);
      sendCoalesceTimer = null;
    }

    if (wsWatchdogTimer) {
      clearInterval(wsWatchdogTimer);
      wsWatchdogTimer = null;
    }

    scheduleReconnect();
  };

  socket.onerror = () => {
    try { socket.close(); } catch {}
  };

  socket.onmessage = (event) => {
    try {

      const msg = JSON.parse(event.data);

      // ACK
      if (msg.action === "ack" && pending && msg.seq === pending.seq) {
        pending = null;
        clearAckTimers();
        return;
      }

      // -------- Heartbeat --------
      if (msg.action === "heartbeat") {

        wsLastHeartbeat = Date.now();

        if (!wsWatchdogTimer) {
            startWSWatchdog();   // erst jetzt!
        }

        updateWSStatus(true);
        updateRssiIcon(msg.wifi_rssi);

        if(msg.updateAvailable) {
          showUpdateNotification(msg);
        }

        if (CONFIGV2.system.debugMode) {
          console.log("Heartbeat:", msg);
        }

      return;
    }

      // UID
      if (msg && msg.uid) {
        highlightUID(msg.uid);
      }

    } catch (e) {
      console.error("WS ERROR:", e, event.data)
    }
  };
}

function startWSWatchdog() {

  if (wsWatchdogTimer) {
    clearInterval(wsWatchdogTimer);
  }

  wsWatchdogTimer = setInterval(() => {

    const delta = Date.now() - wsLastHeartbeat;

    if (delta > WS_TIMEOUT_MS) {
      console.warn("Heartbeat timeout");
      try { socket.close(); } catch {}
    }

  }, WS_CHECK_INTERVAL);
}

function scheduleReconnect() {

  if (reconnectTimer) return;

  reconnectTimer = setTimeout(() => {
    reconnectTimer = null;
    connectWS();

    reconnectDelay = Math.min(reconnectDelay * 2, RECONNECT_MAX);

  }, reconnectDelay);
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
      action: "highlightUIDLED",
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

  
  if(CONFIGV2.system.debugMode) {
    console.log("Highlight UID:", uid);
  }

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


// Highlight mehrere UIDs gleichzeitig (z. B. Filter)
function sendMultiHighlight(uids) {
  if (!socket || socket.readyState !== WebSocket.OPEN) return;

  try {
    socket.send(JSON.stringify({
      action: "highlightMultiLED",
      uids: uids
    }));
  } catch (e) {
    console.error("WebSocket send failed:", e);
  }
}


// ---------------- Raster-Kacheln laden ----------------
async function loadFilamentTiles() {
  const [configRes, filamentsRes] = await Promise.all([
    fetch("/config_v2.json"),
    fetch("/filaments.json")
  ]);

  CONFIGV2 = await configRes.json();
  FILAMENTS = await filamentsRes.json();

  document.body.classList.toggle("daymode", !CONFIGV2.system.darkmode);

  

  // Filter füllen
  populateFilter("filterVendor", "vendor");
  populateFilter("filterColor", "color");
  populateFilter("filterType", "type");
  populateFilter("filterStorage", "storage");

  // Initial alle Tiles rendern
  updateGrid();
}


function getVersion() {
  fetch("/api/version")
    .then(r => r.json())
    .then(data => {
      document.getElementById("fwVersion").textContent = "FW-Version: " + data.firmware;
      //document.getElementById("gitHash").textContent = "Git hash: " + data.git_hash;
      document.getElementById("build_date").textContent = "Build date: " + data.build_date_short;
    })
    .catch(err => console.error("Version fetch failed:", err));
}


function renderFilamentGrid(filaments) {
  const grid = document.getElementById("filamentGrid");
  grid.innerHTML = "";

  filaments.forEach(f => {
    const tile = document.createElement("div");
    tile.className = "tile";
    tile.dataset.uid = f.uid;

    tile.innerHTML = `
      <div class="vendor">${f.vendor}</div>
      <div class="color">${f.color}</div>
      <div class="type">${f.type}</div>
      <div class="storage-badge">${f.storage}</div>
    `;

    tile.onclick  = () => {
      sendHighlight(f.uid);
    }

    tile.ondblclick = () => {
      showDetails(f);
    };

    grid.appendChild(tile);
  });
}

function applyFilters() {

  const free = (activeFilters.free || "").toLowerCase();

  return FILAMENTS.filter(f => {

    if (activeFilters.vendor  && f.vendor  !== activeFilters.vendor)  return false;
    if (activeFilters.color   && f.color   !== activeFilters.color)   return false;
    if (activeFilters.type    && f.type    !== activeFilters.type)    return false;
    if (activeFilters.storage && f.storage !== activeFilters.storage) return false;

    // ----- NEW: Free text filter -----
    if (free) {
      const haystack = [
        f.vendor,
        f.color,
        f.type,
        f.storage,
        f.info1,
        f.info2
      ]
      .join(" ")
      .toLowerCase();

      if (!haystack.includes(free)) return false;
    }

    return true;
  });
}

function updateGrid() {
  const filtered = applyFilters();
  const sorted = sortFilaments(filtered);
  renderFilamentGrid(sorted);
}

function populateFilter(selectId, key) {
  const select = document.getElementById(selectId);
  if (!select) return;

  // Vorherige Optionen entfernen
  select.innerHTML = "";

  // Mapping für schöne Labels
  const FILTER_LABELS_DE = {
    vendor:   "Hersteller",
    color:    "Farben",
    type:     "Typen",
    storage:  "Lagerplatz"
  };

  const FILTER_LABELS_EN = {
    vendor:   "manufactors",
    color:    "colors",
    type:     "types",
    storage:  "storage"
  };

  // Leere Option für „Alle“
  const emptyOpt = document.createElement("option");
  emptyOpt.value = "";
  if(CONFIGV2.system.defaultLanguage === "de") {
    emptyOpt.textContent = "Alle " + (FILTER_LABELS_DE[key] || key);
  } else {  
    emptyOpt.textContent = "all " + (FILTER_LABELS_EN[key] || key);
  }
  select.appendChild(emptyOpt);

  // Alle eindeutigen Werte für dieses Feld
  const values = [...new Set(FILAMENTS.map(f => f[key]))].sort();

  values.forEach(v => {
    const opt = document.createElement("option");
    opt.value = v;
    opt.textContent = v;
    select.appendChild(opt);
  });
}

function highlightFilteredLEDs() {

  const free = (activeFilters.free || "").toLowerCase();

  const filtered = FILAMENTS.filter(f => {

    if (activeFilters.vendor  && f.vendor  !== activeFilters.vendor)  return false;
    if (activeFilters.color   && f.color   !== activeFilters.color)   return false;
    if (activeFilters.type    && f.type    !== activeFilters.type)    return false;
    if (activeFilters.storage && f.storage !== activeFilters.storage) return false;

    // ----- NEW: Free text filter -----
    if (free) {
      const haystack = [
        f.vendor  || "",
        f.color   || "",
        f.type    || "",
        f.storage || "",
        f.info1   || "",
        f.info2   || ""
      ]
      .join(" ")
      .toLowerCase();

      if (!haystack.includes(free)) return false;
    }

    return true;
  });


  // ----- Check if ANY filter is active -----
  const noFiltersActive =
    !activeFilters.vendor &&
    !activeFilters.color &&
    !activeFilters.type &&
    !activeFilters.storage &&
    !free;


  if (noFiltersActive) {

    if (CONFIGV2.system.debugMode) {
      console.log("No filters active - skipping LED highlight");
    }

    return; // sauber beenden
  }


  if (CONFIGV2.system.debugMode) {
    console.log("Highlighting filtered LEDs for filters:", activeFilters);
    console.log("Filtered UIDs:", filtered.map(f => f.uid));
  }

  const uids = filtered.map(f => f.uid);
  sendMultiHighlight(uids);
}






document.getElementById("filterVendor").onchange = e => {
  activeFilters.vendor = e.target.value;
  
  if(CONFIGV2.system.debugMode) {
    console.log("Filter Vendor changed to:", activeFilters.vendor);
  } 
  
  updateGrid();           // Tiles anzeigen/ausblenden
  highlightFilteredLEDs(); // LEDs leuchten
};

document.getElementById("filterColor").onchange = e => {
  activeFilters.color = e.target.value;
  
  if(CONFIGV2.system.debugMode) {
    console.log("Filter Color changed to:", activeFilters.color);
  }
  
  updateGrid();
  highlightFilteredLEDs();
};

document.getElementById("filterType").onchange = e => {
  activeFilters.type = e.target.value;
  
  if(CONFIGV2.system.debugMode) {
    console.log("Filter Type changed to:", activeFilters.type);
  }
  
  updateGrid();
  highlightFilteredLEDs();
};

document.getElementById("filterStorage").onchange = e => {
  activeFilters.storage = e.target.value;
  
  if(CONFIGV2.system.debugMode) {
    console.log("Filter Type changed to:", activeFilters.type);
  }
  
  updateGrid();
  highlightFilteredLEDs();
};

document.getElementById("filterFree").addEventListener("input", e => {
  activeFilters.free = e.target.value.trim();
  
  if(CONFIGV2.system.debugMode) {
    console.log("Filter Type changed to:", activeFilters.type);
  }
  
  updateGrid();
  highlightFilteredLEDs();
});




const toggleBtn = document.getElementById("themeToggle");

toggleBtn.addEventListener("click", () => {
    document.body.classList.toggle("daymode");
});


function showDetails(f) {
  const overlay = document.getElementById("detailOverlay");
  const content = document.getElementById("detailContent");

  content.innerHTML = `
    <h3>${f.vendor} – ${f.type}</h3>
    <p><strong><span data-i18n="txt_color">Color:</span></strong> ${f.color}</p>
    <p><strong><span data-i18n="txt_storage">Storage:</span></strong> ${f.storage}</p>
    <p><strong>Info 1:</strong> ${f.info1}</p>
    <p><strong>Info 2:</strong> ${f.info2}</p>
  `;

  applyTranslations(content);
  overlay.classList.remove("hidden");
}

function closeDetails() {
  const overlay = document.getElementById("detailOverlay");
  overlay.classList.add("hidden");
}

document.getElementById("detailOverlay").addEventListener("click", (e) => {
  if (e.target.id === "detailOverlay") {
    closeDetails();
  }
});




function updateRssiIcon(rssi) {
    const bars = document.querySelectorAll("#rssiIcon .bar");

    // Alle Balken zurücksetzen
    bars.forEach(bar => bar.setAttribute("fill", "gray"));

    let color;

    if (rssi > -50) {       // starkes Signal
        color = "green";
    } else if (rssi > -75) { // mittel
        color = "orange";
    } else if (rssi > -90) { // schwach
        color = "red";
    } else {                 // sehr schwach
        numBars = 0;
    }

    bars.forEach(bar => {
        bar.classList.remove("red","orange","green");
        bar.classList.add(color); // color = "red"|"orange"|"green"
    });

   
}


function showUpdateNotification(msg) {
  
  const updateDiv = document.getElementById('updateStatus');
  updateDiv.textContent = `⚠️ Update available: ${msg.latestVersion}`;
  updateDiv.style.color = 'orange';

}


function sortFilaments(list) {
  if (!activeSort.key) return list;

  return [...list].sort((a, b) => {
    const valA = (a[activeSort.key] || "").toString();
    const valB = (b[activeSort.key] || "").toString();

    const result = valA.localeCompare(valB, undefined, {
      numeric: true,
      sensitivity: "base"
    });

    return activeSort.dir === "asc" ? result : -result;
  });
}

function updateSortIndicators() {
  document.querySelectorAll(".sortable").forEach(label => {
    const indicator = label.querySelector(".sortIndicator");
    const key = label.dataset.key;

    if (key === activeSort.key) {
      indicator.textContent = activeSort.dir === "asc" ? " ↑" : " ↓";
      label.classList.add("activeSort");
    } else {
      indicator.textContent = "";
      label.classList.remove("activeSort");
    }
  });
}

document.querySelectorAll(".sortable").forEach(label => {
  label.addEventListener("click", () => {
    const key = label.dataset.key;

    if (activeSort.key === key) {
      // Richtung toggeln
      activeSort.dir = activeSort.dir === "asc" ? "desc" : "asc";
    } else {
      // neues Feld
      activeSort.key = key;
      activeSort.dir = "asc";
    }

    updateSortIndicators();
    updateGrid();
  });
});



async function init() {
    await loadFilamentTiles();
    // Aufruf nach Laden des Webinterfaces
    



    connectWS();

    getVersion();

    
    loadHelpAndLang(CONFIGV2.system.defaultLanguage); // Standard-Sprache
    selectLanguageSelect.value = CONFIGV2.system.defaultLanguage;
    setupLangSwitcher('langSelect');

    document.getElementById("detailCloseBtn").addEventListener("click", closeDetails);
    
    

}

init();



