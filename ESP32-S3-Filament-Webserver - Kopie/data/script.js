const socket = new WebSocket(`ws://${location.host}/ws`);
const activeTimers = {}; // UID -> TimeoutID

// WebSocket-Nachrichten verarbeiten
socket.onmessage = (event) => {
    const msg = JSON.parse(event.data);
    highlightUID(msg.uid);
};

// Tabelle aufbauen / aktualisieren
async function updateTable() {
    const res = await fetch('/filaments.json');
    const data = await res.json();
    const tbody = document.querySelector("#filamentTable tbody");
    tbody.innerHTML = '';

    data.forEach(f => {
        const tr = document.createElement('tr');
        tr.dataset.uid = f.uid;
        tr.innerHTML = `<td>${f.uid}</td><td>${f.vendor}</td><td>${f.type}</td><td>${f.color}</td><td>${f.ledIndex}</td>`;
        tbody.appendChild(tr);
    });

    // Klick-Handler erneut setzen
    makeRowsClickable();
}

// Highlight-Funktion für bekannte / unbekannte UIDs
const lastScanTimes = {}; // UID -> timestamp
const DEBOUNCE_MS = 2000; // 2 Sekunden Entprellzeit

function highlightUID(uid) {
    const now = Date.now();

    // Debounce prüfen
    if(lastScanTimes[uid] && now - lastScanTimes[uid] < DEBOUNCE_MS) {
        return; // zu früh, ignorieren
    }
    lastScanTimes[uid] = now;

    const tbody = document.querySelector("#filamentTable tbody");

    // Alle vorherigen Highlights sofort entfernen
    tbody.querySelectorAll("tr").forEach(tr => tr.classList.remove("active"));

    // Alle bisherigen Timer abbrechen
    for (const key in activeTimers) {
        clearTimeout(activeTimers[key]);
        delete activeTimers[key];
    }

    // Alle Popups schließen
    const popup = document.getElementById('unknown');
    popup.style.display = 'none';

    const tr = tbody.querySelector(`tr[data-uid="${uid}"]`);
    if(tr){
        // bekannte UID -> gelb markieren
        tr.classList.add("active");
        activeTimers[uid] = setTimeout(() => tr.classList.remove("active"), 10000);
    } else {
        // unbekannte UID -> Popup
        popup.textContent = "Unbekannter Tag: " + uid;
        popup.style.display = 'block';
        setTimeout(() => popup.style.display = 'none', 5000);
    }
}





function connectWS() {
    const socket = new WebSocket(`ws://${location.host}/ws`);

    socket.onopen = () => console.log("WS verbunden!");
    socket.onclose = () => {
        console.log("WS getrennt, versuche erneut in 2s...");
        setTimeout(connectWS, 2000);
    };
    socket.onmessage = (event) => {
        const msg = JSON.parse(event.data);
        highlightUID(msg.uid);
    };
}


// Zeilen klickbar machen, um LED zu aktivieren
function makeRowsClickable() {
    const tbody = document.querySelector("#filamentTable tbody");
    tbody.querySelectorAll("tr").forEach(tr => {
        tr.onclick = () => {
            const uid = tr.dataset.uid;
            if (!uid) return;
            // Nachricht an ESP senden
            if(socket && socket.readyState === WebSocket.OPEN){
                socket.send(JSON.stringify({action: "highlightLED", uid: uid}));
            }
            // **lokal die Zeile direkt markieren, ohne Debounce**
         const trElem = tbody.querySelector(`tr[data-uid="${uid}"]`);
         if(trElem){
                trElem.classList.add("active");
                setTimeout(() => trElem.classList.remove("active"), 3000);
            }
        };
    });
}




connectWS();
// Tabelle initial laden
updateTable();
