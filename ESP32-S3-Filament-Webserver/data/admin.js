const form = document.getElementById("uploadForm");
const dbDiv = document.getElementById("db");
const addForm = document.getElementById("addForm");

let lastHighlightedRow = null;

const ws = new WebSocket(`ws://${location.host}/ws`);

ws.onmessage = async (ev) => {
    let data;
    try {
        data = JSON.parse(ev.data);
    } catch {
        console.warn("WebSocket parse error:", ev.data);
        return;
    }

    if (!data.uid) return; // Nichts Relevantes

    const scannedUID = data.uid;

    // Tabelle neu laden, damit wir die aktuelle DB-Struktur haben
    await loadTable();

    // Passende Zeile suchen
    const rows = document.querySelectorAll("#db table tr");

    let found = false;
    rows.forEach((row, i) => {
        if (i === 0) return; // Header überspringen
        const uidCell = row.querySelector('td[data-field="uid"]');
        if (!uidCell) return;

        if (uidCell.innerText.trim() === scannedUID) {
            found = true;

            // alte Markierung entfernen
            if (lastHighlightedRow) {
                lastHighlightedRow.classList.remove("highlight");
            }

            // neue Markieren setzen
            row.classList.add("highlight");
            lastHighlightedRow = row;

            // Scrollen, damit der Nutzer es sieht
            row.scrollIntoView({ behavior: "smooth", block: "center" });
        }
    });

    if (!found) {
        // Zeile nicht gefunden → unbekannter Tag → neues Formular füllen
        const uidField = document.querySelector('#addForm input[name="uid"]');
        uidField.value = scannedUID;

        const vendorField = document.querySelector('#addForm input[name="vendor"]');
        vendorField.focus();

        // alte Markierung entfernen
        if (lastHighlightedRow) {
            lastHighlightedRow.classList.remove("highlight");
            lastHighlightedRow = null;
        }
    }
};


// --- File-Upload ---
form.addEventListener("submit", async (e) => {
    e.preventDefault();
    const fileInput = form.querySelector("input[name=file]");
    if (!fileInput.files.length) return;

    const formData = new FormData();
    formData.append("file", fileInput.files[0]);

    try {
        const res = await fetch("/api/import", {
            method: "POST",
            body: formData
        });

        if (res.ok) {
            alert("Upload erfolgreich!");
            await loadTable();
        } else {
            alert("Upload fehlgeschlagen!");
        }
    } catch(err) {
        console.error(err);
        alert("Fetch-Fehler: " + err);
    }
});

// --- Tabelle laden ---
async function loadTable() {
    const res = await fetch("/filaments.json");
    if (!res.ok) {
        dbDiv.innerHTML = "<p>Fehler beim Laden der Daten.</p>";
        return;
    }
    const data = await res.json();

    let html = "<table><tr><th>UID</th><th>Hersteller</th><th>Typ</th><th>Farbe</th><th>LED</th><th colspan='2'>Aktion</th></tr>";
    data.forEach((e, idx) => {
        html += `<tr>
          <td contenteditable="true" data-field="uid" data-idx="${idx}">${e.uid}</td>
          <td contenteditable="true" data-field="vendor" data-idx="${idx}">${e.vendor}</td>
          <td data-field="type" data-idx="${idx}">
            <select data-field="type">
              <option value="PLA" ${e.type==="PLA"?"selected":""}>PLA</option>
              <option value="PLA+" ${e.type==="PLA+"?"selected":""}>PLA+</option>
              <option value="PLA-CF" ${e.type==="PLA-CF"?"selected":""}>PLA-CF</option>
              <option value="PETG" ${e.type==="PETG"?"selected":""}>PETG</option>
              <option value="PETG-CF" ${e.type==="PETG-CF"?"selected":""}>PETG-CF</option>
              <option value="ABS" ${e.type==="ABS"?"selected":""}>ABS</option>
              <option value="ASA" ${e.type==="ASA"?"selected":""}>ASA</option>
              <option value="TPU" ${e.type==="TPU"?"selected":""}>TPU</option>
              <option value="Nylon" ${e.type==="Nylon"?"selected":""}>Nylon</option>
              <option value="Holz" ${e.type==="Holz"?"selected":""}>Holz-Filament</option>
            </select>
          </td>
          <td contenteditable="true" data-field="color" data-idx="${idx}">${e.color}</td>
          <td contenteditable="true" data-field="ledIndex" data-idx="${idx}">${e.ledIndex}</td>
          <td><button class="saveBtn" data-idx="${idx}">Speichern</button></td>
          <td><button class="deleteBtn" data-idx="${idx}">Löschen</button></td>
        </tr>`;
    });
    html += "</table>";
    dbDiv.innerHTML = html;

    activateButtons();
}

// --- Buttons aktivieren ---
function activateButtons() {
    document.querySelectorAll(".saveBtn").forEach(btn => {
        btn.addEventListener("click", async () => {
            const row = btn.closest("tr");
            const entry = {};
            row.querySelectorAll("[contenteditable], select").forEach(el => {
                const field = el.dataset.field;
                if (!field) return;
                entry[field] = el.tagName === "SELECT" ? el.value : el.innerText.trim();
            });

            const res = await fetch("/api/update", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(entry)
            });

            if (res.ok) {
                alert("Eintrag gespeichert!");
                await loadTable();
            } else {
                alert("Fehler beim Speichern!");
            }
        });
    });

    document.querySelectorAll(".deleteBtn").forEach(btn => {
        btn.addEventListener("click", async () => {
            const idx = btn.dataset.idx;
            if (!confirm("Eintrag wirklich löschen?")) return;

            const res = await fetch("/api/delete", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: "index=" + idx
            });

            if (res.ok) {
                alert("Eintrag gelöscht!");
                await loadTable();
            } else {
                alert("Fehler beim Löschen!");
            }
        });
    });
}

// --- Add-Formular ---
addForm.addEventListener("submit", async (e) => {
    e.preventDefault();
    const fd = new FormData(addForm);
    const entry = {
        uid: fd.get("uid").trim(),
        vendor: fd.get("vendor").trim(),
        type: fd.get("type"),
        color: fd.get("color").trim(),
        ledIndex: parseInt(fd.get("ledIndex"), 10)
    };

    try {
        const res = await fetch("/api/add", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(entry)
        });

        if (res.ok) {
            alert("Eintrag hinzugefügt!");
            addForm.reset();
            await loadTable();
        } else {
            const text = await res.text();
            alert("Fehler beim Hinzufügen: " + text);
        }
    } catch(err) {
        console.error(err);
        alert("Fetch-Fehler: " + err);
    }
});

// --- Init ---
async function init() {
    await loadTable();
}
init();
