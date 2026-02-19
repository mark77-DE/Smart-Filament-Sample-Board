# tools/git_version.py
import subprocess, os, time, pathlib

ROOT = pathlib.Path(os.getcwd())
HDR  = ROOT / "include" / "version_info.h"
HDR.parent.mkdir(parents=True, exist_ok=True)

def sh(*cmd):
    return subprocess.check_output(cmd, cwd=ROOT).decode("utf-8").strip()

try:
    describe    = sh("git", "describe", "--tags", "--dirty", "--always")
    shortsha    = sh("git", "rev-parse", "--short", "HEAD")
    try:
        latest_tag = sh("git", "describe", "--tags", "--abbrev=0")
    except subprocess.CalledProcessError:
        latest_tag = "v0.0.0"

    #version    = latest_tag if describe == latest_tag else f"{latest_tag}-dev+{shortsha}"
    build_date = time.strftime("%Y-%m-%d %H:%M:%S")
    
    date_short = time.strftime("%d.%m.%y")

    HDR.write_text(
        f'#pragma once\n'
        f'#define FIRMWARE_VERSION "{latest_tag}"\n'
        f'#define GIT_HASH "{shortsha}"\n'
        f'#define BUILD_DATE "{build_date}"\n'
        f'#define BUILD_DATE_SHORT "{date_short}"\n',
        encoding="utf-8"
    )
    print(f"[git_version] version_info.h written: {latest_tag} ({shortsha}) {date_short}")
except Exception as e:
    HDR.write_text(
        '#pragma once\n'
        '#define FIRMWARE_VERSION "v0.0.0-dev"\n'
        '#define GIT_HASH "unknown"\n'
        '#define BUILD_DATE "unknown"\n'
        '#define BUILD_DATE_SHORT "00.00:00"\n',
        encoding="utf-8"
    )
    print(f"[git_version] fallback header written: {e}")
