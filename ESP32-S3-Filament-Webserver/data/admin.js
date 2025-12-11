const form = document.getElementById("uploadForm");
const dbDiv = document.getElementById("db");
const addForm = document.getElementById("addForm");
let lastHighlightedRow = null;
let CONFIG = null;




/* ------------------------ WebSocket ------------------------ */
const ws = new WebSocket(`ws://${location.host}/ws`);

ws.onmessage = async (ev) => {
    let data;
    try {
        data = JSON.parse(ev.data);
    } catch {
        console.warn("WebSocket parse error:", ev.data);
        return;
    }

    if (!data.uid) return;

    const scannedUID = data.uid;

    await loadTable();

    const rows = document.querySelectorAll("#db table tr");
    let found = false;

    rows.forEach((row, i) => {
        if (i === 0) return; // Header
        const uidCell = row.querySelector('td[data-field="uid"]');
        if (!uidCell) return;

        if (uidCell.innerText.trim() === scannedUID) {
            found = true;

            if (lastHighlightedRow) lastHighlightedRow.classList.remove("highlight");

            row.classList.add("highlight");
            lastHighlightedRow = row;

            row.scrollIntoView({ behavior: "smooth", block: "center" });
        }
    });

    if (!found) {
        document.querySelector('#addForm input[name="uid"]').value = scannedUID;
        document.querySelector('#addForm input[name="vendor"]').focus();

        if (lastHighlightedRow) lastHighlightedRow.classList.remove("highlight");
        lastHighlightedRow = null;
    }
};

// ------------------------ Export All ------------------------
document.getElementById("exportAllBtn").addEventListener("click", async () => {
    try {
        const res = await fetch("/api/exportAll");
        if (!res.ok) throw new Error("Export fehlgeschlagen");

        const blob = await res.blob();
        const url = URL.createObjectURL(blob);

        const a = document.createElement("a");
        a.href = url;
        a.download = "filament_package.json";
        document.body.appendChild(a);
        a.click();
        a.remove();
        URL.revokeObjectURL(url);
    } catch (err) {
        alert("Export fehlgeschlagen: " + err);
    }
});

// ------------------------ Import All ------------------------
const importForm = document.getElementById("importAllForm");
importForm.addEventListener("submit", async (e) => {
    e.preventDefault();
    const fileInput = importForm.querySelector("input[name=file]");
    if (!fileInput.files.length) return;

    const file = fileInput.files[0];
    const text = await file.text(); // erst hier lesen

    try {
        const res = await fetch("/api/importAll", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: text
        });

        if (res.ok) {
            alert("Import erfolgreich!");
            CONFIG = null;                // vorherige Config verwerfen
            await loadConfig();           // neue Config laden
            await loadTable();       // Tabelle neu laden
            await updateAddFormLEDs(); // Dropdowns neu befüllen
        } else {
            const errText = await res.text();
            alert("Import fehlgeschlagen: " + errText);
        }
    } catch (err) {
        alert("Import fehlgeschlagen: " + err);
    }
});



/* ------------------------ Tabelle laden ------------------------ */
async function loadTable() {
    await loadConfig();
    const res = await fetch("/filaments.json");
    if (!res.ok) {
        dbDiv.innerHTML = "<p>Fehler beim Laden der Daten.</p>";
        return;
    }

    const data = await res.json();
    const usedLEDs = new Set(data.map(e => Number(e.ledIndex)));

    let html = `
        <table>
            <tr>
                <th>UID</th><th>Hersteller</th><th>Typ</th>
                <th>Farbe</th><th>LED</th>
                <th colspan="2">Aktion</th>
            </tr>
    `;

    data.forEach((e, idx) => {
        html += `
            <tr>
                <td contenteditable="true" data-field="uid" data-idx="${idx}">${e.uid}</td>
                <td contenteditable="true" data-field="vendor" data-idx="${idx}">${e.vendor}</td>

                <td data-idx="${idx}">
                    <select data-field="type">
                        ${getTypeOptions(e.type)}
                    </select>
                </td>

                <td contenteditable="true" data-field="color" data-idx="${idx}">${e.color}</td>

                <td data-idx="${idx}">
                    ${buildLedDropdown(Number(e.ledIndex), usedLEDs)}
                </td>

                <td><button class="saveBtn" data-idx="${idx}">Speichern</button></td>
                <td><button class="deleteBtn" data-idx="${idx}">Löschen</button></td>
            </tr>
        `;
    });

    html += "</table>";
    dbDiv.innerHTML = html;

    activateButtons();
}


function getTypeOptions(selected) {
    const types = ["PLA","PLA+","PLA-CF","PETG","PETG-CF","ABS","ASA","TPU","Nylon","Holz"];
    return types
        .map(t => `<option value="${t}" ${t === selected ? "selected" : ""}>${t}</option>`)
        .join("");
}

