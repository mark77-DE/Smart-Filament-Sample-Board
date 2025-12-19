// -------------------- Globale Referenzen --------------------
const dbDiv = document.getElementById("db");
const addForm = document.getElementById("addForm");
const wsStatus = document.getElementById("wsStatus");
const editToggle = document.getElementById("editToggle");
const debugToggle = document.getElementById("debugToggle");

const ledPinSelect = document.getElementById("ledPin");
const nfcLedPinSelect = document.getElementById("nfcLedPin");

// Button & Buzzer (neu)

const buttonEnabledDiv      = document.getElementById("buttonEnabled");

const buttonPinSelect       = document.getElementById("buttonPin");
const buttonPullupInput     = document.getElementById("buttonPullup");
const buttonDebounceInput   = document.getElementById("buttonDebounce");
const buttonLongInput       = document.getElementById("buttonLong");
const buttonDoubleInput     = document.getElementById("buttonDouble");
const buttonHoldInput       = document.getElementById("buttonHold");

const buzzerEnabledDiv      = document.getElementById("buzzerEnabled");

const buzzerPinSelect       = document.getElementById("buzzerPin");
const buzzerPassiveInput    = document.getElementById("buzzerPassive");
const buzzerActiveHighInput = document.getElementById("buzzerActiveHigh");
const buzzerFreqInput       = document.getElementById("buzzerFreq");
const buzzerSingleMsInput   = document.getElementById("buzzerSingleMs");
const buzzerDoubleOnMsInput = document.getElementById("buzzerDoubleOnMs");
const buzzerDoubleGapMsInput= document.getElementById("buzzerDoubleGapMs");
const buzzerErrorOnMsInput  = document.getElementById("buzzerErrorOnMs");
const buzzerErrorGapMsInput = document.getElementById("buzzerErrorGapMs");
const buzzerErrorCountInput = document.getElementById("buzzerErrorCount");

const ledBrightnessInput = document.getElementById("ledBrightness");
const nfcLedBrightnessInput = document.getElementById("nfcLedBrightness");

const maxLEDInput = document.getElementById("maxLED");
const nfcMaxLEDInput = document.getElementById("nfcMaxLED");

const ledColorInput = document.getElementById("ledColor");
const ledColorErrorInput = document.getElementById("ledColorError");
const ledColorPulseInput = document.getElementById("ledColorPulse");

const nfcLedColorSuccessInput = document.getElementById("nfcLedColorSuccess");
const nfcLedColorErrorInput = document.getElementById("nfcLedColorError");
const nfcLedColorPulseInput = document.getElementById("nfcLedColorPulse");

const ledTimeoutInput = document.getElementById("ledTimeout");
const nfcLedTimeoutInput = document.getElementById("nfcLedTimeout");

const nfcLedSuccessBlinkEnabledInput = document.getElementById("nfcLedSuccessBlinkEnabled");
const nfcLedSuccessBlinkCountInput   = document.getElementById("nfcLedSuccessBlinkCount");
const nfcLedSuccessBlinkMsInput      = document.getElementById("nfcLedSuccessBlinkMs");

const importFileInput = document.getElementById("importFile");
const importBtn = document.getElementById("importBtn");

const toggleBtn = document.getElementById("toggleSettings");
const section = document.getElementById("sectionSettings");

const uidInput = document.querySelector('input[name="uid"]');


let EDIT_MODE = false;
let CONFIG = null;
let lastHighlightedRow = null;


// -------------------- WebSocket --------------------
const socket = new WebSocket(`ws://${location.host}/ws`);

socket.onopen = () => updateWSStatus(true);
socket.onclose = () => {
    updateWSStatus(false);
    document.body.innerHTML = `
        <h2>ESP Verbindung verloren...</h2>
        <p>Seite wird in 2 Sekunden neu laden.</p>
    `;
    setTimeout(() => location.reload(), 2000);
};
socket.onerror = () => updateWSStatus(false);
socket.onmessage = handleWSMessage;


