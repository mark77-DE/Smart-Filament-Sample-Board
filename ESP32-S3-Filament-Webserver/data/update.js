document.getElementById("exportAllBtn").addEventListener("click", async () => {
    try {
        const res = await fetch("/api/exportAll");
        if (!res.ok) throw new Error("Export fehlgeschlagen");

        const blob = await res.blob();
        const url = URL.createObjectURL(blob);

        // Zeitstempel erzeugen
        const now = new Date();
        const pad = (n) => n.toString().padStart(2, "0");
        const timestamp = `${now.getFullYear()}${pad(now.getMonth()+1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;

        const a = document.createElement("a");
        a.href = url;
        a.download = `SpotMyFilament_Backup_${timestamp}.json`;
        document.body.appendChild(a);
        a.click();
        a.remove();
        URL.revokeObjectURL(url);
    } catch (err) {
        alert(err);
    }
});


async function uploadFS() {
  const fileInput = document.getElementById('fsFile');
  if (!fileInput.files.length) return alert("Bitte Datei auswählen");

  const file = fileInput.files[0];
  const status = document.getElementById('status');
  status.textContent = "Uploading FS...";

  try {
    const response = await fetch("/api/uploadFS", {
      method: "POST",
      body: file
    });

    if (response.ok) {
      status.textContent = "FS Upload erfolgreich!";
    } else {
      status.textContent = "FS Upload fehlgeschlagen: " + response.statusText;
    }
  } catch (err) {
    status.textContent = "FS Upload Error: " + err;
  }
}



    async function uploadFirmware() {
      const fileInput = document.getElementById('firmwareFile');
      if (!fileInput.files.length) return alert("Bitte Datei auswählen");

      const file = fileInput.files[0];
      const status = document.getElementById('status');
      status.textContent = "Uploading...";

      const formData = new FormData();
      formData.append("firmware", file);

      try {
        const response = await fetch("/api/otaUpdate", {
          method: "POST",
          body: file // wir senden direkt die Datei als Body
        });

        if (response.ok) {
          status.textContent = "Upload success! ESP rebooting...";
        } else {
          status.textContent = "Upload error: " + response.statusText;
        }
      } catch (err) {
        status.textContent = "Upload Error: " + err;
      }
    }