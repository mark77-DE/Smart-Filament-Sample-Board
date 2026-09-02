"""
release.py - Kompletter Release-Workflow in einem Schritt:

  1. Git-Tag vX.Y.Z setzen (VOR dem Build, damit git_version.py die neue
     Versionsnummer in die Firmware einbettet)
  2. Alle Firmware-Varianten bauen (App + LittleFS)
  3. firmware.factory.bin (Webinstaller) + firmware.bin (OTA-Updater) je Variante
     sowie littlefs.bin (einmal pro Chip-Familie, geteilt über alle Displays) exportieren
  4. Version + Cache-Busting-Parameter in den Manifesten / index.html aktualisieren,
     sowie version.txt (ESP32-Webserver/version.txt) auf die neue Version setzen
  5. Änderungen committen, Tag + Commit pushen
  6. GitHub Release mit allen Firmware-Dateien als Assets erstellen

Voraussetzung: GitHub CLI (`gh`) installiert und eingeloggt (`gh auth login`).

Nutzung:
    python tools/release.py 0.4.2
    python tools/release.py 0.4.2 --skip-release   (nur bauen/exportieren, kein Tag/Release)
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ENV_MAPPING = {
    "esp32": "esp32-sh1106",
    "esp32-st7789": "esp32-st7789",
    "esp32-s3": "esp32-s3-sh1106",
    "esp32-s3-st7789": "esp32-s3-st7789",
}

# LittleFS ist über alle Display-Varianten einer Chip-Familie identisch (gleiche
# data/-Assets, nur die App unterscheidet sich je Display-Treiber). Deshalb wird
# das Dateisystem nur einmal pro Familie gebaut - über diesen repräsentativen Env.
CHIP_FAMILY_FS_ENV = {
    "ESP32": "esp32",
    "ESP32-S3": "esp32-s3",
}

FILES = [
    "firmware.factory.bin",
    "littlefs.bin",
]

PROJECT_DIR = Path(__file__).resolve().parent.parent          # .../ESP32-Webserver
REPO_ROOT = PROJECT_DIR.parent                                # Repo-Root
BUILD_DIR = PROJECT_DIR / ".pio" / "build"
TARGET_BASE = REPO_ROOT / "docs" / "webinstaller" / "firmware"
MANIFEST_DIR = REPO_ROOT / "docs" / "webinstaller" / "manifests"
INDEX_HTML = REPO_ROOT / "docs" / "webinstaller" / "index.html"

# Einfache Textdatei mit der aktuellen Versionsnummer, liegt direkt unter
# ESP32-Webserver/ (also z. B. .../ESP32-Webserver/version.txt), Inhalt: "v0.4.2\n"
VERSION_FILE = PROJECT_DIR / "version.txt"


def run(cmd, cwd=None):
    print(f"  $ {' '.join(cmd)}")
    try:
        subprocess.run(cmd, check=True, cwd=cwd or REPO_ROOT)
    except FileNotFoundError:
        print(f"\nFEHLER: Befehl '{cmd[0]}' wurde nicht gefunden.")
        if cmd[0] == "gh":
            print("GitHub CLI ist offenbar nicht installiert oder nicht im PATH.")
            print("Installieren: winget install --id GitHub.cli")
            print("Danach neues Terminal öffnen und einmalig: gh auth login")
            print(f"\nAlles vor Schritt 6 lief bereits erfolgreich durch.")
            print(f"Sobald 'gh' funktioniert, nur den Release-Schritt nachholen mit:")
            print(f"  python tools/release.py {sys.argv[1]} --release-only")
        sys.exit(1)


def check_git_clean():
    result = subprocess.run(
        ["git", "status", "--porcelain"], cwd=REPO_ROOT,
        capture_output=True, text=True,
    )
    if result.stdout.strip():
        print("FEHLER: Es gibt uncommittete Änderungen im Repo.")
        print("Bitte erst committen oder stashen, dann erneut versuchen.")
        sys.exit(1)


def create_tag(version):
    print(f"\n=== Schritt 1/6: Git-Tag v{version} setzen ===")
    existing = subprocess.run(
        ["git", "tag", "--list", f"v{version}"], cwd=REPO_ROOT,
        capture_output=True, text=True,
    ).stdout.strip()
    if existing:
        print(f"  Tag v{version} existiert bereits lokal - wird nicht neu gesetzt.")
        return
    run(["git", "tag", f"v{version}"])


def build_all():
    print("\n=== Schritt 2/6: Firmware-Varianten bauen ===")
    for env_name in ENV_MAPPING:
        print(f"\n  -- {env_name} (App) --")
        run(["pio", "run", "-e", env_name], cwd=PROJECT_DIR)

    print("\n  -- LittleFS (einmal pro Chip-Familie) --")
    for family, fs_env in CHIP_FAMILY_FS_ENV.items():
        print(f"\n  -- {fs_env} ({family}) --")
        run(["pio", "run", "-e", fs_env, "-t", "buildfs"], cwd=PROJECT_DIR)


def export_firmware():
    print("\n=== Schritt 3/6: Firmware für Webinstaller exportieren ===")
    TARGET_BASE.mkdir(parents=True, exist_ok=True)

    # App-Binary: eine pro Display-Variante
    #   firmware.factory.bin -> für den Webinstaller (Bootloader+Partitionen+App zusammengeführt)
    #   firmware.bin         -> für den eingebauten OTA-Updater (nur die reine App)
    for env_name, target_name in ENV_MAPPING.items():
        source_dir = BUILD_DIR / env_name
        target_dir = TARGET_BASE / target_name

        if not source_dir.exists():
            print(f"  FEHLER: Build-Verzeichnis fehlt: {source_dir}")
            sys.exit(1)

        target_dir.mkdir(parents=True, exist_ok=True)

        for filename in ("firmware.factory.bin", "firmware.bin"):
            source = source_dir / filename
            if not source.exists():
                print(f"  FEHLER: {filename} fehlt in {source_dir}")
                sys.exit(1)
            shutil.copy2(source, target_dir / filename)
            print(f"  {filename} -> {target_name}/")

    # LittleFS: nur einmal pro Chip-Familie, liegt direkt unter firmware/
    # und wird von allen Display-Varianten dieser Familie gemeinsam referenziert
    for family, fs_env in CHIP_FAMILY_FS_ENV.items():
        source = BUILD_DIR / fs_env / "littlefs.bin"
        if not source.exists():
            print(f"  FEHLER: littlefs.bin fehlt in {BUILD_DIR / fs_env}")
            sys.exit(1)
        target_filename = f"{fs_env}-littlefs.bin"
        shutil.copy2(source, TARGET_BASE / target_filename)
        print(f"  littlefs.bin -> {target_filename}  (geteilt für alle {family}-Varianten)")


def bump_manifests(version):
    print(f"\n=== Schritt 4/6: Manifeste + index.html auf v{version} setzen ===")

    if not MANIFEST_DIR.exists():
        print(f"  WARNUNG: {MANIFEST_DIR} nicht gefunden, überspringe.")
    else:
        for manifest_path in sorted(MANIFEST_DIR.glob("*.json")):
            data = json.loads(manifest_path.read_text(encoding="utf-8"))
            data["version"] = version
            for build in data.get("builds", []):
                for part in build.get("parts", []):
                    # bestehenden ?v=... Parameter ersetzen oder neu anhängen
                    base_path = re.sub(r"\?v=[^&]*$", "", part["path"])
                    part["path"] = f"{base_path}?v={version}"
            manifest_path.write_text(
                json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
            )
            print(f"  {manifest_path.name} -> v{version}")

    if INDEX_HTML.exists():
        html = INDEX_HTML.read_text(encoding="utf-8")
        new_html = re.sub(
            r"ASSET_VERSION = '[^']*'", f"ASSET_VERSION = '{version}'", html
        )
        if new_html != html:
            INDEX_HTML.write_text(new_html, encoding="utf-8")
            print(f"  index.html ASSET_VERSION -> {version}")
        else:
            print("  WARNUNG: ASSET_VERSION-Zeile in index.html nicht gefunden.")

    update_version_file(version)


def update_version_file(version):
    """Schreibt/aktualisiert eine einfache Textdatei mit der aktuellen Version,
    z. B. für externe Tools/Skripte, die per HTTP nur die Versionsnummer abfragen
    wollen (ähnlich version_public.txt im Public-Repo)."""
    VERSION_FILE.write_text(f"v{version}\n", encoding="utf-8")
    rel_path = VERSION_FILE.relative_to(REPO_ROOT).as_posix()
    print(f"  {rel_path} -> v{version}")


def commit_and_push(version):
    print(f"\n=== Schritt 5/6: Commit + Push ===")
    run(["git", "add", "docs/webinstaller"])
    run(["git", "add", VERSION_FILE.relative_to(REPO_ROOT).as_posix()])

    staged = subprocess.run(
        ["git", "diff", "--cached", "--quiet"], cwd=REPO_ROOT
    ).returncode
    if staged == 0:
        print("  Keine Änderungen im webinstaller-Ordner, überspringe Commit.")
    else:
        run(["git", "commit", "-m", f"Release v{version}: Firmware-Export für Webinstaller"])

    run(["git", "push"])
    run(["git", "push", "origin", f"v{version}"])


def create_github_release(version):
    print(f"\n=== Schritt 6/6: GitHub Release v{version} erstellen ===")

    assets = []
    renamed_temp = []

    # App-Binaries: firmware.factory.bin (Webinstaller) + firmware.bin (OTA-Updater)
    # je Display-Variante, umbenannt für eindeutige Asset-Namen
    for env_name, target_name in ENV_MAPPING.items():
        for filename in ("firmware.factory.bin", "firmware.bin"):
            src = TARGET_BASE / target_name / filename
            renamed = TARGET_BASE / f"{target_name}-{filename}"
            shutil.copy2(src, renamed)
            renamed_temp.append(renamed)
            assets.append(str(renamed))

    # LittleFS: nur einmal pro Chip-Familie, Dateiname ist schon eindeutig
    for family, fs_env in CHIP_FAMILY_FS_ENV.items():
        assets.append(str(TARGET_BASE / f"{fs_env}-littlefs.bin"))

    tag = f"v{version}"
    release_exists = subprocess.run(
        ["gh", "release", "view", tag], cwd=REPO_ROOT,
        capture_output=True,
    ).returncode == 0

    try:
        if release_exists:
            print(f"  Release {tag} existiert bereits - Assets werden aktualisiert (überschreibt gleichnamige Dateien).")
            run(["gh", "release", "upload", tag, *assets, "--clobber"])
        else:
            run([
                "gh", "release", "create", tag,
                *assets,
                "--title", tag,
                "--generate-notes",
            ])
    finally:
        for renamed in renamed_temp:
            renamed.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description="Kompletter Firmware-Release-Workflow")
    parser.add_argument("version", help="Neue Versionsnummer, z. B. 0.4.2 (ohne 'v')")
    parser.add_argument("--skip-build", action="store_true", help="Bauen überspringen (nutzt vorhandene .pio/build-Dateien)")
    parser.add_argument("--skip-release", action="store_true", help="Kein Git-Tag-Push, kein GitHub Release - nur lokal bauen/exportieren")
    parser.add_argument("--release-only", action="store_true", help="Nur Schritt 6 (GitHub Release) ausführen - für den Fall, dass Build/Commit/Push schon erfolgreich liefen und nur gh fehlgeschlagen ist")
    args = parser.parse_args()

    version = args.version.lstrip("v")

    print(f"=== Release-Workflow für v{version} ===")

    if args.release_only:
        create_github_release(version)
        print(f"\nFertig! Release v{version} wurde nachträglich erstellt.")
        return

    check_git_clean()
    create_tag(version)

    if not args.skip_build:
        build_all()
    else:
        print("\n--skip-build gesetzt: verwende vorhandene .pio/build-Ausgaben.")

    export_firmware()
    bump_manifests(version)

    if args.skip_release:
        print("\n--skip-release gesetzt: kein Commit/Push/Release.")
        print(f"Lokaler Tag v{version} wurde gesetzt, aber nicht gepusht.")
    else:
        commit_and_push(version)
        create_github_release(version)

    print(f"\nFertig! Release v{version} ist unterwegs.")


if __name__ == "__main__":
    main()