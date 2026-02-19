// -------------------- Globale Referenzen --------------------
const MAX_LENGTH = 30; // maximale Zeichenanzahl für vendor und color



const dbDiv = document.getElementById("db");
const addForm = document.getElementById("addForm");
const wsStatus = document.getElementById("wsStatus");
const editToggle = document.getElementById("editToggle");
const debugToggle = document.getElementById("debugToggle");

const selectLanguageSelect = document.getElementById("langSelect");


const daynightToggle = document.getElementById("daynightToggle");


const infoChipName = document.getElementById("infoChipName");
const infoCores = document.getElementById("infoCores");
const infoRevision = document.getElementById("infoRevision");
const infoFwVersion = document.getElementById("infoFwVersion");
const infoFlashSize = document.getElementById("infoFlashSize");
const infoNfcAvailable = document.getElementById("infoNfcAvailable");
const infoNfcFwVer = document.getElementById("infoNfcFwVer");
const infoNfcChipId = document.getElementById("infoNfcChipId");
const infoWifiMac = document.getElementById("infoWifiMac");
const infoWifiIp = document.getElementById("infoWifiIp");
const infoWifiGateway = document.getElementById("infoWifiGateway");
const infoWifiSsid = document.getElementById("infoWifiSsid");
const infoUptime = document.getElementById("infoUptime");
const infoWifiDns1 = document.getElementById("infoWifiDns1");
const infoWifiDns2 = document.getElementById("infoWifiDns2");
const infoWifiSubnet = document.getElementById("infoWifiSubnet");
const infoWifiRssi = document.getElementById("infoWifiRssi");
const infoHostname = document.getElementById("infoHostname");
const infoGitHash = document.getElementById("infoGitHash");
const infoBuildDate = document.getElementById("infoBuildDate");
const infoHeapSize = document.getElementById("infoHeapSize");
const infoFreeHeap = document.getElementById("infoHeapFree");
const infoSketchSize = document.getElementById("infoSketchSize");
const infoFreeSketch = document.getElementById("infoFreeSketch");
const infoSpiffsSize = document.getElementById("infoSpiffsSize");
const infoFreeSpiffs = document.getElementById("infoFreeSpiffs");
const infoBoardVariant = document.getElementById("infoBoardVariant");

const ledPinSelect = document.getElementById("ledPin");
const ledBrightnessInput = document.getElementById("ledBrightness");
const maxLEDInput = document.getElementById("maxLED");
const ledColorInput = document.getElementById("ledColor");
const ledColorErrorInput = document.getElementById("ledColorError");
const ledColorPulseInput = document.getElementById("ledColorPulse");
const ledTimeoutInput = document.getElementById("ledTimeout");
const animationAfterBootInput = document.getElementById("animationAfterBoot");


const nfcLedPinSelect = document.getElementById("nfcLedPin");
const nfcLedBrightnessInput = document.getElementById("nfcLedBrightness");
const nfcMaxLEDInput = document.getElementById("nfcMaxLED");
const nfcLedColorSuccessInput = document.getElementById("nfcLedColorSuccess");
const nfcLedColorErrorInput = document.getElementById("nfcLedColorError");
const nfcLedColorPulseInput = document.getElementById("nfcLedColorPulse");
const nfcLedSuccessBlinkEnabledInput = document.getElementById("nfcLedSuccessBlinkEnabled");
const nfcLedSuccessBlinkCountInput = document.getElementById("nfcLedSuccessBlinkCount");
const nfcLedSuccessBlinkMsInput = document.getElementById("nfcLedSuccessBlinkMs");
const nfcLedTimeoutInput = document.getElementById("nfcLedTimeout");



const buttonEnabledDiv = document.getElementById("buttonEnabled");
const buttonPinSelect = document.getElementById("buttonPin");
const buttonPullupInput = document.getElementById("buttonPullup");
const buttonDebounceInput = document.getElementById("buttonDebounceMs");
const buttonLongInput = document.getElementById("buttonLongMs");
const buttonDoubleInput = document.getElementById("buttonDoubleMs");
const buttonHoldInput = document.getElementById("buttonHoldMs");


const buzzerEnabledDiv = document.getElementById("buzzerEnabled");
const buzzerPinSelect = document.getElementById("buzzerPin");
const buzzerPassiveInput = document.getElementById("buzzerPassive");
const buzzerActiveHighInput = document.getElementById("buzzerActiveHigh");
const buzzerFreqInput = document.getElementById("buzzerFreq");
const buzzerSingleMsInput = document.getElementById("buzzerSingleMs");
const buzzerDoubleOnMsInput = document.getElementById("buzzerDoubleOnMs");
const buzzerDoubleGapMsInput = document.getElementById("buzzerDoubleGapMs");
const buzzerErrorOnMsInput = document.getElementById("buzzerErrorOnMs");
const buzzerErrorGapMsInput = document.getElementById("buzzerErrorGapMs");
const buzzerErrorCountInput = document.getElementById("buzzerErrorCount");

const mqttEnabledDiv = document.getElementById("mqttEnabled");
const mqttBrokerInput = document.getElementById("mqttBroker");
const mqttPortInput = document.getElementById("mqttPort");
const mqttUserInput = document.getElementById("mqttUser");
const mqttPasswordInput = document.getElementById("mqttPassword");
const mqttClientIdInput = document.getElementById("mqttClientId");
const mqttBaseTopicInput = document.getElementById("mqttBaseTopic");
const mqttHADiscoveryCheck = document.getElementById("mqttHADiscovery");
const mqttHADiscoveryPrefixInput = document.getElementById("mqttHADiscoveryPrefix");


const webLedTimeoutInput = document.getElementById("webLedTimeout");

const importFileInput = document.getElementById("importFile");
const importBtn = document.getElementById("importBtn");

