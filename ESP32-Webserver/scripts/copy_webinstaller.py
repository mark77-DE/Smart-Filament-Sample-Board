from pathlib import Path
import shutil


ENV_MAPPING = {
    "esp32": "esp32-ssd1306",
    "esp32-SH1106": "esp32-sh1106",
    "esp32-st7789": "esp32-st7789",
    "esp32-s3": "esp32-s3-ssd1306",
    "esp32-s3-SH1106": "esp32-s3-sh1106",
    "esp32-s3-st7789": "esp32-s3-st7789",
}


PROJECT_DIR = Path(__file__).resolve().parent.parent
BUILD_DIR = PROJECT_DIR / ".pio" / "build"
TARGET_BASE = PROJECT_DIR.parent / "docs" / "webinstaller" / "firmware"

FILES = [
    "firmware.factory.bin",
    "littlefs.bin",
]


def main():
    print("=== Webinstaller Firmware Export ===")

    for env_name, target_name in ENV_MAPPING.items():
        source_dir = BUILD_DIR / env_name
        target_dir = TARGET_BASE / target_name

        print(f"\n[{env_name}]")

        if not source_dir.exists():
            print(f"  FEHLER: Build-Verzeichnis nicht gefunden:")
            print(f"         {source_dir}")
            continue

        target_dir.mkdir(parents=True, exist_ok=True)

        for filename in FILES:
            source = source_dir / filename
            target = target_dir / filename

            if not source.exists():
                print(f"  FEHLER: {filename} nicht gefunden")
                continue

            shutil.copy2(source, target)

            print(f"  {filename} -> {target_name}/")


if __name__ == "__main__":
    main()
