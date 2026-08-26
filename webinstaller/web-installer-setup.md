# Web-Installer einrichten (GitHub Pages)

Diese Anleitung beschreibt, wie der browserbasierte Firmware-Installer
(`webinstaller/index.html`) live geschaltet wird — genau das Prinzip, das auch
WLED oder FilaMan nutzen ([ESP Web Tools](https://esphome.github.io/esp-web-tools/)).

## Funktionsprinzip

Die Seite nutzt die **Web Serial API** des Browsers, um sich direkt per USB mit
dem ESP32 zu verbinden. Ein `<esp-web-install-button>`-Element liest ein
`manifest.json`, das beschreibt, welche Firmware-Dateien es gibt und an welche
Flash-Adressen sie geschrieben werden. Kein Server-Backend nötig — alles läuft
im Browser.

**Voraussetzung:** Die Seite muss über **HTTPS** ausgeliefert werden (Sicherheitsanforderung
von Web Serial). GitHub Pages erfüllt das automatisch.

## 1. Firmware-Binärdateien exportieren

Aus der Arduino IDE:

1. Sketch kompilieren: **Sketch → Kompilieren** (nicht hochladen).
2. **Sketch → Kompilierten Sketch exportieren** — legt die `.bin`-Dateien im Sketch-Ordner ab.
3. Folgende Dateien werden benötigt (Namen können je nach Board-Package leicht abweichen):
   - `<sketchname>.ino.bootloader.bin` → als `bootloader.bin`
   - `<sketchname>.ino.partitions.bin` → als `partitions.bin`
   - `boot_app0.bin` (liegt im ESP32-Board-Package, z. B. unter
     `~/Arduino/hardware/espressif/esp32/tools/partitions/`)
   - `<sketchname>.ino.bin` → als `firmware.bin`
4. Alle vier Dateien in `webinstaller/firmware/` ablegen (Namen exakt wie im
   `manifest.json` erwartet, siehe unten).

**Bei PlatformIO:** liegen die Dateien nach `pio run` unter `.pio/build/<env>/`.

## 2. manifest.json prüfen/anpassen

```json
{
  "name": "Smart Filament Sample Board",
  "version": "1.0.0",
  "builds": [
    {
      "chipFamily": "ESP32",
      "parts": [
        { "path": "firmware/bootloader.bin", "offset": 4096 },
        { "path": "firmware/partitions.bin", "offset": 32768 },
        { "path": "firmware/boot_app0.bin", "offset": 57344 },
        { "path": "firmware/firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
```

- `version` bei jedem Release hochzählen.
- Falls du zusätzlich eine ESP32-S3-Variante unterstützt: einen weiteren
  Eintrag im `builds`-Array mit `"chipFamily": "ESP32-S3"` ergänzen (eigener
  Satz Binärdateien nötig, meist ab Offset `0` statt `4096`).
- `home_assistant_domain` ist optional, hilft aber, wenn das Board per
  ESPHome/Discovery erkannt werden soll.

## 3. GitHub Pages aktivieren

1. Im Repo: **Settings → Pages**.
2. Unter **Build and deployment → Source**: `Deploy from a branch` wählen.
3. **Branch**: `main`, Ordner: `/docs` <!-- TODO: an tatsächliche Ordnerstruktur anpassen, falls webinstaller nicht unter docs/ liegt -->.
4. Speichern — nach ein bis zwei Minuten ist die Seite erreichbar unter:

   ```
   https://mark77-de.github.io/Smart-Filament-Sample-Board/webinstaller/
   ```

5. Falls die Markdown-Dateien in `docs/` dabei ungewollt als Jekyll-Seite
   gerendert werden: eine leere Datei `docs/.nojekyll` anlegen, dann liefert
   GitHub Pages alle Dateien unverändert aus.

## 4. Testen

1. Installer-URL in Chrome, Edge oder Firefox öffnen (nicht Safari/iOS).
2. ESP32 per USB-**Datenkabel** anschließen.
3. „Verbinden & installieren" klicken, Port bestätigen.
4. Flash-Vorgang abwarten — das Board startet danach neu.

## 5. Bei jedem neuen Firmware-Release

1. Neue `.bin`-Dateien exportieren (Schritt 1).
2. Alte Dateien in `webinstaller/firmware/` überschreiben.
3. `version` in `manifest.json` hochzählen.
4. Änderungen committen und pushen — GitHub Pages aktualisiert sich automatisch.

<!-- TODO: Optional, wenn das nervt: einen GitHub-Actions-Workflow einrichten,
der bei jedem Tag/Release automatisch kompiliert, die .bin-Dateien exportiert
und das Manifest aktualisiert. Sag Bescheid, wenn du dabei Hilfe möchtest. -->

## Troubleshooting

| Problem | Lösung |
|---|---|
| Button zeigt „Browser wird nicht unterstützt" | Chrome, Edge oder Firefox auf dem Desktop verwenden, nicht Safari/iOS |
| Verbindung bricht ab / Bootloop nach dem Flashen | Bei ESP-IDF v4+/neueren Arduino-Cores ggf. Binärdateien vorher mit `esptool.py merge_bin` zusammenführen |
| 404 beim Laden von `manifest.json` | Pfade in `manifest.json` sind relativ zur `index.html` — Ordnerstruktur prüfen |
| CORS-Fehler | Nur relevant, falls Manifest/Firmware auf einem anderen Host liegen als die Seite selbst |