// -------------------- Farb-Utils --------------------
function hexToRgb(hex) {
    if (!hex || hex[0] !== '#' || hex.length !== 7) return [255, 0, 0]; // Default Rot
    const r = parseInt(hex.substr(1,2), 16);
    const g = parseInt(hex.substr(3,2), 16);
    const b = parseInt(hex.substr(5,2), 16);
    return [r, g, b];
}

function rgbToHex(rgb) {
    if (!Array.isArray(rgb) || rgb.length < 3) return "#ff0000"; // Default Rot
    const r = rgb[0].toString(16).padStart(2,'0');
    const g = rgb[1].toString(16).padStart(2,'0');
    const b = rgb[2].toString(16).padStart(2,'0');
    return `#${r}${g}${b}`;
}

function updateImportBtnVisibility() {
    importBtn.style.display = importFileInput.files.length ? "inline-block" : "none";
}

function updateWSStatus(connected){
    wsStatus.textContent = connected ? "WebSocket: verbunden" : "WebSocket: getrennt";
    wsStatus.classList.toggle("ws-connected", connected);
    wsStatus.classList.toggle("ws-disconnected", !connected);
}

// -------------------- Edit Mode --------------------
editToggle.addEventListener("change", () => {
    EDIT_MODE = editToggle.checked;
    applyEditMode();
});

function applyEditMode(){
    const table = document.querySelector("#table");
    if(!table) return;
    table.querySelectorAll(".tableRow").forEach(row => {
        row.querySelectorAll("span[contenteditable]").forEach(cell => cell.contentEditable = EDIT_MODE);
        row.querySelectorAll("select").forEach(sel => sel.disabled = !EDIT_MODE);
        row.querySelectorAll(".saveBtn, .deleteBtn").forEach(btn => EDIT_MODE ? btn.removeAttribute("hidden") : btn.setAttribute("hidden",""));
    });
}


// -------------------- WebSocket Handler --------------------
async function handleWSMessage(ev){
    let data;
    try { 
        data = JSON.parse(ev.data); 
    } catch (err) { 
        if(CONFIG?.options?.debugMode) {
            console.error("Fehler beim Parsen der WS-Daten:", ev.data, err);
        }
        return; 
    }

    if(!data.uid) {
        return;
    }

    // UID nur Hex-Ziffern, Großschreibung vereinheitlicht
    const scannedUID = data.uid.replace(/[^a-fA-F0-9]/g,'').toUpperCase();

    const rows = document.querySelectorAll("#db .tableRow");
    let highlighted = false;

    rows.forEach((row) => {
        const uidCell = row.querySelector(".uid");
        if (!uidCell) return;

        const rowUID = uidCell.textContent.replace(/[^a-fA-F0-9]/g,'').toUpperCase();
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

        if(lastHighlightedRow) lastHighlightedRow.classList.remove("highlight");
        lastHighlightedRow = null;
    }
}


// -------------------- Export / Import --------------------
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

document.getElementById("importAllForm").addEventListener("submit", async e=>{
    e.preventDefault();
    const fileInput = document.getElementById("importFile");
    if(!fileInput.files.length) return;
    const text = await fileInput.files[0].text();
    try{
        const res = await fetch("/api/importAll",{method:"POST",headers:{"Content-Type":"application/json"},body:text});
        if(res.ok){ alert("Import erfolgreich!"); CONFIG=null; await loadTable(); await updateAddFormLEDs(); }
        else { alert("Import fehlgeschlagen: "+await res.text()); }
    }catch(err){ alert("Import fehlgeschlagen: "+err); }
});
document.getElementById("importFile").addEventListener("change", e=>{
    document.getElementById("uploadLabel").textContent = e.target.files.length ? e.target.files[0].name : "Datei auswählen";
});

// -------------------- Reboot --------------------
document.getElementById("rebootBtn").addEventListener("click", async ()=>{
    if(!confirm("ESP wirklich neustarten?")) return;
    try{ await fetch("/api/reboot",{method:"POST"});} catch {}
    document.body.innerHTML = `
        <h2>ESP Verbindung verloren...</h2>
        <p>Seite wird in 2 Sekunden neu laden.</p>
    `;
    setTimeout(() => location.reload(), 2000);
});


// -------------------- Add Form --------------------
addForm.addEventListener("submit", async e=>{
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
    const used = db.find(e=>Number(e.ledIndex)===entry.ledIndex);
    if(used){ alert(`LED ${entry.ledIndex + 1} bereits verwendet von UID ${used.uid}`); return; }
    const res = await fetch("/api/add",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(entry)});
    if(res.ok){ alert("Eintrag hinzugefügt!"); addForm.reset(); await loadTable(); await updateAddFormLEDs(); }
    else alert("Fehler beim Hinzufügen!");
});


