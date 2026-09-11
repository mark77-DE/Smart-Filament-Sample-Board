# tools/git_version.py
import subprocess, os, re, time, pathlib

ROOT = pathlib.Path(os.getcwd())
HDR  = ROOT / "include" / "version_info.h"
HDR.parent.mkdir(parents=True, exist_ok=True)

# Branch that gets a "clean" tag-only version (real releases).
# Adjust if your stable branch is named differently (e.g. "master").
STABLE_BRANCH = "main"


def sh(*cmd):
    return subprocess.check_output(cmd, cwd=ROOT).decode("utf-8").strip()


def branch_suffix(branch, max_len=4):
    """Shorten a branch name to a short, header-safe suffix."""
    cleaned = re.sub(r"[^a-zA-Z0-9]", "", branch).lower()
    return cleaned[:max_len]


try:
    branch = sh("git", "branch", "--show-current") or "detached"
    shortsha = sh("git", "rev-parse", "--short", "HEAD")

    try:
        # Exclude old "-dev" pre-release tags so they never become the
        # base version — otherwise it would double up (v0.4.11-dev-dev.1).
        latest_tag = sh(
            "git", "describe", "--tags", "--abbrev=0",
            "--exclude", "*-dev*",
        )
    except subprocess.CalledProcessError:
        latest_tag = "v0.0.0"

    if branch == STABLE_BRANCH:
        # Stable branch: version is exactly the tag, as before.
        version = latest_tag
    else:
        # Any other branch: count commits since the last tag so the
        # version climbs automatically with every commit — no manual
        # bump, no extra counter file needed.
        try:
            commits_since = sh("git", "rev-list", "--count", f"{latest_tag}..HEAD")
        except subprocess.CalledProcessError:
            commits_since = "0"

        if branch == "dev":
            version = f"{latest_tag}-dev.{commits_since}"
        else:
            version = f"{latest_tag}-dev.{commits_since}-{branch_suffix(branch)}"

    build_date = time.strftime("%Y-%m-%d %H:%M:%S")
    date_short = time.strftime("%d.%m.%y")

    HDR.write_text(
        f'#pragma once\n'
        f'#define FIRMWARE_VERSION "{version}"\n'
        f'#define GIT_HASH "{shortsha}"\n'
        f'#define BUILD_DATE "{build_date}"\n'
        f'#define BUILD_DATE_SHORT "{date_short}"\n',
        encoding="utf-8"
    )
    print(f"[git_version] version_info.h written: {version} ({shortsha}) {date_short}")
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