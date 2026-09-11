# scripts/embed_assets.py
import os, gzip

GZIP_EXTS = {".html", ".css", ".js", ".json"}
RAW_EXTS = {".png", ".ico"}
MIME = {
    ".html": "text/html", ".css": "text/css", ".js": "application/javascript",
    ".json": "application/json", ".png": "image/png", ".ico": "image/x-icon",
}

def sanitize(name):
    return "".join(c if c.isalnum() else "_" for c in name)

def generate_header(www_dir, out_header):
    if not os.path.isdir(www_dir):
        print(f"[embed_assets] {www_dir} not found - skipping.")
        return

    entries = []
    lines = ["#pragma once", "#include <Arduino.h>", ""]

    for name in sorted(os.listdir(www_dir)):
        src = os.path.join(www_dir, name)
        if not os.path.isfile(src):
            continue
        _, ext = os.path.splitext(name)
        ext = ext.lower()
        if ext not in GZIP_EXTS and ext not in RAW_EXTS:
            continue

        with open(src, "rb") as f:
            raw = f.read()

        gzipped = ext in GZIP_EXTS
        payload = gzip.compress(raw, compresslevel=9) if gzipped else raw
        var_name = "asset_" + sanitize(name)

        lines.append(f"const uint8_t {var_name}[] PROGMEM = {{")
        lines.append(",".join(str(b) for b in payload))
        lines.append("};")
        lines.append(f"const size_t {var_name}_len = {len(payload)};")
        lines.append("")

        entries.append((name, var_name, MIME.get(ext, "application/octet-stream"), gzipped))

    lines.append("struct WebAsset { const char* filename; const uint8_t* data; size_t len; const char* mime; bool gzipped; };")
    lines.append("const WebAsset WEB_ASSETS[] = {")
    for name, var_name, mime, gzipped in entries:
        lines.append(f'  {{"{name}", {var_name}, {var_name}_len, "{mime}", {"true" if gzipped else "false"}}},')
    lines.append("};")
    lines.append(f"const size_t WEB_ASSETS_COUNT = {len(entries)};")

    os.makedirs(os.path.dirname(out_header), exist_ok=True)
    with open(out_header, "w") as f:
        f.write("\n".join(lines))
    print(f"[embed_assets] Embedded {len(entries)} assets into {out_header}")


# --- Run immediately when PlatformIO loads this pre-script ---
try:
    Import("env")
    _www_dir = os.path.join(env.subst("$PROJECT_DIR"), "web_assets")
    _out_header = os.path.join(env.subst("$PROJECT_DIR"), "include", "web_assets_generated.h")
    generate_header(_www_dir, _out_header)
except NameError:
    pass  # not running inside PlatformIO/SCons, e.g. manual test call