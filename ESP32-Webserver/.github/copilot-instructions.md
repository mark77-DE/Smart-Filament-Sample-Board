# Copilot Instructions for ESP32-S3 Filament Webserver

## Project Overview
This is an ESP32-S3 embedded system project called "Spot My Filament" - a smart filament identification board that uses NFC tagging to identify 3D printer filament spools and activate corresponding LED indicators via a web UI.

**Architecture Pattern**: Core hardware driver interfaces (NFC, LED, Display) with a centralized web service layer.

## Core Architecture & Data Flow

### Key Components
1. **Hardware Drivers** (encapsulated in `include/`)
   - `ledctrl_filament.h` - NeoPixel LED strip control (static methods)
   - `display.h` - OLED display (Adafruit_SH1106 or ST7789 via LovyanGFX)
   - `nfc.h` - PN532 NFC reader
   - `filament_db.h` - In-memory filament database

2. **Web Service** (`my_webserver.cpp`)
   - AsyncWebServer with REST endpoints
   - WebSocket at `/ws` for real-time UI updates
   - File serving from LittleFS (`/data/*`)

3. **Main Loop** (`main.cpp`)
   - NFC polling in real-time loop (10ms tick)
   - Tag detection → `handleUID()` → activates LED & displays info
   - 3-second LED timeout after tag scan

4. **Filament Database**
   - Loaded from `/filaments.json` in LittleFS
   - In-memory static array (`db[100]`) up to 100 entries
   - Entries: `{uid, vendor, type, color, ledIndex}`

### Critical Data Flow
```
# Copilot Instructions for ESP32-S3 Filament Webserver

Purpose: help an AI coding agent become productive quickly in this repo.

Quick Start (common tasks)
- Build firmware: `pio run`
- Upload LittleFS `/data` contents: `pio run --target uploadfs` (used frequently)
- Flash firmware: `pio run --target upload` (or configured environment)

Key entrypoints & architecture (read these files first)
- `src/main.cpp` — main loop, NFC polling, `handleUID()` (core runtime flow)
- `src/my_webserver.cpp` — AsyncWebServer, REST endpoints and WebSocket `/ws`
- `src/filament_db.cpp` & `include/filament_db.h` — in-memory DB, load/save logic (static 100-entry array)
- `src/ledctrl_filament.cpp` & `include/ledctrl_filament.h` — NeoPixel control APIs
- `src/nfc.cpp` & `include/nfc.h` — PN532 init and UID handling

Critical runtime flow (simplified)
1. NFC scan → `handleUID()` in `main.cpp`
2. `FilamentDB::findByUID()` locates entry (returns ledIndex + meta)
3. `LEDCTRL_FILAMENT::activateLed(ledIndex)` + `MYDISPLAY::show()`
# Copilot Instructions for ESP32-S3 Filament Webserver

Purpose: help an AI coding agent become productive quickly in this repo.

Quick Start (common tasks)
- Build firmware: `pio run`
- Upload LittleFS `/data` contents: `pio run --target uploadfs` (used frequently)
- Flash firmware: `pio run --target upload` (or configured environment)

Key entrypoints & architecture (read these files first)
- `src/main.cpp` — main loop, NFC polling, `handleUID()` (core runtime flow)
- `src/my_webserver.cpp` — AsyncWebServer, REST endpoints and WebSocket `/ws`
- `src/filament_db.cpp` & `include/filament_db.h` — in-memory DB, load/save logic (static 100-entry array)
- `src/ledctrl_filament.cpp` & `include/ledctrl_filament.h` — NeoPixel control APIs
- `src/nfc.cpp` & `include/nfc.h` — PN532 init and UID handling

Critical runtime flow (simplified)
1. NFC scan → `handleUID()` in `main.cpp`
2. `FilamentDB::findByUID()` locates entry (returns ledIndex + meta)
3. `LEDCTRL_FILAMENT::activateLed(ledIndex)` + `MYDISPLAY::show()`
4. WebSocket broadcast from `my_webserver.cpp` to clients

Project-specific patterns & constraints
- Drivers use static classes / static methods (e.g., `LEDCTRL_FILAMENT::init(...)`).
- Database is a simple static array (see `filament_db.cpp` — 100 max entries). Expand struct and array there if needed.
- JSON uses `ArduinoJson` with dynamic allocation; allocate document sizes per endpoint (UID events ~256B, list exports several KB).
- LittleFS data files live in `data/` and are uploaded via `pio run --target uploadfs`.

Initialization order (must be preserved)
1. Serial + Wire (I2C)
2. LittleFS mount
3. OLED init
4. `loadConfig()` (reads `data/config.json`) — sets `LED_COUNT`, `LED_PIN`, etc.
5. `LEDCTRL_FILAMENT::init(LED_COUNT, LED_PIN)`
6. `FilamentDB::load()`
7. NFC SAM config & firmware check (`nfc.cpp`)
8. WiFiManager & AP fallback
9. Start WebSocket & web server

Web API & WebSocket notes
- REST: `GET /filaments.json`, `GET /api/exportAll`, `POST /api/importAll` — implemented in `my_webserver.cpp`.
- WebSocket path: `/ws`. Incoming format `{action:"highlightLED",uid:"..."}`; outgoing includes `{action:"knownUID"|"unknownUID", ledIndex, vendor, type, color}`.

Common edits the agent may perform
- Add a filament field: update `FilamentEntry` struct in `include/filament_db.h`, update parsing in `src/filament_db.cpp`, and update UI (`data/script.js`).
- Change LED count/pin: update `data/config.json` and ensure `loadConfig()` is invoked before `LEDCTRL_FILAMENT::init`.
- Increase DB size: edit the static array max in `src/filament_db.cpp`.

Debugging tips
- Serial logs are primary debug output for firmware behavior.
- To test WebSocket messages from a dev machine: `ws.send(JSON.stringify({action:"highlightLED",uid:"04:..."}))` in browser console.

Files to inspect for policy or dependency constraints
- `platformio.ini` — board, filesystem (`littlefs`), build flags and lib deps
- `data/filaments.json` and `data/config.json` — runtime data uploaded to LittleFS

Do not change: the hardware init order and `LEDCTRL_FILAMENT::init` timing; breaking them causes crashes.

If unclear sections remain, ask which subsystem (NFC, LED, display, web) you want more examples for.
