# Anleitung: Schema-Änderungen an config_v2.json und filaments.json

Diese Anleitung beschreibt, welche Stellen im Code angepasst werden müssen,
wenn ein neues Feld hinzugefügt, ein bestehendes Feld geändert oder entfernt
werden soll — ohne dass bestehende Nutzer ein manuelles LittleFS-Backup/Restore
durchführen müssen.

## Grundprinzip

Firmware-Updates werden nur noch als `firmware.bin` ausgeliefert (OTA über
`update.html`, nur app0/app1-Partition betroffen). Die LittleFS-Partition
(`config_v2.json`, `filaments.json`) bleibt dabei unangetastet. Schema-Änderungen
werden stattdessen beim **Boot** bzw. beim **Web-Import** durch Migrationscode
im Speicher nachgezogen und danach persistiert.

Ein LittleFS-Image (`littlefs.bin`) wird nur noch für die **Erstinstallation**
oder einen bewussten **Factory Reset** verwendet, nie für reguläre Updates.

---

## Teil A: Änderungen an config_v2.json

### A1. Neues Feld hinzufügen (einfacher Skalarwert: bool/int/String)

Checkliste, in dieser Reihenfolge:

1. **`config.h` — Struct erweitern**
   Neues Feld im passenden Sub-Struct ergänzen (`systemConfig`, `LedConfigV2`,
   `NfcLedConfigV2`, `ButtonConfigV2`, `BuzzerConfigV2`, `MqttConfigV2`).

2. **`config.cpp` → `loadConfigV2()` — Auslesen mit Default**
   ```cpp
   CONFIGV2.system.neuesFeld = sys["neuesFeld"] | <sinnvollerDefault>;
   ```
   Der `|`-Default ist dein Sicherheitsnetz gegen alte Dateien ohne dieses Feld —
   er verhindert einen funktionalen Fehler auch ganz ohne Migration.

3. **`config.cpp` → `saveConfigV2()` — Zurückschreiben**
   ```cpp
   system["neuesFeld"] = CONFIGV2.system.neuesFeld;
   ```

4. **`config.cpp` → `updateConfigFromJsonV2()` — Web-UI-Update übernehmen**
   ```cpp
   CONFIGV2.system.neuesFeld = sys["neuesFeld"] | CONFIGV2.system.neuesFeld;
   ```

5. **`config.cpp` → `importConfigJsonV2()` — Web-Import übernehmen**
   Gleiches Pattern wie Punkt 4, im `src["system"]`-Block.

6. **`config.h` → `migrateConfigV2()` — Migrationsschritt ergänzen**
   Nur nötig, wenn der Default aus Punkt 2 *nicht* harmlos ist (siehe A3).
   Für harmlose Defaults reicht der `|`-Fallback aus Punkt 2 — die Migration
   sorgt dann nur dafür, dass der Default auch dauerhaft in die Datei
   geschrieben wird, statt bei jedem Boot neu aus dem Code zu kommen:
   ```cpp
   if (ver == "<alte Version>") {
       if (cfg["system"]["neuesFeld"].isNull()) {
           cfg["system"]["neuesFeld"] = <sinnvollerDefault>;
       }
       Serial.println(F("[MIGRATION] <alt> -> <neu>"));
       ver = "<neu>";
       changed = true;
   }
   ```
   `CURRENT_VERSION`-Konstante in derselben Funktion entsprechend hochzählen.

### A2. Verschachtelte Struktur ändern (z. B. Array → Objekt, Feld umbenannt)

Hier reicht der `|`-Default **nicht** aus, weil sich die Form des Werts ändert,
nicht nur sein Vorhandensein. Beispiel: `"color": [0,255,0]` wird zu
`"color": {"r":0,"g":255,"b":0}`.

Zusätzlich zu A1, Punkt 6 muss der Migrationsschritt das alte Format aktiv
in das neue überführen, **bevor** der normale Auslese-Code läuft:

```cpp
if (ver == "<alte Version>") {
    if (cfg["led"]["color"].is<JsonArray>()) {
        JsonArray old = cfg["led"]["color"];
        JsonObject neu = cfg["led"]["color"].to<JsonObject>(); // überschreibt das Array
        neu["r"] = old[0];
        neu["g"] = old[1];
        neu["b"] = old[2];
    }
    ver = "<neu>";
    changed = true;
}
```

### A3. Wann ein Default *nicht* harmlos ist