const toggleBtn = document.getElementById("toggleSettings");
const section = document.getElementById("sectionSettings");

const uidInput = document.querySelector('input[name="uid"]');

const hostnameInput = document.getElementById("hostname");

const ledSelect = document.getElementById("ledIndexSelect");



let FILAMENT_DATA = [];
let EDIT_MODE = false;
let CONFIGV2 = null;
let lastHighlightedRow = null;
let BOARD_VARIANT = "unknown"; // wird in config.js überschrieben, dient aber hier schon als Fallback / Referenz




document.querySelectorAll(".infoTitle[data-toggle]").forEach(title => {
    title.addEventListener("click", () => {
        title.parentElement.classList.toggle("open");
    });
});

// -------------------- WebSocket --------------------
const socket = new WebSocket(`ws://${location.host}/ws`);

socket.onopen = () => updateWSStatus(true);
socket.onclose = () => {
    updateWSStatus(false);
    document.body.innerHTML = `
        <h2><span data-i18n="txt_esp_disconnected">ESP disconnected...</span></h2>
        <p><span data-i18n="txt_page_reload">Page will reload in 2 seconds.</span></p>
    `;
    setTimeout(() => location.reload(), 2000);
};
socket.onerror = () => updateWSStatus(false);
socket.onmessage = handleWSMessage;


// -------------------- Farb-Utils --------------------
function hexToRgb(hex) {
    if (!hex || hex[0] !== '#' || hex.length !== 7) return [255, 0, 0]; // Default Rot
    const r = parseInt(hex.substr(1, 2), 16);
    const g = parseInt(hex.substr(3, 2), 16);
    const b = parseInt(hex.substr(5, 2), 16);
    return [r, g, b];
}

function rgbToHex(rgb) {
    if (!Array.isArray(rgb) || rgb.length < 3) return "#ff0000"; // Default Rot
    const r = rgb[0].toString(16).padStart(2, '0');
    const g = rgb[1].toString(16).padStart(2, '0');
    const b = rgb[2].toString(16).padStart(2, '0');
    return `#${r}${g}${b}`;
}



function updateImportUI() {
    const hasFile = importFileInput.files.length > 0;

    const fileNameSpan = document.getElementById("selectedFileName");

    if (hasFile) {
        fileNameSpan.textContent = importFileInput.files[0].name;
        importBtn.disabled = false;
    } else {
        fileNameSpan.textContent = "No file selected";
        importBtn.disabled = true;
    }
}


function updateWSStatus(connected) {

    // Key je nach Status
    const key = connected ? 'websocket_connected' : 'websocket_disconnected';

    wsStatus.textContent = i18nData[key] || (connected ? "WebSocket: connected" : "WebSocket: disconnected");
    wsStatus.classList.toggle("ws-connected", connected);
    wsStatus.classList.toggle("ws-disconnected", !connected);
}

// -------------------- Edit Mode --------------------
editToggle.addEventListener("change", () => {
    EDIT_MODE = editToggle.checked;
    applyEditMode();
});

function applyEditMode() {
    const table = document.querySelector("#table");
    if (!table) return;
    table.querySelectorAll(".tableRow").forEach(row => {
        row.querySelectorAll("span[contenteditable]").forEach(cell => cell.contentEditable = EDIT_MODE);
        row.querySelectorAll("select").forEach(sel => sel.disabled = !EDIT_MODE);
        row.querySelectorAll(".saveBtn, .deleteBtn").forEach(btn => EDIT_MODE ? btn.removeAttribute("hidden") : btn.setAttribute("hidden", ""));
    });
}


// -------------------- WebSocket Handler --------------------
async function handleWSMessage(ev) {
    let data;
    try {
        data = JSON.parse(ev.data);
    } catch (err) {
        if (CONFIG_V2?.system?.debugMode) {
            console.error("Fehler beim Parsen der WS-Daten:", ev.data, err);
        }
        return;
    }

    if (!data.uid) {
        return;
    }

    // UID nur Hex-Ziffern, Großschreibung vereinheitlicht
    const scannedUID = data.uid.replace(/[^a-fA-F0-9]/g, '').toUpperCase();

    const rows = document.querySelectorAll("#db .tableRow");
    let highlighted = false;

    rows.forEach((row) => {
        const uidCell = row.querySelector(".uid");
        if (!uidCell) return;

        const rowUID = uidCell.textContent.replace(/[^a-fA-F0-9]/g, '').toUpperCase();
        if (rowUID === scannedUID) {
            highlighted = true;

            if (lastHighlightedRow && lastHighlightedRow !== row) {
                lastHighlightedRow.classList.remove("highlight");
            }

            row.classList.add("highlight");
            lastHighlightedRow = row;

            row.scrollIntoView({ behavior: "smooth", block: "center" });
        }
    });

    if (!highlighted) {
        document.querySelector('#addForm input[name="uid"]').value = data.uid;
        document.querySelector('#addForm input[name="vendor"]').focus();

        if (lastHighlightedRow) lastHighlightedRow.classList.remove("highlight");
        lastHighlightedRow = null;
    }
}


