# Release-Workflow (`tools/release.py`)

Dieses Script übernimmt den kompletten Ablauf von einer neuen Versionsnummer
bis zum fertigen GitHub Release: Git-Tag setzen, alle Firmware-Varianten
bauen, Dateien für den Webinstaller exportieren, Manifeste aktualisieren,
committen/pushen und ein GitHub Release mit allen Firmware-Dateien anlegen.

## Voraussetzungen

- [PlatformIO CLI](https://platformio.org/) (`pio`) im PATH — bei VSCode-Nutzung meist automatisch vorhanden
- [GitHub CLI](https://cli.github.com/) (`gh`) installiert **und** eingeloggt:
  ```powershell
  gh auth login
  ```
- Ein sauberer Git-Status (keine uncommitteten Änderungen) — das Script bricht sonst ab
- Das Script wird aus dem `ESP32-Webserver`-Ordner heraus aufgerufen (dort, wo auch `platformio.ini` liegt)

## Grundnutzung

```powershell
python tools/release.py 0.4.2
```

Das führt **alle sechs Schritte** hintereinander aus:

| Schritt | Was passiert |
|---|---|
| 1. Tag setzen | Legt lokal den Git-Tag `v0.4.2` an — **vor** dem Build, damit `git_version.py` die neue Versionsnummer in die Firmware einbettet |
| 2. Bauen | Baut alle Display-Varianten (App); baut LittleFS **nur einmal pro Chip-Familie** (ESP32, ESP32-S3), da der Dateisystem-Inhalt über alle Displays hinweg identisch ist |
| 3. Exportieren | Kopiert `firmware.factory.bin` + `firmware.bin` je Variante sowie die geteilten `littlefs.bin`-Dateien nach `docs/webinstaller/firmware/` |
| 4. Manifeste aktualisieren | Setzt die Versionsnummer und einen Cache-Busting-Parameter (`?v=...`) in allen `docs/webinstaller/manifests/*.json` sowie `ASSET_VERSION` in `index.html` |
| 5. Commit + Push | Committet die Änderungen in `docs/webinstaller/`, pusht den Commit und den Tag |
| 6. GitHub Release | Erstellt (oder aktualisiert) ein GitHub Release mit allen Firmware-Dateien als Assets |

## Welche Dateien wofür verwendet werden

| Datei | Verwendungszweck |
|---|---|
| `<variante>-firmware.factory.bin` | Für den **Webinstaller** — enthält Bootloader, Partitionstabelle und App in einer Datei, wird bei `0x0` geflasht |
| `<variante>-firmware.bin` | Für den **eingebauten OTA-Updater** im WebIF — reine App-Binärdatei zum manuellen Hochladen |
| `esp32-littlefs.bin` / `esp32-s3-littlefs.bin` | Dateisystem-Image, **einmal pro Chip-Familie**, wird von allen Display-Varianten dieser Familie geteilt (identischer Inhalt) |

Der Webinstaller selbst nutzt ausschließlich `firmware.factory.bin` + die geteilte `littlefs.bin` (siehe `docs/webinstaller/manifests/*.json`). Die einzelnen `firmware.bin`-Dateien sind **nicht** im Webinstaller verlinkt, sondern nur als Release-Asset für den manuellen OTA-Upload gedacht.

## Schalter (Flags)

| Flag | Wirkung |
|---|---|
| *(keiner)* | Kompletter Ablauf: Tag → Bauen → Exportieren → Manifeste → Commit/Push → Release |
| `--skip-build` | Überspringt Schritt 2 (Bauen) und nutzt die vorhandenen Dateien in `.pio/build/`. Sinnvoll, wenn kurz zuvor schon manuell gebaut wurde. |
| `--skip-release` | Führt nur Schritt 1–4 aus (Tag lokal, bauen, exportieren, Manifeste aktualisieren) — **kein** Commit, **kein** Push, **kein** GitHub Release. Zum risikofreien Testen, bevor etwas veröffentlicht wird. |
| `--release-only` | Überspringt Schritt 1–5 komplett und führt **nur** Schritt 6 (GitHub Release) aus. Für den Fall, dass Build/Commit/Push schon erfolgreich liefen und nur die Release-Erstellung fehlgeschlagen ist (z. B. weil `gh` noch nicht eingeloggt war). |

### Beispiele

```powershell
# Alles auf einmal
python tools/release.py 0.4.2

# Nur bauen und exportieren, um das Ergebnis lokal zu prüfen
python tools/release.py 0.4.2 --skip-release

# Build übernehmen, den Rest normal durchlaufen lassen
python tools/release.py 0.4.2 --skip-build

# Release nachträglich erstellen, weil "gh" beim ersten Versuch fehlte
python tools/release.py 0.4.2 --release-only
```

## Verhalten bei bereits existierendem Release

Läuft das Script mit derselben Versionsnummer ein zweites Mal (z. B. nach
einem Fehler oder um Assets zu aktualisieren), prüft Schritt 6 automatisch,
ob unter diesem Tag schon ein GitHub Release existiert:

- **Existiert noch nicht** → `gh release create` legt ein neues Release an.
- **Existiert bereits** → `gh release upload ... --clobber` lädt die Dateien
  hoch und überschreibt gleichnamige, bereits vorhandene Assets. Das Script
  bricht in diesem Fall **nicht** mehr mit einem Fehler ab.

## Typische Fehler und Lösungen

| Fehlermeldung | Ursache | Lösung |
|---|---|---|
| `FEHLER: Es gibt uncommittete Änderungen im Repo.` | Es liegen nicht committete Änderungen vor | Erst committen oder stashen, dann erneut versuchen |
| `FileNotFoundError` bei `gh` | GitHub CLI nicht installiert oder nicht im PATH des verwendeten Terminals | `winget install --id GitHub.cli`, danach neues Terminal öffnen |
| `To get started with GitHub CLI, please run: gh auth login` | `gh` ist installiert, aber nicht angemeldet | Einmalig `gh auth login` ausführen |
| `a release with the same tag name already exists` | Release wurde schon einmal erstellt (z. B. bei einem vorherigen, fehlgeschlagenen Lauf) | Passiert mit der aktuellen Script-Version nicht mehr — Assets werden automatisch per Upload ergänzt/überschrieben |
| `FEHLER: Build-Verzeichnis fehlt` / `FEHLER: firmware.bin fehlt` | Für eine Variante wurde noch nicht gebaut | `--skip-build` weglassen, damit das Script selbst baut, oder manuell `pio run -e <env>` ausführen |

## Wo die Versionsnummer eigentlich herkommt

Die Firmware selbst liest ihre Versionsangabe (`Firmware Version Info: v0.4.2`
im Boot-Log) über das Pre-Build-Script `tools/git_version.py` aus dem
aktuellsten Git-Tag aus. Deshalb setzt `release.py` den Tag **immer vor** dem
Build — wird die Reihenfolge vertauscht, kompiliert die Firmware weiterhin
mit der alten Versionsnummer, auch wenn das GitHub Release schon den neuen
Namen trägt.
