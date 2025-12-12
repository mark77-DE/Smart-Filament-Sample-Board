// -------------------- Globale Referenzen --------------------
const dbDiv = document.getElementById("db");
const addForm = document.getElementById("addForm");
const wsStatus = document.getElementById("wsStatus");
const editToggle = document.getElementById("editToggle");
let EDIT_MODE = false;
let CONFIG = null;
let lastHighlightedRow = null;


let LAST_CONFIG_JSON = null; // speichert die letzte geladene Config

// -------------------- WebSocket --------------------
const ws = new WebSocket(`ws://${location.host}/ws`);

ws.onopen = () => updateWSStatus(true);
ws.onclose = () => { updateWSStatus(false); setTimeout(() => location.reload(), 3000); };
ws.onerror = () => updateWSStatus(false);
ws.onmessage = handleWSMessage;

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
        console.error("Fehler beim Parsen der WS-Daten:", ev.data, err);
        return; 
    }

    if(!data.uid) {
        //console.log("Keine UID im empfangenen Datenobjekt:", data);
        return;
    }

    // UID nur Hex-Ziffern, Großschreibung vereinheitlicht
    const scannedUID = data.uid.replace(/[^a-fA-F0-9]/g,'').toUpperCase();
    //console.log("Scanned UID:", scannedUID);

    const rows = document.querySelectorAll("#db .tableRow");
    let highlighted = false;

    rows.forEach((row, idx) => {
        const uidCell = row.querySelector(".uid");
        if (!uidCell) {
            //console.log("Keine UID-Zelle in Zeile", idx);
            return;
        }

        const rowUID = uidCell.textContent.replace(/[^a-fA-F0-9]/g,'').toUpperCase();
        //console.log(`Zeile ${idx}: rowUID=${rowUID} | scannedUID=${scannedUID}`);

        if (rowUID === scannedUID) {
            //console.log(`Zeile ${idx} match!`);
            highlighted = true;

            if (lastHighlightedRow && lastHighlightedRow !== row) {
                //console.log("Entferne vorherige Highlight-Markierung von Zeile", lastHighlightedRow.dataset.idx);
                lastHighlightedRow.classList.remove("highlight");
            }

            row.classList.add("highlight");
            lastHighlightedRow = row;

            row.scrollIntoView({ behavior: "smooth", block: "center" });
        }
    });

    if (!highlighted) {
        //console.log("UID unbekannt, Add-Form vorbereiten:", scannedUID);
        document.querySelector('#addForm input[name="uid"]').value = data.uid;
        document.querySelector('#addForm input[name="vendor"]').focus();

        if(lastHighlightedRow) lastHighlightedRow.classList.remove("highlight");
        lastHighlightedRow = null;
    }
}






// -------------------- Export / Import --------------------
document.getElementById("exportAllBtn").addEventListener("click", async ()=>{
    try{
        const res = await fetch("/api/exportAll");
        if(!res.ok) throw new Error("Export fehlgeschlagen");
        const blob = await res.blob();
        const url = URL.createObjectURL(blob);
        const a=document.createElement("a"); a.href=url; a.download="filament_package.json"; document.body.appendChild(a); a.click(); a.remove(); URL.revokeObjectURL(url);
    }catch(err){ alert(err); }
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
    document.body.innerHTML="<h2>ESP wird neu gestartet...</h2><p>Bitte kurz warten.</p>";
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
    if(used){ alert(`LED ${entry.ledIndex} bereits verwendet von UID ${used.uid}`); return; }
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
                <span id="uidHeader">UID</span>
                <span id="vendorHeader">Hersteller</span>
                <span id="typeHeader">Typ</span>
                <span id="colorHeader">Farbe</span>
                <span id="ledHeader">LED</span>
                <span id="actionHeader">Aktion</span>
            </div>
    `;

    //data.forEach((e, idx) => console.log(idx, e.uid));

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
}


function getTypeOptions(selected){ return ["PLA","PLA+","PLA-CF","PETG","PETG-CF","ABS","ASA","TPU","Nylon","Holz"].map(t=>`<option value="${t}" ${t===selected?"selected":""}>${t}</option>`).join(""); }
function buildLedDropdown(currentLED, usedLEDs){ let html=`<select data-field="ledIndex">`; for(let i=0;i<CONFIG.options.ledCount;i++){ if(!usedLEDs.has(i)||i===currentLED) html+=`<option value="${i}" ${i===currentLED?"selected":""}>LED ${i}</option>`;} html+=`</select>`; return html; }

// -------------------- Buttons für Save/Delete --------------------
function activateButtons(){
    document.querySelectorAll(".saveBtn").forEach(btn=>btn.addEventListener("click", async ()=>{
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

    const idx = Number(btn.dataset.idx); // statt UID
    const res = await fetch("/api/delete", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: `index=${idx}`
    });

    if(res.ok){ alert("Eintrag gelöscht!"); await loadTable(); await updateAddFormLEDs(); }
    else alert("Fehler beim Löschen!");
}));
}






async function updateAddFormLEDs(){ const data=await (await fetch("/filaments.json")).json(); const free=[]; for(let i=0;i<CONFIG.options.ledCount;i++){ if(!data.find(e=>Number(e.ledIndex)===i)) free.push(i); } const sel=document.getElementById("ledIndexSelect"); sel.innerHTML=""; free.forEach(v=>{ const opt=document.createElement("option"); opt.value=v; opt.textContent=`LED ${v}`; sel.appendChild(opt); }); }

// -------------------- LED Config --------------------
document.getElementById("saveLedConfig").addEventListener("click", async ()=>{
    const ledCount = Number(document.getElementById("maxLED").value);
    const ledPin = Number(document.getElementById("ledPin").value);
    const ledBrightness = Number(document.getElementById("ledBrightness").value);
    const col = document.getElementById("ledColor").value;
    const ledColor = [parseInt(col.substr(1,2),16),parseInt(col.substr(3,2),16),parseInt(col.substr(5,2),16)];
    const res=await fetch("/api/updateLedConfig",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({ledCount,ledPin,ledBrightness,ledColor})});
    const text=await res.text();
    if(text.includes("REBOOTING")){
        document.body.innerHTML="<h2>Device is rebooting...</h2><p>Please wait...</p>";
        const check=setInterval(async()=>{
            try{ if((await fetch("/config.json",{cache:"no-store"})).ok){ clearInterval(check); location.reload(); } } catch(e){}
        },2000);
    }else alert("Error saving LED config");
});



// -------------------- LED / Config --------------------
async function loadConfig() {
    const res = await fetch("/config.json");
    if(!res.ok) throw new Error("Config konnte nicht geladen werden");
    const json = await res.json();

    const newConfigJSON = JSON.stringify(json);

    let rebootNeeded = false;
    if(LAST_CONFIG_JSON && LAST_CONFIG_JSON !== newConfigJSON){
        rebootNeeded = true; // Config hat sich geändert
    }

    CONFIG = json;
    LAST_CONFIG_JSON = newConfigJSON;

    return rebootNeeded;
}



// -------------------- Init --------------------
async function init() {
    const rebootNeeded = await loadConfig();
    await loadTable();
    await updateAddFormLEDs();

    if(rebootNeeded){
        // Automatischer Reboot
        const res = await fetch("/api/reboot", { method: "POST" }).catch(() => {});
        document.body.innerHTML = "<h2>ESP wird neu gestartet...</h2><p>Bitte kurz warten.</p>";
    }
}




init();
