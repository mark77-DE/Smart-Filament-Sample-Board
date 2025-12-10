const form = document.getElementById("uploadForm");
const dbDiv = document.getElementById("db");
const addForm = document.getElementById("addForm");
let lastHighlightedRow = null;

const maxLEDs = 120;

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

/* ------------------------ File-Upload ------------------------ */
form.addEventListener("submit", async (e) => {
    e.preventDefault();

    const fileInput = form.querySelector("input[name=file]");
    if (!fileInput.files.length) return;

    const formData = new FormData();
    formData.append("file", fileInput.files[0]);

    try {
        const res = await fetch("/api/import", { method: "POST", body: formData });
        if (res.ok) {
            alert("Upload erfolgreich!");
            await loadTable();
            await updateAddFormLEDs();
        } else {
            alert("Upload fehlgeschlagen!");
        }
    } catch (err) {
        console.error(err);
        alert("Fetch-Fehler: " + err);
    }
});

/* ------------------------ Tabelle laden ------------------------ */
async function loadTable() {
    const res = await fetch("/filaments.json");
    if (!res.ok) {
        dbDiv.innerHTML = "<p>Fehler beim Laden der Daten.</p>";
        return;
    }

    const data = await res.json();

    let html = `
        <table>
            <tr>
                <th>UID</th><th>Hersteller</th><th>Typ</th><th>Farbe</th><th>LED</th>
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
                <td contenteditable="true" data-field="ledIndex" data-idx="${idx}">${e.ledIndex}</td>

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
function getFreeLEDs(data, maxLEDs) {
    const used = new Set(data.map(e => Number(e.ledIndex)));
    const free = [];
    for (let i = 0; i < maxLEDs; i++) if (!used.has(i)) free.push(i);
    return free;
}

async function updateAddFormLEDs() {
    const data = await (await fetch("/filaments.json")).json();
    const free = getFreeLEDs(data, maxLEDs);

    const sel = document.getElementById("ledIndexSelect");
    sel.innerHTML = "";
    free.forEach(v => {
        const opt = document.createElement("option");
        opt.value = v;
        opt.textContent = `LED ${v}`;
        sel.appendChild(opt);
    });
}

/* ------------------------ Init ------------------------ */
async function init() {
    await loadTable();
    await updateAddFormLEDs();
}
init();