Vorsicht bei Feldern, bei denen `0` / `false` / `""` eine gültige, aber
inhaltlich falsche Bedeutung hätte (z. B. `0` bedeutet in der Anwendungslogik
"deaktiviert"). Hier **muss** ein Migrationsschritt (A1, Punkt 6) den echten
Default setzen, sonst verschwindet bei jedem alten Gerät ohne dieses Feld
stillschweigend eine Funktion — ohne Absturz, aber mit stillem Funktionsverlust.

### A4. Feld entfernen

- Auslese-Zeile in `loadConfigV2()` und `importConfigJsonV2()` entfernen.
- Schreib-Zeile in `saveConfigV2()` entfernen.
- Struct-Feld in `config.h` entfernen.
- Kein Migrationsschritt nötig — überzählige Felder in alten Dateien werden
  beim nächsten `saveConfigV2()` automatisch nicht mehr mitgeschrieben.

---

## Teil B: Änderungen an filaments.json

Die Filament-DB ist ein Array ohne eigenes `version`-Feld. Migrationen werden
daher über `CONFIGV2.system.version` gesteuert (an dieser Stelle bereits durch
`loadConfigV2()` migriert und gesetzt, bevor `loadFilaments()` läuft).

### B1. Neues Feld hinzufügen

1. **`filament_db.h` → `FilamentEntry`-Struct erweitern**
   ```cpp
   String neuesFeld;
   ```

2. **`filament_db.cpp` → `loadFromJsonArray()` — Lesen mit Default**
   ```cpp
   db[dbCount].neuesFeld = o["neuesFeld"] | "";
   ```

3. **`filament_db.cpp` → `toJsonArray()` — Zurückschreiben**
   ```cpp
   o["neuesFeld"] = db[i].neuesFeld;
   ```

4. **`filehandling.h` → `migrateFilamentEntry()` — nur bei nicht-harmlosem Default**
   ```cpp
   if (configVersion == "<alte Version>") {
       if (entry["neuesFeld"].isNull()) {
           entry["neuesFeld"] = <sinnvollerDefault>;
           changed = true;
       }
   }
   ```

### B2. Feld entfernen

Analog zu A4: Zeilen in `loadFromJsonArray()`, `toJsonArray()` und das
Struct-Feld entfernen. Kein Migrationsschritt nötig.

---

## Teil C: Zwei Aufrufpfade — beide müssen migriert werden

Jede Schema-Änderung existiert in **zwei** Pfaden, die unabhängig voneinander
laufen. Ein Fix nur in einem Pfad reicht nicht:

| Pfad | Config | Filamente |
|---|---|---|
| Boot | `loadConfigV2()` | `loadFilaments()` |
| Web-Import | `importConfigJsonV2()` | `importFilamentsJson()` |

**Beispiel aus der Praxis:** `migrateConfigV2()` wurde korrekt im Import-Pfad
aufgerufen, aber der migrierte `version`-Wert wurde dort nicht nach
`CONFIGV2.system.version` übertragen. `saveConfigV2()` liest beim Schreiben
aber aus `CONFIGV2`, nicht aus dem migrierten JSON — dadurch wurde die
Migration beim Speichern wieder überschrieben. Lehre daraus: Bei jeder neuen
Migration prüfen, ob **alle** Stellen, die den Wert lesen, schreiben oder
weiterreichen, den aktuellen (migrierten) Stand tatsächlich verwenden.

---

## Teil D: Checkliste zum Abhaken

Bei jeder neuen Schema-Version:

- [ ] Struct in `config.h` bzw. `filament_db.h` erweitert/angepasst
- [ ] Lesen mit Default in `loadConfigV2()` bzw. `loadFromJsonArray()`
- [ ] Schreiben in `saveConfigV2()` bzw. `toJsonArray()`
- [ ] Web-UI-Update-Pfad (`updateConfigFromJsonV2()`) berücksichtigt
- [ ] Web-Import-Pfad (`importConfigJsonV2()` / `importFilamentsJson()`) berücksichtigt
- [ ] Bei nicht-harmlosem Default: Migrationsschritt in `migrateConfigV2()` /
      `migrateFilamentEntry()` ergänzt
- [ ] `CURRENT_VERSION`-Konstante in `migrateConfigV2()` hochgezählt
- [ ] Mit Testdatei (alte Version, per Web-Import UND per Boot) verifiziert,
      dass die neue Version tatsächlich persistiert wird
- [ ] Alten Migrationsschritt **nicht** nachträglich verändert — neue Stufe
      stattdessen als zusätzlichen Block anhängen