// -------------------- Table / LED --------------------
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
        <div id="table">
            <div id="tableHeader">
                <span id="uidHeader">Tag UID</span>
                <span id="vendorHeader">Name</span>
                <span id="typeHeader">Typ</span>
                <span id="colorHeader">Farbe</span>
                <span id="ledHeader">LED</span>
                <span id="actionHeader">Aktion</span>
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

    activateButtons();
    applyEditMode(); // Buttons / selects im Edit-Modus korrekt setzen

    // ---- UI aus CONFIG füllen ----
    const opts = CONFIG.options || {};

    debugToggle.checked = !!(opts.debugMode);

    // --- Mögliche fehlende Pins in den Dropdowns ergänzen ---
    ensureOption(ledPinSelect,        opts.ledPin);
    ensureOption(nfcLedPinSelect,     opts.nfcLedPin);
    if (buttonPinSelect) ensureOption(buttonPinSelect, opts.buttonPin ?? 32);
    if (buzzerPinSelect) ensureOption(buzzerPinSelect, opts.buzzerPin ?? 33);

    // --- Werte setzen ---
    ledPinSelect.value        = String(opts.ledPin);
    nfcLedPinSelect.value     = String(opts.nfcLedPin);
    if (buttonPinSelect) buttonPinSelect.value = String(opts.buttonPin ?? 32);
    if (buzzerPinSelect) buzzerPinSelect.value = String(opts.buzzerPin ?? 33);

    ledBrightnessInput.value  = ledValueToPercent(opts.ledBrightness ?? 50);
    nfcLedBrightnessInput.value = ledValueToPercent(opts.nfcLedBrightness ?? 100);

    maxLEDInput.value         = opts.ledCount ?? 8;
    nfcMaxLEDInput.value      = opts.nfcLedCount ?? 8;

    ledTimeoutInput.value     = opts.ledTimeout ?? 3000;
    nfcLedTimeoutInput.value  = opts.nfcLedTimeout ?? 4000;

    ledColorInput.value       = rgbToHex(opts.ledColor       ?? [255,0,0]);
    ledColorErrorInput.value  = rgbToHex(opts.ledColorError  ?? [255,0,0]);
    ledColorPulseInput.value  = rgbToHex(opts.ledColorPulse  ?? [0,51,170]);

    nfcLedColorSuccessInput.value = rgbToHex(opts.nfcLedColorSuccess ?? [0,255,0]);
    nfcLedColorErrorInput.value   = rgbToHex(opts.nfcLedColorError   ?? [255,0,0]);
    nfcLedColorPulseInput.value   = rgbToHex(opts.nfcLedColorPulse   ?? [0,51,170]);

    // --- Success Blink UI ---
    nfcLedSuccessBlinkEnabledInput.checked = opts.nfcLedSuccessBlinkEnabled ?? true;
    nfcLedSuccessBlinkCountInput.value     = opts.nfcLedSuccessBlinkCount   ?? 3;
    nfcLedSuccessBlinkMsInput.value        = opts.nfcLedSuccessBlinkMs      ?? 150;

    const syncBlinkUi = () => {
        const en = !!nfcLedSuccessBlinkEnabledInput.checked;
        nfcLedSuccessBlinkCountInput.disabled = !en;
        nfcLedSuccessBlinkMsInput.disabled    = !en;
    };
    nfcLedSuccessBlinkEnabledInput.onchange = syncBlinkUi;
    syncBlinkUi();

    // --- Button UI ---
    if (buttonPullupInput)     buttonPullupInput.checked   = (opts.buttonPullup ?? true);
    if (buttonDebounceInput)   buttonDebounceInput.value   = opts.buttonDebounceMs ?? 30;
    if (buttonLongInput)       buttonLongInput.value       = opts.buttonLongMs     ?? 800;
    if (buttonDoubleInput)     buttonDoubleInput.value     = opts.buttonDoubleMs   ?? 400;
    if (buttonHoldInput)       buttonHoldInput.value       = opts.buttonHoldMs     ?? 250;

    // --- Buzzer UI ---
    if (buzzerPassiveInput)     buzzerPassiveInput.checked    = (opts.buzzerPassive ?? false);
    if (buzzerActiveHighInput)  buzzerActiveHighInput.checked = (opts.buzzerActiveHigh ?? true);
    if (buzzerFreqInput)        buzzerFreqInput.value         = opts.buzzerFreq        ?? 4000;
    if (buzzerSingleMsInput)    buzzerSingleMsInput.value     = opts.buzzerSingleMs    ?? 80;
    if (buzzerDoubleOnMsInput)  buzzerDoubleOnMsInput.value   = opts.buzzerDoubleOnMs  ?? 60;
    if (buzzerDoubleGapMsInput) buzzerDoubleGapMsInput.value  = opts.buzzerDoubleGapMs ?? 80;
    if (buzzerErrorOnMsInput)   buzzerErrorOnMsInput.value    = opts.buzzerErrorOnMs   ?? 50;
    if (buzzerErrorGapMsInput)  buzzerErrorGapMsInput.value   = opts.buzzerErrorGapMs  ?? 60;
    if (buzzerErrorCountInput)  buzzerErrorCountInput.value   = opts.buzzerErrorCount  ?? 3;

    // Nach dem Setzen: Sperrlogik ausführen
    updatePinOptions();
}

