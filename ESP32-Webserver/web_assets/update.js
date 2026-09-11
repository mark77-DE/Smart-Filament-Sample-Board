const fwInput = document.getElementById("firmwareFile");
const fsInput = document.getElementById("fsFile");


fwInput.addEventListener("change", () => {
  document.getElementById("fwFileName").textContent =
    fwInput.files.length ? fwInput.files[0].name : t("txt_no_file_selected");
});



async function uploadFirmware() {
  const fileInput = document.getElementById('firmwareFile');
  if (!fileInput.files.length) return alert(t("txt_select_file"));

  const file = fileInput.files[0];

  // Simple sanity check
  if (!file.name.endsWith("firmware.bin")) {
    return alert(t("txt_firmware_invalid"));
  }


  const status = document.getElementById('statusTxt');
  status.textContent = t("txt_uploading_fw");

  try {
    const response = await fetch("/api/otaUpdate", {
      method: "POST",
      body: file
    });

    if (response.ok) {
      status.textContent = t("txt_update_success");
    } else {
      let errMsg = response.statusText;
      try {
        const errJson = await response.json();
        if (errJson?.msg) errMsg = errJson.msg;
      } catch (_) {
        // Body war kein JSON, statusText bleibt Fallback
      }
      status.textContent = t("txt_update_failed") + ": " + errMsg;
      console.error("Upload FW failed:", errMsg);
    }
  } catch (err) {
    status.textContent = t("txt_update_failed") + ": " + err;
  }

}


