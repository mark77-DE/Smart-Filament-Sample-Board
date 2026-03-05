const fwInput = document.getElementById("firmwareFile");
const fsInput = document.getElementById("fsFile");


fwInput.addEventListener("change", () => {
    document.getElementById("fwFileName").textContent =
        fwInput.files.length ? fwInput.files[0].name : t("txt_no_file_selected");
});

fsInput.addEventListener("change", () => {
    document.getElementById("fsFileName").textContent =
        fsInput.files.length ? fsInput.files[0].name : t("txt_no_file_selected");
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
  if (!fileInput.files.length) return alert(t("txt_select_file"));

  const file = fileInput.files[0];

  // einfache Flüchtigkeitsprüfung
  if (!file.name.endsWith("littlefs.bin")) {
    return alert(t("txt_filesystem_invalid"));
  }

  const status = document.getElementById('status');
  status.textContent = t("txt_uploading_fs");

  try {
    const response = await fetch("/api/uploadFS", {
      method: "POST",
      body: file
    });

    if (response.ok) {
      status.textContent = t("txt_update_success");
    } else {
      status.textContent = t("txt_update_failed") + ": " + response.statusText;
    }
  } catch (err) {
    status.textContent = t("txt_update_failed") + ": " + err;
  }
}


async function uploadFirmware() {
  const fileInput = document.getElementById('firmwareFile');
  if (!fileInput.files.length) return alert(t("txt_select_file"));

  const file = fileInput.files[0];

  // einfache Flüchtigkeitsprüfung
  if (!file.name.endsWith("firmware.bin")) {
    return alert(t("txt_firmware_invalid"));
  }


  const status = document.getElementById('status');
  status.textContent = t("txt_uploading_fw");

  try {
    const response = await fetch("/api/otaUpdate", {
      method: "POST",
      body: file
    });

    if (response.ok) {
      status.textContent = t("txt_update_success");
    } else {
      status.textContent = t("txt_update_failed") + ": " + response.statusText;
    }
  } catch (err) {
    status.textContent = t("txt_update_failed") + ": " + err;
  }
}


