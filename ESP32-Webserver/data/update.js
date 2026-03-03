const fwInput = document.getElementById("firmwareFile");
const fsInput = document.getElementById("fsFile");


fwInput.addEventListener("change", () => {
    document.getElementById("fwFileName").textContent =
        fwInput.files.length ? fwInput.files[0].name : "no file selected";
});

fsInput.addEventListener("change", () => {
    document.getElementById("fsFileName").textContent =
        fsInput.files.length ? fsInput.files[0].name : "no file selected";
});





document.getElementById("exportAllBtn").addEventListener("click", async () => {
    try {
        const res = await fetch("/api/exportAll");
        if (!res.ok) throw new Error("Export failed: " + res.statusText);

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
  if (!fileInput.files.length) return alert("Please choose file");

  const file = fileInput.files[0];

  // einfache Flüchtigkeitsprüfung
  if (!file.name.endsWith("littlefs.bin")) {
    return alert("FS file needs to start with 'littlefs' and end with '.bin'!");
  }

  const status = document.getElementById('status');
  status.textContent = "Uploading FS...";

  try {
    const response = await fetch("/api/uploadFS", {
      method: "POST",
      body: file
    });

    if (response.ok) {
      status.textContent = "FS upload success! Rebooting";
    } else {
      status.textContent = "FS upload failed: " + response.statusText;
    }
  } catch (err) {
    status.textContent = "FS upload error: " + err;
  }
}


async function uploadFirmware() {
  const fileInput = document.getElementById('firmwareFile');
  if (!fileInput.files.length) return alert("Please choose file");

  const file = fileInput.files[0];

  // einfache Flüchtigkeitsprüfung
  if (!file.name.endsWith("firmware.bin")) {
    return alert("Firmware file needs to start with 'firmware' and end with '.bin'!");
  }


  const status = document.getElementById('status');
  status.textContent = "Uploading...";

  try {
    const response = await fetch("/api/otaUpdate", {
      method: "POST",
      body: file
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




getVersion();