// -------------------- Export / Import --------------------
document.getElementById("exportAllBtn").addEventListener("click", async () => {
    try {
        const res = await fetch("/api/exportAll");
        if (!res.ok) throw new Error(t("txt_export_failed") + ": " + await res.text());

        const blob = await res.blob();
        const url = URL.createObjectURL(blob);

        // Zeitstempel erzeugen
        const now = new Date();
        const pad = (n) => n.toString().padStart(2, "0");
        const timestamp = `${now.getFullYear()}${pad(now.getMonth() + 1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;

        const a = document.createElement("a");
        a.href = url;
        a.download = `SpotMyFilament_${timestamp}_${infoBoardVariant.textContent}.json`;
        document.body.appendChild(a);
        a.click();
        a.remove();
        URL.revokeObjectURL(url);
    } catch (err) {
        alert(err);
    }
});

document.getElementById("importAllForm").addEventListener("submit", async e => {
    e.preventDefault();
    const fileInput = document.getElementById("importFile");
    if (!fileInput.files.length) return;
    const text = await fileInput.files[0].text();
    try {
        const res = await fetch("/api/importAll", { method: "POST", headers: { "Content-Type": "application/json" }, body: text });
        if (res.ok) { 
            alert(t("txt_import_success"));

            await loadFilaments();
            renderTable();
            updateAddFormSamples();
            await loadConfig_V2(); 
        }
        else { 
            alert(t("txt_import_failed") + ": " + await res.text()); 
        }
    } catch (err) { 
        alert(t("txt_import_failed") + ": " + err); 
    }
});




// -------------------- Reboot --------------------
document.getElementById("rebootBtn").addEventListener("click", async () => {
    if (!confirm(t("txt_shure_reboot"))) return;
    try { await fetch("/api/reboot", { method: "POST" }); } catch { }
    document.body.innerHTML = `
        <h2 data-i18n="txt_esp_disconnected">ESP disconnected...</h2>
        <p data-i18n="txt_page_reload">Page will reload in 2 seconds.</p>
    `;
    setTimeout(() => location.reload(), 2000);
});


// -------------------- Add Form --------------------
addForm.addEventListener("submit", async e => {
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
    if (used) { alert(`LED ${entry.ledIndex + 1} bereits verwendet von UID ${used.uid}`); return; }
    const res = await fetch("/api/add", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(entry) });
    if (res.ok) { alert(t("txt_add_sample_success")); addForm.reset(); await loadFilaments(); renderTable(); updateAddFormSamples(); }
    else alert(t("txt_add_sample_failed"));
});


// -------------------- Table / LED --------------------
function renderTable() {

    if (!Array.isArray(FILAMENT_DATA)) {
        dbDiv.innerHTML = "<p>Invalid filament data</p>";
        return;
    }

    const data = FILAMENT_DATA;
    const usedLEDs = new Set(data.map(e => Number(e.ledIndex)));

    let html = `
        <div id="table">
            <div id="tableHeader">
                <span id="uidHeader" data-i18n="txt_uid">Tag UID</span>
                <span id="vendorHeader" data-i18n="txt_vendor">Name</span>
                <span id="typeHeader" data-i18n="txt_type">Type</span>
                <span id="colorHeader" data-i18n="txt_color">Color</span>
                <span id="ledHeader" data-i18n="txt_led">LED</span>
                <span id="actionHeader" data-i18n="txt_action">Action</span>
            </div>
    `;

    data.forEach((e, idx) => {
        html += `
            <div class="tableRow">
                <span class="uid" contenteditable="${EDIT_MODE}" data-field="uid" data-idx="${idx}">${e.uid}</span>
                <span class="vendor" contenteditable="${EDIT_MODE}" data-field="vendor" data-idx="${idx}">${e.vendor}</span>
                <span class="type" data-idx="${idx}">
                    <select data-field="type" ${EDIT_MODE ? "" : "disabled"}>
                        ${getTypeOptions(e.type)}
                    </select>
                </span>
                <span class="color" contenteditable="${EDIT_MODE}" data-field="color" data-idx="${idx}">${e.color}</span>
                <span class="led" data-idx="${idx}">
                    ${buildLedDropdown(Number(e.ledIndex), usedLEDs, !EDIT_MODE)}
                </span>
                <div class="actionCell">
                    <button class="saveBtn" data-idx="${idx}" ${EDIT_MODE ? "" : "hidden"}>Speichern</button>
                    <button class="deleteBtn" data-uid="${e.uid}" ${EDIT_MODE ? "" : "hidden"}>Löschen</button>
                </div>
            </div>
        `;
    });

    html += "</div>";
    dbDiv.innerHTML = html;

    const tableDiv = document.getElementById("table");  // jetzt existiert es
    tableDiv.addEventListener("change", (event) => {
        const target = event.target;
        if (target.matches("select[data-field='ledIndex']")) {
            const index = Number(target.value);
            highlightLedIndex(index);
        }
    });

    dbDiv.addEventListener('input', e => {
        if (e.target.matches('span[data-field="vendor"], span[data-field="color"]')) {
            if (e.target.innerText.length > MAX_LENGTH) {
                e.target.innerText = e.target.innerText.slice(0, MAX_LENGTH);

                // Cursor ans Ende setzen, sonst springt er nach vorne
                const range = document.createRange();
                const sel = window.getSelection();
                range.selectNodeContents(e.target);
                range.collapse(false);
                sel.removeAllRanges();
                sel.addRange(range);
            }
        }
    });

    activateButtons();
    applyEditMode(); // Buttons / selects im Edit-Modus korrekt setzen

    // ---- UI aus CONFIG füllen ----
    const sys = CONFIGV2.system || {};
    const led = CONFIGV2.led || {};
    const nfc = CONFIGV2.nfc || {};
    const buz = CONFIGV2.buzzer || {};
    const btn = CONFIGV2.button || {};
    const mqtt = CONFIGV2.mqttConfig || {};

    debugToggle.checked = !!(sys.debugMode);
    
    loadHelpAndLang(CONFIGV2.system.defaultLanguage); // Standard-Sprache
    selectLanguageSelect.value = CONFIGV2.system.defaultLanguage;
    setupLangSwitcher('langSelect');
    // Help-Icons einfügen
    injectHelpIcons();

    // --- Mögliche fehlende Pins in den Dropdowns ergänzen ---
    ensureOption(ledPinSelect, led.pin);
    ensureOption(nfcLedPinSelect, nfc.pin);
    if (buttonPinSelect) ensureOption(buttonPinSelect, btn.pin ?? -1);
    if (buzzerPinSelect) ensureOption(buzzerPinSelect, buz.pin ?? -1);

    // --- Werte setzen ---
    ledPinSelect.value = String(led.pin);
    nfcLedPinSelect.value = String(nfc.pin);
    if (buttonPinSelect) buttonPinSelect.value = String(btn.pin ?? -1);
    if (buzzerPinSelect) buzzerPinSelect.value = String(buz.pin ?? -1);

    ledBrightnessInput.value = ledValueToPercent(led.brightness ?? 99);
    nfcLedBrightnessInput.value = ledValueToPercent(nfc.brightness ?? 99);

    maxLEDInput.value = led.count ?? 1;
    nfcMaxLEDInput.value = nfc.count ?? 1;

    ledTimeoutInput.value = led.timeout ?? 1111;
    nfcLedTimeoutInput.value = nfc.timeout ?? 3333;

    if (webLedTimeoutInput) {
        webLedTimeoutInput.value = sys.webLEDTimeout ?? led.timeout ?? 4999; // NEU (Fallback)
    }


    ledColorInput.value = rgbToHex(led.color ?? [255, 0, 0]);
    ledColorErrorInput.value = rgbToHex(led.colorError ?? [255, 0, 0]);
    ledColorPulseInput.value = rgbToHex(led.colorPulse ?? [0, 51, 170]);

    nfcLedColorSuccessInput.value = rgbToHex(nfc.colorSuccess ?? [0, 255, 0]);
    nfcLedColorErrorInput.value = rgbToHex(nfc.colorError ?? [255, 0, 0]);
    nfcLedColorPulseInput.value = rgbToHex(nfc.colorPulse ?? [0, 51, 170]);

    // --- Success Blink UI ---
    nfcLedSuccessBlinkEnabledInput.checked = nfc.successBlinkEnabled ?? true;
    nfcLedSuccessBlinkCountInput.value = nfc.successBlinkCount ?? 1;
    nfcLedSuccessBlinkMsInput.value = nfc.successBlinkMs ?? 111;

    const syncBlinkUi = () => {
        const en = !!nfcLedSuccessBlinkEnabledInput.checked;
        nfcLedSuccessBlinkCountInput.disabled = !en;
        nfcLedSuccessBlinkMsInput.disabled = !en;
    };
    nfcLedSuccessBlinkEnabledInput.onchange = syncBlinkUi;
    syncBlinkUi();

    // --- Button UI ---
    if (buttonPullupInput) buttonPullupInput.checked = (btn.pullup ?? true);
    if (buttonDebounceInput) buttonDebounceInput.value = btn.debounceMs ?? 33;
    if (buttonLongInput) buttonLongInput.value = btn.longMs ?? 888;
    if (buttonDoubleInput) buttonDoubleInput.value = btn.doubleMs ?? 444;
    if (buttonHoldInput) buttonHoldInput.value = btn.holdMs ?? 222;

    // --- Buzzer UI ---
    if (buzzerPassiveInput) buzzerPassiveInput.checked = (buz.passive ?? false);
    if (buzzerActiveHighInput) buzzerActiveHighInput.checked = (buz.activeHigh ?? true);
    if (buzzerFreqInput) buzzerFreqInput.value = buz.freq ?? 4444;
    if (buzzerSingleMsInput) buzzerSingleMsInput.value = buz.singleMs ?? 88;
    if (buzzerDoubleOnMsInput) buzzerDoubleOnMsInput.value = buz.doubleOnMs ?? 66;
    if (buzzerDoubleGapMsInput) buzzerDoubleGapMsInput.value = buz.doubleGapMs ?? 88;
    if (buzzerErrorOnMsInput) buzzerErrorOnMsInput.value = buz.errorOnMs ?? 55;
    if (buzzerErrorGapMsInput) buzzerErrorGapMsInput.value = buz.errorGapMs ?? 66;
    if (buzzerErrorCountInput) buzzerErrorCountInput.value = buz.errorCount ?? 1;

    // --- MQTT UI ---
    if (mqttEnabledDiv) mqttEnabledDiv.checked = !!mqtt.enabled;
    if (mqttBrokerInput) mqttBrokerInput.value = mqtt.server ?? "";
    if (mqttPortInput) mqttPortInput.value = mqtt.port ?? 1883;
    if (mqttUserInput) mqttUserInput.value = mqtt.user ?? "";
    if (mqttPasswordInput) mqttPasswordInput.value = mqtt.password ?? "";
    if (mqttClientIdInput) mqttClientIdInput.value = mqtt.clientId ?? "";
    if (mqttBaseTopicInput) mqttBaseTopicInput.value = mqtt.baseTopic ?? "";
    if (mqttHADiscoveryCheck) mqttHADiscoveryCheck.checked = !!mqtt.haDiscovery;
    if (mqttHADiscoveryPrefixInput) mqttHADiscoveryPrefixInput.value = mqtt.haDiscoveryPrefix ?? "";

    // --- Hostsettings ---
    if (hostnameInput) hostnameInput.value = sys.hostname ?? "hostname";
    if (daynightToggle) daynightToggle.checked = !sys.darkmode ?? true;
    
    document.body.classList.toggle("daymode", daynightToggle.checked);

    // Nach dem Setzen: Sperrlogik ausführen
    updatePinOptions();
}

function getTypeOptions(selected) {
    return ["PLA", "PLA+", "PLA-CF", "PLA-Matte", "PLA-Silk", "PETG", "PETG-CF", "ABS", "ASA", "TPU", "Nylon", "Holz"]
        .map(t => `<option value="${t}" ${t === selected ? "selected" : ""}>${t}</option>`).join("");
}

function buildLedDropdown(currentLED, usedLEDs, disabled = false) {
    let html = `<select data-field="ledIndex" ${disabled ? "disabled" : ""}>`;
    for (let i = 0; i < CONFIGV2.led.count; i++) {
        if (!usedLEDs.has(i) || i === currentLED) {
            html += `<option value="${i}" ${i === currentLED ? "selected" : ""}>LED ${i + 1}</option>`;
        }
    }
    html += `</select>`;
    return html;
}


// -------------------- Buttons für Save/Delete --------------------
function activateButtons() {
    document.querySelectorAll(".saveBtn").forEach(btn => btn.addEventListener("click", async () => {
        //if (!confirm("Eintrag sichern?")) return;
        const idx = Number(btn.dataset.idx); const row = btn.closest(".tableRow");
        const entry = { idx };
        row.querySelectorAll("[data-field]").forEach(el => {
            const field = el.dataset.field;
            entry[field] = el.tagName === "SELECT" ? el.value : el.innerText.trim();
        });
        const res = await fetch("/api/update", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(entry) });
        if (res.ok) { alert(t("txt_save_sample_success")); await loadFilaments(); renderTable(); updateAddFormSamples(); } else alert(t("txt_save_sample_failed"));
    }));

    document.querySelectorAll(".deleteBtn").forEach(btn => btn.addEventListener("click", async () => {
        if (!confirm(t("txt_delete_sample_shure"))) return;

        const uid = btn.dataset.uid;
        const res = await fetch("/api/delete", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: `uid=${encodeURIComponent(uid)}`
        });

        if (res.ok) { alert(t("txt_delete_sample_success")); await loadFilaments(); renderTable(); updateAddFormSamples(); }
        else alert(t("txt_delete_sample_failed"));
    }));
}

async function updateAddFormSamples() {
    const data = FILAMENT_DATA;
    const free = [];
    for (let i = 0; i < CONFIGV2.led.count; i++) {
        if (!data.find(e => Number(e.ledIndex) === i)) free.push(i);
    }
    const sel = document.getElementById("ledIndexSelect");
    sel.innerHTML = "";
    free.forEach(v => {
        const opt = document.createElement("option");
        opt.value = v;
        opt.textContent = `LED ${v + 1}`;
        sel.appendChild(opt);
    });
}


async function saveConfigHandler() {
    if (!confirm(t("txt_save_config_sure"))) return;

    // --- Filament LED ---
    const ledCount = Number(document.getElementById("maxLED").value);
    const ledPin = Number(document.getElementById("ledPin").value);
    const ledBrightness = percentToLedValue(Number(document.getElementById("ledBrightness").value));
    const ledColor = hexToRgb(document.getElementById("ledColor").value);
    const ledColorError = hexToRgb(document.getElementById("ledColorError").value);
    const ledColorPulse = hexToRgb(document.getElementById("ledColorPulse").value);
    const ledTimeout = Number(document.getElementById("ledTimeout").value);
    const webLEDTimeout = webLedTimeoutInput ? Number(webLedTimeoutInput.value) : ledTimeout;

    const debugMode = debugToggle.checked;
    const darkmode = !daynightToggle.checked;
    const defaultLanguage = selectLanguageSelect.value;
    const animationAfterBoot = animationAfterBootInput.checked;
    

    // --- NFC ---
    const nfcLedCount = Number(document.getElementById("nfcMaxLED").value);
    const nfcLedPin = Number(document.getElementById("nfcLedPin").value);
    const nfcLedBrightness = percentToLedValue(Number(document.getElementById("nfcLedBrightness").value));
    const nfcLedColorSuccess = hexToRgb(document.getElementById("nfcLedColorSuccess").value);
    const nfcLedColorError = hexToRgb(document.getElementById("nfcLedColorError").value);
    const nfcLedTimeout = Number(document.getElementById("nfcLedTimeout").value);
    const nfcLedColorPulse = hexToRgb(document.getElementById("nfcLedColorPulse").value);
    const nfcLedSuccessBlinkEnabled = document.getElementById("nfcLedSuccessBlinkEnabled").checked;
    const nfcLedSuccessBlinkCount = Number(document.getElementById("nfcLedSuccessBlinkCount").value);
    const nfcLedSuccessBlinkMs = Number(document.getElementById("nfcLedSuccessBlinkMs").value);

    // --- Button ---
    const buttonPin = buttonPinSelect ? Number(buttonPinSelect.value) : -1;
    const buttonPullup = buttonPullupInput ? buttonPullupInput.checked : true;
    const buttonDebounceMs = buttonDebounceInput ? Number(buttonDebounceInput.value) : 30;
    const buttonLongMs = buttonLongInput ? Number(buttonLongInput.value) : 800;
    const buttonDoubleMs = buttonDoubleInput ? Number(buttonDoubleInput.value) : 400;
    const buttonHoldMs = buttonHoldInput ? Number(buttonHoldInput.value) : 250;

    // --- Buzzer ---
    const buzzerPin = buzzerPinSelect ? Number(buzzerPinSelect.value) : -1;
    const buzzerActiveHigh = buzzerActiveHighInput ? buzzerActiveHighInput.checked : true;
    const buzzerFreq = buzzerFreqInput ? Number(buzzerFreqInput.value) : 4000;
    const buzzerSingleMs = buzzerSingleMsInput ? Number(buzzerSingleMsInput.value) : 80;
    const buzzerDoubleOnMs = buzzerDoubleOnMsInput ? Number(buzzerDoubleOnMsInput.value) : 60;
    const buzzerDoubleGapMs = buzzerDoubleGapMsInput ? Number(buzzerDoubleGapMsInput.value) : 80;
    const buzzerErrorOnMs = buzzerErrorOnMsInput ? Number(buzzerErrorOnMsInput.value) : 50;
    const buzzerErrorGapMs = buzzerErrorGapMsInput ? Number(buzzerErrorGapMsInput.value) : 60;
    const buzzerErrorCount = buzzerErrorCountInput ? Number(buzzerErrorCountInput.value) : 3;

    // --- MQTT ---
    const mqttEnabledCheck = mqttEnabledDiv ? mqttEnabledDiv.checked : false;
    const mqttBrokerInputValue = mqttBrokerInput ? mqttBrokerInput.value.trim() : "";
    const mqttPortInputValue = mqttPortInput ? Number(mqttPortInput.value) : 1883;
    const mqttUserInputValue = mqttUserInput ? mqttUserInput.value.trim() : "";
    const mqttPasswordInputValue = mqttPasswordInput ? mqttPasswordInput.value : "";
    const mqttClientIdInputValue = mqttClientIdInput ? mqttClientIdInput.value.trim() : "";
    const mqttBaseTopicInputValue = mqttBaseTopicInput ? mqttBaseTopicInput.value.trim() : "";
    const mqttHADiscoveryCheckValue = mqttHADiscoveryCheck ? mqttHADiscoveryCheck.checked : false;
    const mqttHADiscoveryPrefixInputValue = mqttHADiscoveryPrefixInput ? mqttHADiscoveryPrefixInput.value.trim() : "homeassistant";

    // --- Hostsettings ---
    const hostname = hostnameInput ? hostnameInput.value.trim() : "hostname";

    try {
        await fetch("/api/updateConfig", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
                system: {
                    darkmode,
                    debugMode,
                    webLEDTimeout,
                    animationAfterBoot,
                    hostname: hostname || "filament-board",
                    defaultLanguage
                },
                led: {
                    count: ledCount,
                    pin: ledPin,
                    brightness: ledBrightness,
                    timeout: ledTimeout,
                    color: ledColor,
                    colorError: ledColorError,
                    colorPulse: ledColorPulse
                },
                nfc: {
                    count: nfcLedCount,
                    pin: nfcLedPin,
                    brightness: nfcLedBrightness,
                    timeout: nfcLedTimeout,
                    colorSuccess: nfcLedColorSuccess,
                    colorError: nfcLedColorError,
                    colorPulse: nfcLedColorPulse,
                    successBlinkEnabled: nfcLedSuccessBlinkEnabled,
                    successBlinkCount: nfcLedSuccessBlinkCount,
                    successBlinkMs: nfcLedSuccessBlinkMs
                },
                button: {
                    pin: buttonPin,
                    pullup: buttonPullup,
                    debounceMs: buttonDebounceMs,
                    longMs: buttonLongMs,
                    doubleMs: buttonDoubleMs,
                    holdMs: buttonHoldMs
                },
                buzzer: {
                    pin: buzzerPin,
                    activeHigh: buzzerActiveHigh,
                    freq: buzzerFreq,
                    singleMs: buzzerSingleMs,
                    doubleOnMs: buzzerDoubleOnMs,
                    doubleGapMs: buzzerDoubleGapMs,
                    errorOnMs: buzzerErrorOnMs,
                    errorGapMs: buzzerErrorGapMs,
                    errorCount: buzzerErrorCount
                },
                mqttConfig: {
                    enabled: mqttEnabledCheck,
                    server: mqttBrokerInputValue,
                    port: mqttPortInputValue,
                    user: mqttUserInputValue,
                    password: mqttPasswordInputValue,
                    clientId: mqttClientIdInputValue,
                    baseTopic: mqttBaseTopicInputValue,
                    haDiscovery: mqttHADiscoveryCheckValue,
                    haDiscoveryPrefix: mqttHADiscoveryPrefixInputValue
                }
            })
        });
    } catch (e) {
        // ESP offline → ignorieren
    }
}





// -------------------- load config --------------------
async function loadConfig_V2() {
    const res = await fetch("/config_v2.json");
    if (!res.ok) throw new Error(t("txt_load_config_failed"));
    const json = await res.json();
    CONFIGV2 = json;
}


// -------------------- Pin-Dropdowns: Helfer & Sperrlogik --------------------
// ergänzt einen Wert, falls er noch nicht als <option> existiert
function ensureOption(select, value) {
    if (!select || value === undefined || value === null) return;
    const v = String(value);
    const exists = Array.from(select.options).some(o => o.value === v);
    if (!exists) {
        const opt = document.createElement("option");
        opt.value = v;
        opt.textContent = v;
        select.appendChild(opt);
    }
}

// Sperrlogik: alle vier (falls vorhanden) gegenseitig exklusiv, "-1" bleibt frei
function updatePinOptions() {
    const selects = [ledPinSelect, nfcLedPinSelect, buttonPinSelect, buzzerPinSelect].filter(Boolean);

    // Alle Optionen aktivieren
    selects.forEach(s => Array.from(s.options).forEach(o => o.disabled = false));

    // Jeden Select gegen die anderen abgleichen
    selects.forEach(s => {
        const selectedValuesOfOthers = new Set(
            selects.filter(other => other !== s).map(other => String(other.value))
        );
        Array.from(s.options).forEach(o => {
            const val = String(o.value);
            // "-1" (deaktiviert) niemals sperren, und nicht die eigene Auswahl
            if (val !== String(s.value) && val !== "-1" && selectedValuesOfOthers.has(val)) {
                o.disabled = true;
            }
        });
    });
}



// -------------------- Color Presets (HEX) --------------------
const PRESET_COLORS = [
    "#ff0000", // Rot
    "#00ff00", // Grün
    "#0000ff", // Blau
    "#ffff00", // Gelb
    "#ff7a00", // Orange (schön satt)
    "#ff00ff", // Magenta
    "#00ffff"  // Cyan
];

function normalizeHex(v) {
    if (!v) return "";
    v = String(v).trim().toLowerCase();
    return /^#([0-9a-f]{6})$/.test(v) ? v : "";
}

function autoFixHex(input) {
    let v = String(input.value).trim();

    // exakt 6 Hex-Zeichen ohne #
    if (/^[0-9a-fA-F]{6}$/.test(v)) {
        input.value = "#" + v.toLowerCase();
        return;
    }

    // # + 6 Hex-Zeichen → nur normalisieren
    if (/^#([0-9a-fA-F]{6})$/.test(v)) {
        input.value = v.toLowerCase();
    }
}

function clearActive(container) {
    container.querySelectorAll(".colorSwatch.active").forEach(el => el.classList.remove("active"));
}

function updateActiveForInput(container, input) {
    const current = normalizeHex(input.value);
    clearActive(container);
    if (!current) return;
    const match = container.querySelector(`.colorSwatch[data-hex="${current}"]`);
    if (match) match.classList.add("active");
}

function initColorPresets() {
    document.querySelectorAll(".colorPresets").forEach((container) => {
        const targetId = container.dataset.target;
        const input = document.getElementById(targetId);
        if (!input) return;

        container.innerHTML = "";

        PRESET_COLORS.forEach((hex) => {
            const hexNorm = hex.toLowerCase();

            const sw = document.createElement("div");
            sw.className = "colorSwatch";
            sw.style.backgroundColor = hexNorm;
            sw.title = hexNorm;
            sw.dataset.hex = hexNorm;

            sw.addEventListener("click", () => {
                input.value = hexNorm;
                updateActiveForInput(container, input);
                input.dispatchEvent(new Event("input", { bubbles: true }));
                input.dispatchEvent(new Event("change", { bubbles: true }));
            });

            container.appendChild(sw);
        });

        input.addEventListener("input", () => {
            autoFixHex(input);
            updateActiveForInput(container, input);
        });

        input.addEventListener("change", () => {
            autoFixHex(input);
            updateActiveForInput(container, input);
        });

        updateActiveForInput(container, input);
    });
}




function disableButton() {
    const pin = parseInt(buttonPinSelect.value, 10);


    if (pin === -1) {
        buttonEnabledDiv.classList.add("disabled");
    } else {
        buttonEnabledDiv.classList.remove("disabled");
    }
}

function disableBuzzer() {
    const pin = parseInt(buzzerPinSelect.value, 10);


    if (pin === -1) {
        buzzerEnabledDiv.classList.add("disabled");
    } else {
        buzzerEnabledDiv.classList.remove("disabled");
    }
}

function isValidNFCUID(uid) {
    // Großschreiben & Trim
    uid = String(uid).trim().toUpperCase();

    // Format: 7 Gruppen Hex (2 Zeichen) getrennt durch ":"
    const regex = /^([0-9A-F]{2}:){6}[0-9A-F]{2}$/;
    return regex.test(uid);
}


uidInput.addEventListener('input', () => {
    if (!isValidNFCUID(uidInput.value)) {
        uidInput.classList.add('invalid');
    } else {
        uidInput.classList.remove('invalid');
    }
});


dbDiv.addEventListener('input', e => {
    if (e.target.matches('span.uid')) {
        if (!isValidNFCUID(e.target.textContent)) {
            e.target.classList.add('invalid');
        } else {
            e.target.classList.remove('invalid');
        }
    }
});


// Live-Validierung ohne Cursorverlust
dbDiv.addEventListener('input', e => {
    if (e.target.matches('span.uid')) validateUIDSpan(e.target);
});

// Blur: optional abschließende Validierung + Großschreibung
dbDiv.addEventListener('blur', e => {
    if (e.target.matches('span.uid')) {
        e.target.textContent = e.target.textContent.trim().toUpperCase();
        validateUIDSpan(e.target);
    }
}, true);

// Paste: nach Einfügen prüfen
dbDiv.addEventListener('paste', e => {
    if (e.target.matches('span.uid')) {
        setTimeout(() => validateUIDSpan(e.target), 0);
    }
});

// Optional: nur erlaubte Zeichen
dbDiv.addEventListener('keydown', e => {
    if (e.target.matches('span.uid') && e.key.length === 1) {
        if (!/[0-9a-fA-F:]/.test(e.key)) e.preventDefault();
    }
});





// Nur Klassenzuweisung, Text bleibt unverändert
function validateUIDSpan(span) {

    const uid = span.textContent.trim().toUpperCase();

    if (!isValidNFCUID(uid)) {
        span.classList.add('invalid');
    } else {
        span.classList.remove('invalid');
    }
}





// Eventlistener hinzufügen
ledPinSelect.addEventListener("change", updatePinOptions);
nfcLedPinSelect.addEventListener("change", updatePinOptions);
buttonPinSelect.addEventListener("change", updatePinOptions);
buzzerPinSelect.addEventListener("change", updatePinOptions);
importFileInput.addEventListener("change", updateImportUI);


buttonPinSelect.addEventListener("change", disableButton);
buzzerPinSelect.addEventListener("change", disableBuzzer);

document.getElementById("saveConfig")
    ?.addEventListener("click", saveConfigHandler);










// -------------------- Helpers --------------------






function percentToLedValue(percent) {
    percent = Math.max(0, Math.min(100, percent));
    return Math.round((percent / 100) * 255);
}

function ledValueToPercent(value) {
    value = Math.max(0, Math.min(255, value));
    return Math.round((value / 255) * 100);
}

openSettings.onclick = () =>
    settingsOverlay.classList.add("active");

closeSettings.onclick = () =>
    settingsOverlay.classList.remove("active");







function formatUptime(ms) {
    let seconds = Math.floor(ms / 1000);
    let minutes = Math.floor(seconds / 60);
    let hours = Math.floor(minutes / 60);
    let days = Math.floor(hours / 24);

    seconds = seconds % 60;
    minutes = minutes % 60;
    hours = hours % 24;

    const parts = [];

    if (days > 0) {
        parts.push(`${days} ${days === 1 ? t("txt_day") : t("txt_day_plural")}`);
    }

    if (hours > 0) {
        parts.push(`${hours} ${hours === 1 ? t("txt_hour") : t("txt_hour_plural")}`);
    }

    if (minutes > 0) {
        parts.push(`${minutes} ${minutes === 1 ? t("txt_minute") : t("txt_minute_plural")}`);
    }

    if (seconds > 0) {
        parts.push(`${seconds} ${seconds === 1 ? t("txt_second") : t("txt_second_plural")}`);
    }

    return parts.join(" ");
}


function rssiToColor(rssi) {
    if (rssi >= -60) return "green";      // stark
    else if (rssi >= -75) return "yellow"; // mittel
    else return "red";                     // schwach
}

// Funktion zum Aktualisieren der Anzeige
function updateRssiDisplay(rssi) {
    const rssiSpan = document.getElementById("infoWifiRssi");
    rssiSpan.textContent = rssi + " dBm";
    rssiSpan.style.color = rssiToColor(rssi);
}



async function getVersion() {
    fetch("/api/version")
        .then(r => r.json())
        .then(data => {

            
            //console.log("Version Info:", data);
        
            document.getElementById("fwVersion").textContent = "FW-Version: " + data.firmware;
            //document.getElementById("gitHash").textContent = "Git hash: " + data.git_hash;
            document.getElementById("build_date").textContent = "Build date: " + data.build_date_short;

            BOARD_VARIANT = data.boardVariant; // global verfügbar machen
    
            infoChipName.textContent = data.chipName;
            infoBoardVariant.textContent = BOARD_VARIANT;
            infoCores.textContent = data.cores;
            infoRevision.textContent = data.revision;
            infoFlashSize.textContent = data.flashSize + " bytes";
            infoFwVersion.textContent = data.firmware;
            infoGitHash.textContent = data.git_hash;
            infoBuildDate.textContent = data.build_date;
            infoHostname.textContent = data.hostname;

            infoHeapSize.textContent = data.heap_size + " bytes";
            infoFreeHeap.textContent = data.free_heap + " bytes";
            infoSketchSize.textContent = data.sketch_size + " bytes";
            infoFreeSketch.textContent = data.free_sketch + " bytes";
            infoSpiffsSize.textContent = data.spiffs_size + " bytes";
            infoFreeSpiffs.textContent = data.free_spiffs + " bytes";

            infoWifiSsid.textContent = data.wifi_ssid;
            infoWifiIp.textContent = data.wifi_ip;
            infoWifiGateway.textContent = data.wifi_gateway;
            infoWifiSubnet.textContent = data.wifi_subnet;
            infoWifiMac.textContent = data.wifi_mac;
            infoWifiDns1.textContent = data.wifi_dns1;
            infoWifiDns2.textContent = data.wifi_dns2;
            infoWifiRssi.textContent = data.wifi_rssi + " dBm";
            infoUptime.textContent = formatUptime(data.uptime_ms);

            infoNfcAvailable.textContent = data.nfc_available;
            infoNfcFwVer.textContent = data.nfc_fwVerMajor + "." + data.nfc_fwVerMinor;
            infoNfcChipId.textContent = data.nfc_chipID;

            updateRssiDisplay(data.wifi_rssi);

        })
        .catch(err => console.error("Version fetch failed:", err));
}


document.querySelectorAll(".navItem").forEach(btn => {
    btn.addEventListener("click", () => {

        // Sidebar aktiv markieren
        document.querySelectorAll(".navItem").forEach(b => b.classList.remove("active"));
        btn.classList.add("active");

        // Panels umschalten
        document.querySelectorAll(".settingsPanel").forEach(p => p.classList.remove("active"));
        const target = document.getElementById(btn.dataset.target);
        if (target) target.classList.add("active");

    });
});



function highlightLedIndex(ledIndex) {

    if (!socket || socket.readyState !== WebSocket.OPEN) {
        console.warn("WebSocket nicht verbunden");
        return;
    }

    if (typeof ledIndex !== "number" || ledIndex < 0) {
        console.warn("Ungültiger LED Index:", ledIndex);
        return;
    }

    socket.send(JSON.stringify({
        action: "highlightSingleLed",
        led: ledIndex
    }));
}

ledSelect.addEventListener("change", () => {
    const value = ledSelect.value;      // Wert aus Select
    const ledIndex = parseInt(value);   // in Zahl umwandeln

    if (!isNaN(ledIndex)) {
        highlightLedIndex(ledIndex);    // Funktion aufrufen
    } else {
        console.warn("Ungültiger LED-Index:", value);
    }
});



daynightToggle.addEventListener("change", function () {
    document.body.classList.toggle("daymode", this.checked);
});





async function loadFilaments() {
    const res = await fetch("/filaments.json");

    if (!res.ok) {
        FILAMENT_DATA = [];
        throw new Error("Failed to load filaments");
    }

    FILAMENT_DATA = await res.json();
}


async function checkBoardVariant() {
    await loadHelpAndLang(CONFIGV2.system.defaultLanguage); // sicherstellen, dass i18nData geladen ist

    if (CONFIGV2.boardVariant != BOARD_VARIANT) {
        console.warn(`Board-Variante in config (${CONFIGV2.boardVariant}) stimmt nicht überein mit Firmware (${BOARD_VARIANT})!`);
        alert(
            t("txt_board_variant_mismatch") +
            "\n\nconfig: " + CONFIGV2.boardVariant +
            "\nfirmware: " + BOARD_VARIANT
        );
    }
}






// -------------------- Init --------------------
async function init() {

    await getVersion();
    await loadConfig_V2();
    await loadFilaments();
    checkBoardVariant()
    renderTable();
    updateAddFormSamples();
    updatePinOptions();
    updateImportUI();
    initColorPresets();
    disableButton();
    disableBuzzer();
    
    
}

init();