/* ------------------------ Buttons aktivieren ------------------------ */
function activateButtons() {
    document.querySelectorAll(".saveBtn").forEach(btn => {
        btn.addEventListener("click", async () => {
            const idx = btn.dataset.idx;
            const row = btn.closest("tr");
            const entry = {};

            row.querySelectorAll("[data-field]").forEach(el => {
                const field = el.dataset.field;
                if (el.tagName === "SELECT") {
                    entry[field] = el.value;
                } else {
                    entry[field] = el.innerText.trim();
                }
            });

            const res = await fetch("/api/update", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(entry)
            });

            if (res.ok) {
                alert("Eintrag gespeichert!");
                await loadTable();
                await updateAddFormLEDs();
            } else {
                alert("Fehler beim Speichern!");
            }
        });
    });

    document.querySelectorAll(".deleteBtn").forEach(btn => {
        btn.addEventListener("click", async () => {
            if (!confirm("Eintrag wirklich löschen?")) return;

            const res = await fetch("/api/delete", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: "index=" + btn.dataset.idx
            });

            if (res.ok) {
                alert("Eintrag gelöscht!");
                await loadTable();
                await updateAddFormLEDs();
            } else {
                alert("Fehler beim Löschen!");
            }
        });
    });
}

/* ------------------------ Add-Formular ------------------------ */
addForm.addEventListener("submit", async (e) => {
    e.preventDefault();

    const fd = new FormData(addForm);
    const entry = {
        uid: fd.get("uid").trim(),
        vendor: fd.get("vendor").trim(),
        type: fd.get("type"),
        color: fd.get("color").trim(),
        ledIndex: Number(fd.get("ledIndex"))
    };

    const db = await (await fetch("/filaments.json")).json();

    const used = db.find(e => Number(e.ledIndex) === entry.ledIndex);
    if (used) {
        alert(`LED ${entry.ledIndex} wird bereits verwendet von UID ${used.uid}.`);
        return;
    }

    const res = await fetch("/api/add", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(entry)
    });

    if (res.ok) {
        alert("Eintrag hinzugefügt!");
        addForm.reset();
        await loadTable();
        await updateAddFormLEDs();
    } else {
        alert("Fehler beim Hinzufügen!");
    }
});

/* ------------------------ LED Dropdown ------------------------ */
function getFreeLEDs(data) {
    const maxLEDs = CONFIG.options.ledCount;
    const used = new Set(data.map(e => Number(e.ledIndex)));

    console.log("Max LEDs:", maxLEDs, "Used LEDs:", used);

    const free = [];
    for (let i = 0; i < maxLEDs; i++) {
        if (!used.has(i)) free.push(i);
    }
    return free;
}


async function updateAddFormLEDs() {
    await loadConfig();

    const data = await (await fetch("/filaments.json")).json();
    const free = getFreeLEDs(data);

    const sel = document.getElementById("ledIndexSelect");
    sel.innerHTML = "";
    free.forEach(v => {
        const opt = document.createElement("option");
        opt.value = v;
        opt.textContent = `LED ${v}`;
        sel.appendChild(opt);
    });
}


function buildLedDropdown(currentLED, usedLEDs) {

    const maxLEDs = CONFIG.options.ledCount;

    let html = `<select data-field="ledIndex">`;

    for (let i = 0; i < maxLEDs; i++) {
        const isUsed = usedLEDs.has(i);
        const isCurrent = (i === currentLED);

        // aktuelle LED darf benutzt bleiben
        if (!isUsed || isCurrent) {
            html += `<option value="${i}" ${isCurrent ? "selected" : ""}>
                        LED ${i}
                     </option>`;
        }
    }

    html += `</select>`;
    return html;
}

async function loadLedConfig() {
    const res = await fetch("/config.json");
    if(!res.ok) return;
    const config = await res.json();

    document.getElementById("maxLED").value = config.options.ledCount || 8;
    document.getElementById("ledPin").value = config.options.ledPin || 5;
    document.getElementById("ledBrightness").value = config.options.ledBrightness || 50;

    const col = config.options.ledColor || [255,0,0];
    const hex = '#' + col.map(v => v.toString(16).padStart(2,'0')).join('');
    document.getElementById("ledColor").value = hex;
}

document.getElementById("saveLedConfig").addEventListener("click", async () => {
    const ledCount = Number(document.getElementById("maxLED").value);
    const ledPin   = Number(document.getElementById("ledPin").value);
    const ledBrightness = Number(document.getElementById("ledBrightness").value);
    const col = document.getElementById("ledColor").value; // #RRGGBB
    const ledColor = [
        parseInt(col.substr(1,2),16),
        parseInt(col.substr(3,2),16),
        parseInt(col.substr(5,2),16)
    ];

    const res = await fetch("/api/updateLedConfig", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ ledCount, ledPin, ledBrightness, ledColor })
    });

    const text = await res.text();
    if (text.includes("REBOOTING")) {
        document.body.innerHTML = "<h2>Device is rebooting...</h2><p>Please wait...</p>";
        const check = setInterval(async () => {
            try {
                const ping = await fetch("/config.json", { cache: "no-store" });
                if (ping.ok) {
                    clearInterval(check);
                    location.reload();
                }
            } catch (e) {}
        }, 2000);
    } else {
        alert("Error saving LED config");
    }
});


loadLedConfig();

async function loadConfig() {
    if (!CONFIG) {
        const res = await fetch("/config.json");
        CONFIG = await res.json();
    }
    return CONFIG;
}

/* ------------------------ Init ------------------------ */
async function init() {
    await loadConfig();
    await loadTable();
    await updateAddFormLEDs();
}
init();