function getTypeOptions(selected){
  return ["PLA","PLA+","PLA-CF","PLA-Matte","PETG","PETG-CF","ABS","ASA","TPU","Nylon","Holz"]
    .map(t=>`<option value="${t}" ${t===selected?"selected":""}>${t}</option>`).join("");
}

function buildLedDropdown(currentLED, usedLEDs, disabled=false) {
    let html = `<select data-field="ledIndex" ${disabled ? "disabled" : ""}>`;
    for (let i = 0; i < CONFIG.options.ledCount; i++) {
        if (!usedLEDs.has(i) || i === currentLED) {
            html += `<option value="${i}" ${i === currentLED ? "selected" : ""}>LED ${i+1}</option>`;
        }
    }
    html += `</select>`;
    return html;
}


// -------------------- Buttons für Save/Delete --------------------
function activateButtons(){
    document.querySelectorAll(".saveBtn").forEach(btn=>btn.addEventListener("click", async ()=>{
        if(!confirm("Eintrag sichern?")) return;
        const idx=Number(btn.dataset.idx); const row=btn.closest(".tableRow");
        const entry={idx};
        row.querySelectorAll("[data-field]").forEach(el=>{
            const field=el.dataset.field;
            entry[field]=el.tagName==="SELECT"?el.value:el.innerText.trim();
        });
        const res=await fetch("/api/update",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(entry)});
        if(res.ok){ alert("Eintrag gespeichert!"); await loadTable(); await updateAddFormLEDs(); } else alert("Fehler beim Speichern!");
    }));

    document.querySelectorAll(".deleteBtn").forEach(btn => btn.addEventListener("click", async () => {
        if(!confirm("Eintrag wirklich löschen?")) return;

        const uid = btn.dataset.uid;
        const res = await fetch("/api/delete", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: `uid=${encodeURIComponent(uid)}`
        });

        if(res.ok){ alert("Eintrag gelöscht!"); await loadTable(); await updateAddFormLEDs(); }
        else alert("Fehler beim Löschen!");
    }));
}

