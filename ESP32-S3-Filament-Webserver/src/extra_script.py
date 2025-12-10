Import("env")
import subprocess

# Git Commit Hash auslesen
try:
    git_hash = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).decode().strip()
except:
    git_hash = "unknown"

env.Replace(GIT_HASH=git_hash)

# Optionale automatische Version aus Tag oder Commit zählen
try:
    version = subprocess.check_output(["git", "describe", "--tags"]).decode().strip()
except:
    version = "0.0.0"

env.Replace(FIRMWARE_VERSION=version)
