const form = document.getElementById("uploadForm");
const dbDiv = document.getElementById("db");

// --- File-Upload ---
form.addEventListener("submit", async (e) => {
    e.preventDefault();
    const fileInput = form.querySelector("input[name=file]");
    if (!fileInput.files.length) return;

    const formData = new FormData();
    formData.append("file", fileInput.files[0]);

    const res = await fetch("/api/import", {
        method: "POST",
        body: formData
    });

    if (res.ok) {
        alert("Upload erfolgreich!");
        loadTable();
    } else {
        alert("Upload fehlgeschlagen!");
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

    let html = "<table><tr><th>UID</th><th>Hersteller</th><th>Typ</th><th>Farbe</th><th>LED</th><th>Aktion</th></tr>";
    data.forEach((e, idx) => {
        html += `<tr>
          <td contenteditable="true" data-field="uid" data-idx="${idx}">${e.uid}</td>
          <td contenteditable="true" data-field="vendor" data-idx="${idx}">${e.vendor}</td>
          <td contenteditable="true" data-field="type" data-idx="${idx}">${e.type}</td>
          <td contenteditable="true" data-field="color" data-idx="${idx}">${e.color}</td>
          <td contenteditable="true" data-field="ledIndex" data-idx="${idx}">${e.ledIndex}</td>
          <td><button class="saveBtn" data-idx="${idx}">Speichern</button></td>
          <td><button class="deleteBtn" data-idx="${idx}">Löschen</button></td>
        </tr>`;
    });
    html += "</table>";
    dbDiv.innerHTML = html;

    // --- Save-Buttons aktivieren ---
    document.querySelectorAll(".saveBtn").forEach(btn => {
    btn.addEventListener("click", async () => {
        const row = btn.closest("tr");
        const entry = {};
        row.querySelectorAll("[contenteditable]").forEach(td => {
            const field = td.dataset.field;
            entry[field] = td.innerText.trim();
        });

        // JSON an Backend schicken
        const res = await fetch("/api/update", {
            method: "POST",
            headers: { "Content-Type": "application/json" }, // <-- WICHTIG
            body: JSON.stringify(entry)
        });

        if(res.ok) {
            alert("Eintrag gespeichert!");
            loadTable();  // Tabelle neu laden
        } else {
            alert("Fehler beim Speichern!");
        }
    });
});



const addForm = document.getElementById("addForm");
addForm.addEventListener("submit", async (e) => {
  e.preventDefault();
  const fd = new FormData(addForm);
  const entry = {
    uid: fd.get("uid").trim(),
    vendor: fd.get("vendor").trim(),
    type: fd.get("type").trim(),
    color: fd.get("color").trim(),
    ledIndex: parseInt(fd.get("ledIndex"), 10)
  };

  // Wichtig: send as raw body (no Content-Type header) OR use the chunk-style on server.
  // For the chunk-style server above, DO NOT set Content-Type.
  const res = await fetch("/api/add", {
    method: "POST",
    body: JSON.stringify(entry)
  });

  if(res.ok) {
    alert("Eintrag hinzugefügt!");
    addForm.reset();
    loadTable();
  } else {
    alert("Fehler beim Hinzufügen!");
  }
});


// --- Delete-Buttons aktivieren ---
document.querySelectorAll(".deleteBtn").forEach(btn => {
    btn.addEventListener("click", async () => {
        const idx = btn.dataset.idx;

        if(!confirm("Eintrag wirklich löschen?")) return;

        const res = await fetch("/api/delete", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: "index=" + idx
        });

        if(res.ok){
            alert("Eintrag gelöscht!");
            loadTable();  // Tabelle neu laden
        } else {
            alert("Fehler beim Löschen!");
        }
    });
});



}

// Tabelle initial laden
loadTable();