async function updateAddFormLEDs(){ 
    const data=await (await fetch("/filaments.json")).json(); 
    const free=[]; 
    for(let i=0;i<CONFIG.options.ledCount;i++){
        if(!data.find(e=>Number(e.ledIndex)===i)) free.push(i);
    }
    const sel=document.getElementById("ledIndexSelect"); 
    sel.innerHTML=""; 
    free.forEach(v=>{
        const opt=document.createElement("option"); 
        opt.value = v; 
        opt.textContent = `LED ${v+1}`; 
        sel.appendChild(opt);
    }); 
}


// -------------------- LED Config (Speichern) --------------------
document.getElementById("saveConfig").addEventListener("click", async () => {
    if(!confirm("Konfiguration sichern?")) return;

    // --- Filament LED ---
    const ledCount       = Number(document.getElementById("maxLED").value);
    const ledPin         = Number(document.getElementById("ledPin").value);
    const ledBrightness  = percentToLedValue(Number(document.getElementById("ledBrightness").value));
    const ledColor       = hexToRgb(document.getElementById("ledColor").value);
    const ledColorError  = hexToRgb(document.getElementById("ledColorError").value);
    const ledColorPulse  = hexToRgb(document.getElementById("ledColorPulse").value);
    const ledTimeout     = Number(document.getElementById("ledTimeout").value);

    const debugMode      = debugToggle.checked;

    // --- NFC LED ---
    const nfcLedCount       = Number(document.getElementById("nfcMaxLED").value);
    const nfcLedPin         = Number(document.getElementById("nfcLedPin").value);
    const nfcLedBrightness  = percentToLedValue(Number(document.getElementById("nfcLedBrightness").value));
    const nfcLedColorSuccess= hexToRgb(document.getElementById("nfcLedColorSuccess").value);
    const nfcLedColorError  = hexToRgb(document.getElementById("nfcLedColorError").value);
    const nfcLedTimeout     = Number(document.getElementById("nfcLedTimeout").value);
    const nfcLedColorPulse  = hexToRgb(document.getElementById("nfcLedColorPulse").value);
    const nfcLedSuccessBlinkEnabled = document.getElementById("nfcLedSuccessBlinkEnabled").checked;
    const nfcLedSuccessBlinkCount   = Number(document.getElementById("nfcLedSuccessBlinkCount").value);
    const nfcLedSuccessBlinkMs      = Number(document.getElementById("nfcLedSuccessBlinkMs").value);

    // --- Button ---
    const buttonPin       = buttonPinSelect ? Number(buttonPinSelect.value) : -1;
    const buttonPullup    = buttonPullupInput ? !!buttonPullupInput.checked : true;
    const buttonDebounceMs= buttonDebounceInput ? Number(buttonDebounceInput.value) : 30;
    const buttonLongMs    = buttonLongInput ? Number(buttonLongInput.value) : 800;
    const buttonDoubleMs  = buttonDoubleInput ? Number(buttonDoubleInput.value) : 400;
    const buttonHoldMs    = buttonHoldInput ? Number(buttonHoldInput.value) : 250;

    // --- Buzzer ---
    const buzzerPin         = buzzerPinSelect ? Number(buzzerPinSelect.value) : -1;
    const buzzerPassive     = buzzerPassiveInput ? !!buzzerPassiveInput.checked : false;
    const buzzerActiveHigh  = buzzerActiveHighInput ? !!buzzerActiveHighInput.checked : true;
    const buzzerFreq        = buzzerFreqInput ? Number(buzzerFreqInput.value) : 4000;
    const buzzerSingleMs    = buzzerSingleMsInput ? Number(buzzerSingleMsInput.value) : 80;
    const buzzerDoubleOnMs  = buzzerDoubleOnMsInput ? Number(buzzerDoubleOnMsInput.value) : 60;
    const buzzerDoubleGapMs = buzzerDoubleGapMsInput ? Number(buzzerDoubleGapMsInput.value) : 80;
    const buzzerErrorOnMs   = buzzerErrorOnMsInput ? Number(buzzerErrorOnMsInput.value) : 50;
    const buzzerErrorGapMs  = buzzerErrorGapMsInput ? Number(buzzerErrorGapMsInput.value) : 60;
    const buzzerErrorCount  = buzzerErrorCountInput ? Number(buzzerErrorCountInput.value) : 3;

    if(CONFIG?.options?.debugMode) {
        console.log("Neue Config:", {
            ledCount, ledPin, ledBrightness, ledColor, ledTimeout, ledColorError, ledColorPulse,
            nfcLedCount, nfcLedPin, nfcLedBrightness, nfcLedColorSuccess, nfcLedColorError, nfcLedTimeout, nfcLedColorPulse,
            nfcLedSuccessBlinkEnabled, nfcLedSuccessBlinkCount, nfcLedSuccessBlinkMs,
            buttonPin, buttonPullup, buttonDebounceMs, buttonLongMs, buttonDoubleMs, buttonHoldMs,
            buzzerPin, buzzerPassive, buzzerActiveHigh, buzzerFreq,
            buzzerSingleMs, buzzerDoubleOnMs, buzzerDoubleGapMs, buzzerErrorOnMs, buzzerErrorGapMs, buzzerErrorCount,
            debugMode
        });
    }

    try {
        await fetch("/api/updateConfig", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
                options: {
                    // LED
                    ledCount,
                    ledPin,
                    ledBrightness,
                    ledColor,
                    ledColorError,
                    ledColorPulse,
                    ledTimeout,

                    // NFC
                    nfcLedCount,
                    nfcLedPin,
                    nfcLedBrightness,
                    nfcLedColorSuccess,
                    nfcLedColorError,
                    nfcLedColorPulse,
                    nfcLedTimeout,

                    nfcLedSuccessBlinkEnabled,
                    nfcLedSuccessBlinkCount,
                    nfcLedSuccessBlinkMs,

                    // Button
                    buttonPin,
                    buttonPullup,
                    buttonDebounceMs,
                    buttonLongMs,
                    buttonDoubleMs,
                    buttonHoldMs,

                    // Buzzer
                    buzzerPin,
                    buzzerPassive,
                    buzzerActiveHigh,
                    buzzerFreq,
                    buzzerSingleMs,
                    buzzerDoubleOnMs,
                    buzzerDoubleGapMs,
                    buzzerErrorOnMs,
                    buzzerErrorGapMs,
                    buzzerErrorCount,

                    // misc
                    debugMode
                }
            })
        });
    } catch(e){
        // ESP schon offline – kein Problem
    }
});


