Import("env")
import subprocess

def git(cmd):
    try:
        return subprocess.check_output(cmd, stderr=subprocess.DEVNULL).decode().strip()
    except:
        return "unknown"

version = git(["git", "describe", "--tags", "--dirty", "--always"])
hash    = git(["git", "rev-parse", "--short", "HEAD"])

env.Append(
    BUILD_FLAGS=[
        f'-DFIRMWARE_VERSION=\\"{version}\\"',
        f'-DGIT_HASH=\\"{hash}\\"'
    ]
)
