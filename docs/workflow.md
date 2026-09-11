# Dev- & Release-Workflow — Smart Filament Sample Board

## Überblick

Zwei getrennte Pfade:

- **`dev_release.py`** — lokale Testbuilds, OTA Self-Update gegen einen simulierten Testserver, kein Git-Tag, kein GitHub Release.
- **`release.py`** — echtes, öffentliches Release (Git-Tag, GitHub Pre-/Release, Webinstaller-Update).

Versionsnummern werden **nicht** manuell gepflegt, sondern automatisch von `tools/git_version.py` erzeugt (läuft als PlatformIO `pre:`-Script bei jedem Build).

---

## 1. Entwickeln auf `dev` (oder einem Feature-Branch)

Normal weiterarbeiten, committen wie gewohnt.

```bash
git switch dev
# ... Änderungen ...
git add .
git commit -m "..."
```

Feature-Branches (z. B. `display`) zweigen von `dev` ab und funktionieren genauso — `dev_release.py` und `git_version.py` erkennen automatisch, dass es kein `main` ist.

---

## 2. Lokal testen mit `dev_release.py`

```bash
python tools/dev_release.py
# oder mit expliziter Test-Server-IP:
python tools/dev_release.py --server-ip 172.16.0.123
python tools/dev_release.py --server-ip 172.16.0.123 --port 8080
```

Was dabei passiert:

1. **Branch-Check**: bricht ab, wenn der aktuelle Branch `main` ist (Sicherheitsnetz, damit nie versehentlich ein "Test"-Build von `main` aus läuft).
2. **`ota_local_server_generated.h`** wird geschrieben (Testserver-IP/-Port), entweder automatisch erkannt oder per `--server-ip` vorgegeben.
3. **Build** der vier `*_dev`-Envs (`esp32_dev`, `esp32-st7789_dev`, `esp32-s3_dev`, `esp32-s3-st7789_dev`) — diese haben zusätzlich `-D OTA_DEBUG_LOCAL_SERVER` gesetzt.
4. **Kopieren** der vier `firmware.bin`-Dateien nach `tools/local_testserver/v9.9.9/` mit festen Namen:
   - `esp32-sh1106-firmware.bin`
   - `esp32-st7789-firmware.bin`
   - `esp32-s3-sh1106-firmware.bin`
   - `esp32-s3-st7789-firmware.bin`

Danach den simulierten Testserver in diesem Ordner starten (z. B. `python -m http.server 8000` in `tools/local_testserver/v9.9.9/`) und am Gerät den OTA Self-Update testen.

`FIRMWARE_VERSION` in diesem Build lautet automatisch z. B. `v0.4.11-dev.3` (auf `dev`) oder `v0.4.11-dev.3-disp` (auf einem Feature-Branch namens `display`).

---

## 3. Release bauen

```bash
git switch main
git merge dev              # WICHTIG: main auf den aktuellen dev-Stand bringen
python tools/release.py 0.4.12
```

`release.py` übernimmt: Git-Tag setzen, Firmware bauen (normale Envs ohne `OTA_DEBUG_LOCAL_SERVER`, also mit der echten GitHub-URL), Webinstaller-Manifeste aktualisieren, GitHub Release erstellen, pushen.

---

## 4. `dev` wieder auf den aktuellen Stand bringen

**Nicht vergessen — sonst zählt `git_version.py` ab dem alten Tag weiter:**

```bash
git switch dev
git merge main
```

Ohne diesen Schritt bleibt der zuletzt getaggte Release-Commit für `dev` unerreichbar, und `git_version.py` fällt auf den vorletzten Tag zurück (z. B. `v0.4.9-dev.N` statt `v0.4.11-dev.N`).

---

## Versionslogik (`tools/git_version.py`)

Läuft bei **jedem** Build (alle Envs, egal ob `release.py` oder `dev_release.py`).

| Branch                  | `FIRMWARE_VERSION`             |
|--------------------------|---------------------------------|
| `main`                   | exakt der letzte Tag, z. B. `v0.4.11` |
| `dev`                    | `v0.4.11-dev.N` (N = Commits seit dem letzten *echten* Tag) |
| Feature-Branch (z. B. `display`) | `v0.4.11-dev.N-disp` (Branchname auf 4 Zeichen gekürzt) |

Wichtig: Alte `-dev`-Tags aus dem ursprünglichen Workflow (z. B. `v0.4.11-dev`, `v0.4.12-dev`) werden beim Ermitteln des Basis-Tags bewusst ausgeschlossen (`--exclude "*-dev*"`), damit sie die Zählung nicht verfälschen. Sie können bei Gelegenheit aufgeräumt werden, stören aber nicht mehr:

```bash
git tag -d v0.4.11-dev
git push origin :refs/tags/v0.4.11-dev
```

`STABLE_BRANCH` ist in `git_version.py` als `"main"` hinterlegt — falls der Produktions-Branch anders heißt, dort anpassen.

---

## Beteiligte Dateien

| Datei | Zweck | Wird committet? |
|---|---|---|
| `tools/git_version.py` | Generiert `include/version_info.h` bei jedem Build (Version, Git-Hash, Build-Datum) | Ja |
| `tools/dev_release.py` | Lokaler Testbuild + Kopie nach `local_testserver/` | Ja |
| `tools/release.py` | Echtes Release (Tag, Build, GitHub Release, Webinstaller) | Ja |
| `include/version_info.h` | Auto-generiert, nicht von Hand pflegen | Nein (gitignore) |
| `include/ota_local_server_generated.h` | Auto-generiert von `dev_release.py`, enthält lokale Test-Server-URL | Nein (gitignore) |
| `tools/local_testserver/v9.9.9/` | Ablage der Testbuilds für OTA Self-Update Tests | Nein (gitignore empfohlen) |

`.gitignore`-Einträge, falls noch nicht vorhanden:

```
include/version_info.h
include/ota_local_server_generated.h
tools/local_testserver/
```

---

## platformio.ini — relevante Envs

```ini
; Normale Produktions-Envs (nutzt release.py)
[env:esp32]
...
[env:esp32-st7789]
...
[env:esp32-s3]
...
[env:esp32-s3-st7789]
...

; Dev/Test-Envs (nutzt dev_release.py) — je extends + OTA_DEBUG_LOCAL_SERVER
[env:esp32_dev]
extends = env:esp32
build_flags =
    ${env:esp32.build_flags}
    -D OTA_DEBUG_LOCAL_SERVER

[env:esp32-st7789_dev]
extends = env:esp32-st7789
build_flags =
    ${env:esp32-st7789.build_flags}
    -D OTA_DEBUG_LOCAL_SERVER

[env:esp32-s3_dev]
extends = env:esp32-s3
build_flags =
    ${env:esp32-s3.build_flags}
    -D OTA_DEBUG_LOCAL_SERVER

[env:esp32-s3-st7789_dev]
extends = env:esp32-s3-st7789
build_flags =
    ${env:esp32-s3-st7789.build_flags}
    -D OTA_DEBUG_LOCAL_SERVER
```

Alle Envs teilen sich:

```ini
extra_scripts = pre:tools/git_version.py
```

---

## Kurz-Spickzettel

```bash
# --- Entwickeln ---
git switch dev
# ... arbeiten, committen ...
python tools/dev_release.py           # lokal testen (OTA gegen local_testserver)

# --- Releasen ---
git switch main
git merge dev
python tools/release.py 0.4.12

# --- Zurück zu dev ---
git switch dev
git merge main
```