// -------------------- Config laden --------------------
async function loadConfig() {
    const res = await fetch("/config.json");
    if(!res.ok) throw new Error("Config konnte nicht geladen werden");
    const json = await res.json();
    CONFIG = json;
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

// Eventlistener hinzufügen
ledPinSelect.addEventListener("change", updatePinOptions);
nfcLedPinSelect.addEventListener("change", updatePinOptions);
if (buttonPinSelect) buttonPinSelect.addEventListener("change", updatePinOptions);
if (buzzerPinSelect) buzzerPinSelect.addEventListener("change", updatePinOptions);
importFileInput.addEventListener("change", updateImportBtnVisibility);


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

    console.log("Pin: " + pin);

    if (pin === -1) {
        buttonEnabledDiv.classList.add("disabled");
    } else {
        buttonEnabledDiv.classList.remove("disabled");
    }
}

function disableBuzzer() {
    const pin = parseInt(buzzerPinSelect.value, 10);

    console.log("Pin: " + pin);

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



//----------------- Event Listeners ---------------------



buttonPinSelect.addEventListener("change", disableButton);
buzzerPinSelect.addEventListener("change", disableBuzzer);











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


// -------------------- Init --------------------
async function init() {
    await loadConfig();
    await loadTable();
    await updateAddFormLEDs();
    updatePinOptions();
    updateImportBtnVisibility();
    initColorPresets();
    disableButton();
    disableBuzzer();
}

init();
