import os
from shutil import copyfile
Import("env")

def rename_firmware(*args, **kwargs):
    build_dir = env.subst("$BUILD_DIR")

    # Suche nach allen .bin-Dateien im Build-Ordner
    bin_files = [f for f in os.listdir(build_dir) if f.endswith(".bin")]

    if not bin_files:
        print(f"[WARN] Keine .bin Datei im Build-Ordner gefunden: {build_dir}")
        return

    # Nimm die erste Firmware-Datei, die nicht LittleFS/Partitions ist
    firmware_path = None
    for f in bin_files:
        if "littlefs" not in f.lower() and "partition" not in f.lower():
            firmware_path = os.path.join(build_dir, f)
            break

    if firmware_path is None:
        print(f"[WARN] Keine Haupt-Firmware-Datei gefunden in {bin_files}")
        return

    # Board-Name aus Environment
    board = env['PIOENV']
    if "esp32-s3" in board.lower():
        new_name = os.path.join(build_dir, "firmware_ESP32-S3.bin")
    elif "esp32-ssd1306" in board.lower():
        new_name = os.path.join(build_dir, "firmware_ESP32-ssd1306.bin")
    else:
        new_name = os.path.join(build_dir, "firmware_ESP32.bin")

    copyfile(firmware_path, new_name)
    print(f"[INFO] Firmware erfolgreich umbenannt: {new_name}")

# Hook registrieren: nach allen Build-Schritten
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", rename_firmware)
