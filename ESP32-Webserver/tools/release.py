"""
release.py - Kompletter Release-Workflow in einem Schritt:

  1. Git-Tag vX.Y.Z setzen (VOR dem Build, damit git_version.py die neue
     Versionsnummer in die Firmware einbettet)
  2. Alle Firmware-Varianten bauen (App + LittleFS)
  3. firmware.factory.bin + littlefs.bin für den Webinstaller exportieren
  4. Version + Cache-Busting-Parameter in den Manifesten / index.html aktualisieren
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
    "esp32": "esp32-ssd1306",
    "esp32-SH1106": "esp32-sh1106",
    "esp32-st7789": "esp32-st7789",
    "esp32-s3": "esp32-s3-ssd1306",
    "esp32-s3-SH1106": "esp32-s3-sh1106",
    "esp32-s3-st7789": "esp32-s3-st7789",
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
        print(f"\n  -- {env_name} --")
        run(["pio", "run", "-e", env_name], cwd=PROJECT_DIR)
        run(["pio", "run", "-e", env_name, "-t", "buildfs"], cwd=PROJECT_DIR)


def export_firmware():
    print("\n=== Schritt 3/6: Firmware für Webinstaller exportieren ===")
    for env_name, target_name in ENV_MAPPING.items():
        source_dir = BUILD_DIR / env_name
        target_dir = TARGET_BASE / target_name

        if not source_dir.exists():
            print(f"  FEHLER: Build-Verzeichnis fehlt: {source_dir}")
            sys.exit(1)

        target_dir.mkdir(parents=True, exist_ok=True)

        for filename in FILES:
            source = source_dir / filename
            if not source.exists():
                print(f"  FEHLER: {filename} fehlt in {source_dir}")
                sys.exit(1)
            shutil.copy2(source, target_dir / filename)
            print(f"  {filename} -> {target_name}/")


def bump_manifests(version):
    print(f"\n=== Schritt 4/6: Manifeste + index.html auf v{version} setzen ===")

    if not MANIFEST_DIR.exists():
        print(f"  WARNUNG: {MANIFEST_DIR} nicht gefunden, überspringe.")
        return

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


def commit_and_push(version):
    print(f"\n=== Schritt 5/6: Commit + Push ===")
    run(["git", "add", "docs/webinstaller"])

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
    for env_name, target_name in ENV_MAPPING.items():
        for filename in FILES:
            src = TARGET_BASE / target_name / filename
            renamed = TARGET_BASE / f"{target_name}-{filename}"
            shutil.copy2(src, renamed)
            assets.append(str(renamed))

    try:
        run([
            "gh", "release", "create", f"v{version}",
            *assets,
            "--title", f"v{version}",
            "--generate-notes",
        ])
    finally:
        # temporäre, umbenannte Kopien wieder aufräumen
        for env_name, target_name in ENV_MAPPING.items():
            for filename in FILES:
                renamed = TARGET_BASE / f"{target_name}-{filename}"
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