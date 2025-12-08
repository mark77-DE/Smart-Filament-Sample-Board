const form = document.getElementById("uploadForm");
const dbDiv = document.getElementById("db");

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

async function loadTable() {
    const res = await fetch("/filaments.json");
    if (!res.ok) {
        dbDiv.innerHTML = "<p>Fehler beim Laden der Daten.</p>";
        return;
    }
    const data = await res.json();
    let html = "<table><tr><th>UID</th><th>Hersteller</th><th>Typ</th><th>Farbe</th><th>LED</th></tr>";
    data.forEach(e => {
        html += `<tr>
          <td>${e.uid}</td>
          <td>${e.vendor}</td>
          <td>${e.type}</td>
          <td>${e.color}</td>
          <td>${e.ledIndex}</td>
        </tr>`;
    });
    html += "</table>";
    dbDiv.innerHTML = html;
}

// Tabelle initial laden
loadTable();
