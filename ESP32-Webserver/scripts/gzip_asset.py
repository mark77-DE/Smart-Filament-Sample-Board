# scripts/gzip_assets.py
# Automatically gzip LittleFS assets before buildfs/uploadfs

Import("env")
import os, gzip, shutil

DATA_DIR = env.subst("$PROJECTDATA_DIR")  # normalerweise "<projekt>/data"
EXTS = {".html", ".css", ".js", ".json", ".svg"}  # hier festlegen, was gepackt wird

def needs_update(src, dst):
    # .gz neu bauen, wenn es fehlt oder Quelle neuer ist
    return (not os.path.exists(dst)) or (os.path.getmtime(src) > os.path.getmtime(dst))

def gzip_file(src, dst):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(src, "rb") as f_in, gzip.open(dst, "wb", compresslevel=9) as f_out:
        shutil.copyfileobj(f_in, f_out)

def run_gzip_all(*args, **kwargs):
    if not os.path.isdir(DATA_DIR):
        print("[gzip] data directory not found - skipping.")
        return
    count = 0
    for root, _, files in os.walk(DATA_DIR):
        for name in files:
            _, ext = os.path.splitext(name)
            if ext.lower() in EXTS:
                src = os.path.join(root, name)
                dst = src + ".gz"
                if needs_update(src, dst):
                    gzip_file(src, dst)
                    count += 1
                    rels = os.path.relpath(src, DATA_DIR)
                    print(f"[gzip] {rels} -> {rels}.gz")
    if count == 0:
        print("[gzip] nichts zu tun – alle .gz aktuell.")

# Run before creating/uploading the LittleFS image
env.AddPreAction("buildfs", run_gzip_all)
env.AddPreAction("uploadfs", run_gzip_all)
