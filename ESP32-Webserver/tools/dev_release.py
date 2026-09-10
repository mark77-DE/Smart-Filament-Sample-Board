"""
dev_release.py - Separater Development-Release-Workflow.

Sicherheitsregeln:
  - Darf ausschließlich auf dem Branch "dev" ausgeführt werden.
  - Verwendet eigene Dev-Pfade unter docs/webinstaller-dev/.
  - Erstellt GitHub Releases als Pre-Release.
  - Stable release.py bleibt vollständig unberührt.

Nutzung:
    python tools/dev_release.py 0.4.10

Optional:
    python tools/dev_release.py 0.4.10 --skip-build
    python tools/dev_release.py 0.4.10 --skip-release
    python tools/dev_release.py 0.4.10 --release-only

Aus 0.4.10 wird automatisch:
    Git-Tag:      v0.4.10-dev
    Release:      v0.4.10-dev (GitHub Pre-Release)

Hinweis:
  Ein Dev-Release pro Basisversion. Für das nächste Dev-Release die
  Basisversion erhöhen, z. B. 0.4.11.
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

CHIP_FAMILY_FS_ENV = {
    "ESP32": "esp32",
    "ESP32-S3": "esp32-s3",
}

PROJECT_DIR = Path(__file__).resolve().parent.parent
REPO_ROOT = PROJECT_DIR.parent
BUILD_DIR = PROJECT_DIR / ".pio" / "build"

# Development files are kept completely separate from the stable webinstaller.
DEV_WEBINSTALLER = REPO_ROOT / "docs" / "webinstaller-dev"
TARGET_BASE = DEV_WEBINSTALLER / "firmware"
MANIFEST_DIR = DEV_WEBINSTALLER / "manifests"
INDEX_HTML = DEV_WEBINSTALLER / "index.html"

# Keep the stable version.txt untouched. This file is only used by the dev
# webinstaller / dev workflow.
DEV_VERSION_FILE = PROJECT_DIR / "version-dev.txt"


def run(cmd, cwd=None):
    """Run a command and abort on failure."""
    print(f"  $ {' '.join(cmd)}")
    try:
        subprocess.run(cmd, check=True, cwd=cwd or REPO_ROOT)
    except FileNotFoundError:
        print(f"\nFEHLER: Befehl '{cmd[0]}' wurde nicht gefunden.")
        if cmd[0] == "gh":
            print("GitHub CLI ist offenbar nicht installiert oder nicht im PATH.")
            print("Installieren: winget install --id GitHub.cli")
            print("Danach neues Terminal öffnen und einmalig: gh auth login")
        sys.exit(1)


def git_branch():
    """Return the currently checked-out branch."""
    result = subprocess.run(
        ["git", "branch", "--show-current"],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout.strip()


def check_dev_branch():
    """Safety check: this script may only run on dev."""
    branch = git_branch()
    print(f"  Aktueller Git-Branch: {branch or '<detached HEAD>'}")

    if branch != "dev":
        print("\nFEHLER: dev_release.py darf ausschließlich auf dem Branch 'dev' laufen.")
        print("Bitte zuerst wechseln mit:")
        print("  git switch dev")
        sys.exit(1)


def check_git_clean():
    """Require a clean working tree before starting."""
    result = subprocess.run(
        ["git", "status", "--porcelain"],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=True,
    )
    if result.stdout.strip():
        print("FEHLER: Es gibt uncommittete Änderungen im Repo.")
        print("Bitte erst committen oder stashen, dann erneut versuchen.")
        print("\nAktueller Status:")
        print(result.stdout)
        sys.exit(1)


def normalize_version(version):
    """Normalize input such as v0.4.10 to 0.4.10."""
    version = version.strip().lstrip("v")
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        print(
            f"FEHLER: Ungültige Basisversion '{version}'. "
            "Erwartet wird z. B. 0.4.10."
        )
        sys.exit(1)
    return version


def dev_version(base_version):
    return f"{base_version}-dev"


def dev_tag(base_version):
    return f"v{dev_version(base_version)}"


def check_tag_available(tag):
    """Do not silently reuse an existing dev tag."""
    result = subprocess.run(
        ["git", "tag", "--list", tag],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=True,
    )
    if result.stdout.strip():
        print(f"FEHLER: Git-Tag {tag} existiert bereits.")
        print("Bitte eine neue Basisversion verwenden.")
        sys.exit(1)


def create_tag(tag):
    print(f"\n=== Schritt 1/6: Git-Tag {tag} setzen ===")
    check_tag_available(tag)
    run(["git", "tag", tag])


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
    print("\n=== Schritt 3/6: Dev-Firmware für Webinstaller exportieren ===")

    TARGET_BASE.mkdir(parents=True, exist_ok=True)

    for env_name, target_name in ENV_MAPPING.items():
        source_dir = BUILD_DIR / env_name
        target_dir = TARGET_BASE / target_name

        if not source_dir.exists():
            print(f"FEHLER: Build-Verzeichnis fehlt: {source_dir}")
            sys.exit(1)

        target_dir.mkdir(parents=True, exist_ok=True)

        for filename in ("firmware.factory.bin", "firmware.bin"):
            source = source_dir / filename

            if not source.exists():
                print(f"FEHLER: {filename} fehlt in {source_dir}")
                sys.exit(1)

            shutil.copy2(source, target_dir / filename)
            print(f"  {filename} -> {target_name}/")

    for family, fs_env in CHIP_FAMILY_FS_ENV.items():
        source = BUILD_DIR / fs_env / "littlefs.bin"

        if not source.exists():
            print(f"FEHLER: littlefs.bin fehlt in {BUILD_DIR / fs_env}")
            sys.exit(1)

        target_filename = f"{fs_env}-littlefs.bin"
        shutil.copy2(source, TARGET_BASE / target_filename)
        print(
            f"  littlefs.bin -> {target_filename} "
            f"(geteilt für alle {family}-Varianten)"
        )


def bump_manifests(version):
    print(f"\n=== Schritt 4/6: Dev-Manifeste + index.html auf {version} setzen ===")

    if not MANIFEST_DIR.exists():
        print(f"  WARNUNG: {MANIFEST_DIR} nicht gefunden, überspringe.")
    else:
        for manifest_path in sorted(MANIFEST_DIR.glob("*.json")):
            data = json.loads(manifest_path.read_text(encoding="utf-8"))
            data["version"] = version

            for build in data.get("builds", []):
                for part in build.get("parts", []):
                    # Replace an existing cache-busting parameter.
                    base_path = re.sub(r"\?v=[^&]*$", "", part["path"])
                    part["path"] = f"{base_path}?v={version}"

            manifest_path.write_text(
                json.dumps(data, indent=2, ensure_ascii=False) + "\n",
                encoding="utf-8",
            )
            print(f"  {manifest_path.name} -> {version}")

    if INDEX_HTML.exists():
        html = INDEX_HTML.read_text(encoding="utf-8")
        new_html = re.sub(
            r"ASSET_VERSION = '[^']*'",
            f"ASSET_VERSION = '{version}'",
            html,
        )

        if new_html != html:
            INDEX_HTML.write_text(new_html, encoding="utf-8")
            print(f"  index.html ASSET_VERSION -> {version}")
        else:
            print("  WARNUNG: ASSET_VERSION-Zeile in Dev-index.html nicht gefunden.")
    else:
        print(f"  INFO: Keine Dev-index.html vorhanden: {INDEX_HTML}")

    DEV_VERSION_FILE.write_text(f"v{version}\n", encoding="utf-8")
    print(f"  {DEV_VERSION_FILE.relative_to(REPO_ROOT).as_posix()} -> v{version}")


def commit_and_push(tag, version):
    print("\n=== Schritt 5/6: Commit + Push ===")

    # Only Dev webinstaller files and the Dev version marker are staged.
    run(["git", "add", "docs/webinstaller-dev"])
    run(["git", "add", DEV_VERSION_FILE.relative_to(REPO_ROOT).as_posix()])

    staged = subprocess.run(
        ["git", "diff", "--cached", "--quiet"],
        cwd=REPO_ROOT,
    ).returncode

    if staged == 0:
        print("  Keine Änderungen im Dev-Webinstaller, überspringe Commit.")
    else:
        run(
            [
                "git",
                "commit",
                "-m",
                f"Dev Release {tag}: Firmware-Export für Webinstaller",
            ]
        )

    # Push the dev branch explicitly. This prevents accidentally pushing
    # another checked-out branch if the workflow is ever changed later.
    run(["git", "push", "origin", "dev"])
    run(["git", "push", "origin", tag])


def create_github_release(tag, version):
    print(f"\n=== Schritt 6/6: GitHub Dev Release {tag} erstellen ===")

    assets = []
    renamed_temp = []

    try:
        # App binaries get unique names for the GitHub Release.
        for env_name, target_name in ENV_MAPPING.items():
            for filename in ("firmware.factory.bin", "firmware.bin"):
                src = TARGET_BASE / target_name / filename

                if not src.exists():
                    print(f"FEHLER: Release-Asset fehlt: {src}")
                    sys.exit(1)

                renamed = TARGET_BASE / f"{target_name}-{filename}"
                shutil.copy2(src, renamed)
                renamed_temp.append(renamed)
                assets.append(str(renamed))

        # LittleFS is shared by the display variants of each chip family.
        for family, fs_env in CHIP_FAMILY_FS_ENV.items():
            asset = TARGET_BASE / f"{fs_env}-littlefs.bin"

            if not asset.exists():
                print(f"FEHLER: Release-Asset fehlt: {asset}")
                sys.exit(1)

            assets.append(str(asset))

        release_exists = subprocess.run(
            ["gh", "release", "view", tag],
            cwd=REPO_ROOT,
            capture_output=True,
        ).returncode == 0

        if release_exists:
            print(
                f"  Release {tag} existiert bereits - "
                "Assets werden aktualisiert."
            )
            run(["gh", "release", "upload", tag, *assets, "--clobber"])
        else:
            # --prerelease is the important distinction from stable release.py.
            run(
                [
                    "gh",
                    "release",
                    "create",
                    tag,
                    *assets,
                    "--title",
                    tag,
                    "--generate-notes",
                    "--prerelease",
                ]
            )

    finally:
        for renamed in renamed_temp:
            renamed.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(
        description="Separater Dev-Firmware-Release-Workflow (nur Branch dev)"
    )
    parser.add_argument(
        "version",
        help="Basisversion, z. B. 0.4.10 (ohne 'v'); daraus wird v0.4.10-dev",
    )
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Bauen überspringen (nutzt vorhandene .pio/build-Dateien)",
    )
    parser.add_argument(
        "--skip-release",
        action="store_true",
        help="Kein Push/kein GitHub Release - nur lokal bauen/exportieren",
    )
    parser.add_argument(
        "--release-only",
        action="store_true",
        help="Nur GitHub Release erstellen; Build/Commit/Push müssen bereits erfolgt sein",
    )

    args = parser.parse_args()
    base_version = normalize_version(args.version)
    version = dev_version(base_version)
    tag = dev_tag(base_version)

    print(f"=== Dev-Release-Workflow für {tag} ===")

    # Safety checks happen before any modification.
    check_dev_branch()

    if args.release_only:
        create_github_release(tag, version)
        print(f"\nFertig! Dev-Release {tag} wurde erstellt.")
        return

    check_git_clean()
    create_tag(tag)

    if not args.skip_build:
        build_all()
    else:
        print("\n--skip-build gesetzt: verwende vorhandene .pio/build-Ausgaben.")

    export_firmware()
    bump_manifests(version)

    if args.skip_release:
        print("\n--skip-release gesetzt: kein Commit/Push/Release.")
        print(f"Lokaler Tag {tag} wurde gesetzt, aber nicht gepusht.")
    else:
        commit_and_push(tag, version)
        create_github_release(tag, version)

    print(f"\nFertig! Dev-Release {tag} ist unterwegs.")


if __name__ == "__main__":
    main()
