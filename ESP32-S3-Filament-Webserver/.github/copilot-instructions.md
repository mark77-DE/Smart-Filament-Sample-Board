# Copilot Instructions for ESP32-S3 Filament Webserver

## Project Overview
This is an ESP32-S3 embedded system project called "Spot My Filament" - a smart filament identification board that uses NFC tagging to identify 3D printer filament spools and activate corresponding LED indicators via a web UI.

**Architecture Pattern**: Core hardware driver interfaces (NFC, LED, Display) with a centralized web service layer.

## Core Architecture & Data Flow

### Key Components
1. **Hardware Drivers** (encapsulated in `include/`)
   - `ledctrl.h` - NeoPixel LED strip control (static methods)
   - `display.h` - OLED display (Adafruit_SSD1306)
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
NFC Tag Scan → handleUID() → FilamentDB::findByUID() 
  → activateLed(ledIndex) + MYDISPLAY::show() 
  → WebSocket broadcast to UI
  → LED timeout resets after 3s inactivity
```

## Build & Development Setup

### Build System
- **PlatformIO** with ESP32 dev board configuration
- Key dependencies:
  - `ESPAsyncWebServer` + `AsyncTCP` (web service)
  - `Adafruit_PN532`, `Adafruit_SSD1306`, `Adafruit_GFX` (hardware)
  - `ArduinoJson 7.4.2+` (JSON parsing - see version pin)
  - `WiFiManager 2.0.5` (AP setup)
  - LittleFS filesystem in SPIFFS

- Build flags embed firmware version and git hash at compile time

### Configuration Loading
- `loadConfig()` reads `/config.json` from LittleFS
- Sets globals: `LED_COUNT`, `LED_PIN`, `LED_BRIGHTNESS`, `LED_COLOR`
- Called in setup before hardware init

## Project-Specific Patterns

### Namespace vs Class Conventions
- **Drivers use static class methods**: `LEDCTRL::init()`, `MYDISPLAY::show()` 
- **Database is namespace**: `FilamentDB::loadFromFile()`, `FilamentDB::findByUID()`
- Exception: `NFC` is namespace with pointer injection pattern

### Hardware Initialization Order (Critical)
1. Serial + Wire (I2C)
2. LittleFS mount
3. OLED init
4. `loadConfig()` - sets LED globals
5. `LEDCTRL::init(LED_COUNT, LED_PIN)` - must have counts before init
6. `FilamentDB::load()` 
7. NFC SAM config + firmware check
8. WiFiManager AP connection
9. WebSocket + web server start

Breaking this order causes "not initialized" crashes.

### Global State Management
- `targetLed` (int) - currently active LED index (-1 = none)
- `lastTagTime` (millis) - tracks inactivity for timeout logic
- `activeUID` (String) - last scanned tag
- LED_COUNT, LED_PIN, LED_BRIGHTNESS shared as extern globals

### JSON Handling
- Uses `ArduinoJson` with dynamic sizing
- Document size must accommodate payload + overhead (~256 for UID events, ~4096 for filament lists, ~128KB for exports)
- Serialization: `serializeJson(doc, string)` for compact; `serializeJsonPretty()` for debug

## Web Service Patterns

### REST Endpoints
- `GET /filaments.json` - Returns all filaments as JSON array
- `GET /api/exportAll` - Exports combined config + filaments
- `POST /api/importAll` - Bulk import with validation
- Static files served from LittleFS (`/style.css`, `/script.js`, `/admin.html`)

### WebSocket Protocol
- Path: `/ws` with AsyncWebSocket
- **Incoming Message Format**: `{action: string, uid?: string, ...}`
  - `action: "highlightLED"` triggers LED + display
- **Outgoing**: `{action: "knownUID"|"unknownUID", ledIndex, vendor, type, color}`
- Broadcast to all clients with `ws.textAll(msg)`

### Error Handling Pattern
- File operations check `.exists()` before open
- JSON parse errors logged to Serial with `deserializeJson()` error code
- Hardware not found → display error message + halt

## Common Development Tasks

### Adding a New Filament Entry
1. Edit `/filaments.json` manually or use admin UI
2. Call `FilamentDB::loadFromFile()` to reload
3. UI polls `/filaments.json` to refresh grid

### Extending Hardware Configuration
1. Add field to `FilamentEntry` struct
2. Update JSON parsing in `filament_db.cpp` 
3. Update WebSocket message schema
4. Sync UI in `script.js`

### WebSocket Debugging
- All inbound messages deserialized with error handling
- Enable Serial debug: messages logged before processing
- Test with browser console: `ws.send(JSON.stringify({action: "highlightLED", uid: "04:D3:..."}))` 

### File Management
- Configuration & data live in LittleFS (8MB typical)
- Device uses `littlefs` not SPIFFS (see platformio.ini: `board_build.filesystem = littlefs`)
- Data files in source at `/data/`, copied to LittleFS via PlatformIO upload_fs

## Known Constraints & Gotchas

1. **LED Array Size**: Static 100-entry limit in `filament_db.cpp` - expand if needed
2. **JSON Buffer Sizes**: Auto-sized dynamically but monitor for stack overflow in low-memory scenarios
3. **PN532 Firmware Check**: Crashes if NFC module not detected - always check `getFirmwareVersion()`
4. **WiFi Connection**: Uses AP fallback ("NFC-Setup-AP") if SSID not in memory
5. **OLED Font**: Only FreeMono7pt7b included - add more fonts to `/Fonts/` if needed

## References
- [PlatformIO Docs](https://docs.platformio.org/en/latest/)
- [AsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [ArduinoJson](https://arduinojson.org/) - version 7.4.2 for this project
- [Adafruit PN532](https://github.com/adafruit/Adafruit-PN532)
