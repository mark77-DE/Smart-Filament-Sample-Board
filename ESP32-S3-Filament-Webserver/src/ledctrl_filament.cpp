#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>
#include "ledctrl_filament.h"
#include "neopixel_guard.h"

// Debug-Ausgaben einschalten mit -DLED_FIL_DEBUG (build_flags)
#ifdef LED_FIL_DEBUG
  #define FILDBG(...) do { Serial.printf("[FILLED][t=%lu] ", millis()); Serial.printf(__VA_ARGS__); } while (0)
#else
  #define FILDBG(...) do {} while (0)
#endif

// ============================================================================
// Öffentliche Konfig-Variablen (werden von loadLedConfig() überschrieben)
// ============================================================================
int       LED_COUNT       = 0;
int       LED_PIN         = 4;
int       LED_TIMEOUT     = 3000;
int       LED_BRIGHTNESS  = 50;

// Standardfarbe für „normale“ Pixel (z. B. setPixel)
uint32_t  LED_COLOR       = 0x00FF00;   // Beispiel: grün

// Eigenständige Error-Farbe (nur für errorBlink/errorAll)
uint32_t  LED_COLOR_ERROR = 0xFF0000;   // rot

// Farbe für den Idle-Breath-Pulse
uint32_t  LED_COLOR_PULSE = 0x0033AA;   // blau-ish

// ============================================================================
// Private Members der Klasse (Definitionen für die static-Variablen)
// ============================================================================
Adafruit_NeoPixel* LEDCTRL_FILAMENT::_leds     = nullptr;

uint32_t*          LEDCTRL_FILAMENT::_buf      = nullptr;
int                LEDCTRL_FILAMENT::_bufCount = 0;

// Error-Blink-Zustand
bool               LEDCTRL_FILAMENT::_errBlinkActive = false;
unsigned long      LEDCTRL_FILAMENT::_errBlinkStart  = 0;
uint16_t           LEDCTRL_FILAMENT::_errBlinkMs     = 150; // Intervall (ms)
uint8_t            LEDCTRL_FILAMENT::_errBlinkCount  = 3;   // 3x An-Aus
uint8_t            LEDCTRL_FILAMENT::_errBlinkStep   = 0;

// Vor-konvertierte Blinkfarbe (damit das Blinken NICHT vom Buffer abhängt)
static uint32_t    s_errBlinkColorNeo = 0;

bool               LEDCTRL_FILAMENT::_errSolidActive = false;

// Presence/Timeout
bool               LEDCTRL_FILAMENT::_tagHeld         = false;
unsigned long      LEDCTRL_FILAMENT::_lastTagSeen     = 0;
unsigned long      LEDCTRL_FILAMENT::_releaseTs       = 0;
const uint16_t     LEDCTRL_FILAMENT::TAG_HELD_GRACE_MS = 200;

// Reassert (gegen Glitches)
unsigned long      LEDCTRL_FILAMENT::_lastHoldRefresh = 0;
const uint16_t     LEDCTRL_FILAMENT::HOLD_REFRESH_MS  = 25;

// Idle-Pulse
bool               LEDCTRL_FILAMENT::_idlePulseEnabled = true;
float              LEDCTRL_FILAMENT::_minBrightness    = 0.30f;
unsigned long      LEDCTRL_FILAMENT::_lastPulseUpdate  = 0;
const uint16_t     LEDCTRL_FILAMENT::PULSE_INTERVAL_MS = 16;
const uint16_t     LEDCTRL_FILAMENT::BREATHS_PER_MIN   = 15;
uint8_t            LEDCTRL_FILAMENT::_ditherPhase      = 0;

// Idle-Blocker (wirkt nur im Idle)
unsigned long      LEDCTRL_FILAMENT::_idleBlockUntil   = 0;

// ============================================================================
// Kleine Helper
// ============================================================================
static inline uint32_t rgbHexToNeo(Adafruit_NeoPixel* s, uint32_t rgb) {
  const uint8_t r = (rgb >> 16) & 0xFF;
  const uint8_t g = (rgb >>  8) & 0xFF;
  const uint8_t b =  rgb        & 0xFF;
  return s->Color(r, g, b);
}

static uint8_t breath8(uint16_t bpm, uint32_t nowMs, uint8_t low, uint8_t high) {
  const float periodMs = 60000.0f / (float)bpm;
  float phase01 = fmodf((float)nowMs, periodMs) / periodMs;
  float s = (sinf(phase01 * 2.0f * PI) + 1.0f) * 0.5f;
  uint16_t v = (uint16_t)(low + s * (float)(high - low) + 0.5f);
  return (uint8_t)min<uint16_t>(v, 255);
}

static const uint8_t BAYER4[16] = {
  0,8,2,10, 12,4,14,6, 3,11,1,9, 15,7,13,5
};

// ============================================================================
// Private Methoden (Buffer)
// ============================================================================
void LEDCTRL_FILAMENT::ensureBuf(int n) {
  if (_bufCount == n && _buf) return;
  if (_buf) { delete[] _buf; _buf = nullptr; }
  _buf = new uint32_t[n];
  _bufCount = n;
  for (int i = 0; i < n; ++i) _buf[i] = 0;
}

void LEDCTRL_FILAMENT::renderAllFromBuf(Adafruit_NeoPixel* s) {
  if (!s || !_buf) return;
  for (int i = 0; i < _bufCount; ++i) s->setPixelColor(i, _buf[i]);
  neopixelShowSafe(s);
}

bool LEDCTRL_FILAMENT::bufAnyLit() {
  if (!_buf) return false;
  for (int i = 0; i < _bufCount; ++i) {
    if (_buf[i] != 0) return true;
  }
  return false;
}

// ============================================================================
// Idle-Pulse Frame
// ============================================================================
static void renderIdlePulseFrame(Adafruit_NeoPixel* s,
                                 unsigned long now,
                                 uint32_t pulseRgbHex,
                                 float minBrightness,
                                 uint8_t& ditherPhase,
                                 uint16_t breathsPerMin) {
  if (!s) return;
  s->setBrightness(constrain(LED_BRIGHTNESS, 0, 255));

  const uint8_t low8 = (uint8_t)constrain((int)lroundf(minBrightness * 255.0f), 0, 255);
  const uint8_t lvl  = breath8(breathsPerMin, now, low8, 255);
  const uint8_t glvl = Adafruit_NeoPixel::gamma8(lvl);

  const uint8_t r0 = (pulseRgbHex >> 16) & 0xFF;
  const uint8_t g0 = (pulseRgbHex >>  8) & 0xFF;
  const uint8_t b0 =  pulseRgbHex        & 0xFF;

  auto dimDither8 = [](uint8_t base, uint8_t dim, uint8_t thr)->uint8_t {
    const uint32_t v12 = ((uint32_t)base * (uint32_t)dim * 16U + 127U) / 255U;
    uint8_t out = (uint8_t)(v12 >> 4);
    if ((v12 & 0x0F) > thr && out < 255) out++;
    return out;
  };

  const int n = s->numPixels();
  for (int i = 0; i < n; ++i) {
    const uint8_t thr = BAYER4[(ditherPhase + (i & 0x0F)) & 0x0F];
    const uint8_t r = dimDither8(r0, glvl, thr);
    const uint8_t g = dimDither8(g0, glvl, thr);
    const uint8_t b = dimDither8(b0, glvl, thr);
    s->setPixelColor(i, s->Color(r, g, b));
  }
  ditherPhase = (uint8_t)((ditherPhase + 1) & 0x0F);
}

// ============================================================================
// Public API
// ============================================================================
void LEDCTRL_FILAMENT::init(int count, int pin, int timeout_ms, int brightness) {
  LED_COUNT      = max(0, count);
  LED_PIN        = pin;
  LED_TIMEOUT    = max(0, timeout_ms);
  LED_BRIGHTNESS = constrain(brightness, 0, 255);

  // vorhandenen Strip sauber freigeben
  if (_leds) {
    _leds->setBrightness(255);
    _leds->clear();
    neopixelShowSafe(_leds);
    delete _leds;
    _leds = nullptr;
  }

  if (LED_COUNT <= 0) return;

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  _leds = new Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
  _leds->begin();
  _leds->clear();
  _leds->setBrightness(LED_BRIGHTNESS);
  neopixelShowSafe(_leds);

  ensureBuf(LED_COUNT);
  for (int i = 0; i < _bufCount; ++i) _buf[i] = 0;

  // State zurücksetzen
  _errBlinkActive  = false;
  _errSolidActive  = false;
  _tagHeld         = false;
  _lastTagSeen     = 0;
  _releaseTs       = 0;
  _lastHoldRefresh = 0;
  _lastPulseUpdate = millis();
  _ditherPhase     = 0;
  _idleBlockUntil  = 0;

  FILDBG("init: count=%d pin=%d bright=%d timeout=%d\n", LED_COUNT, LED_PIN, LED_BRIGHTNESS, LED_TIMEOUT);
}

void LEDCTRL_FILAMENT::setPixel(int index, uint32_t color) {
  if (!_leds || !_buf) return;
  if (index < 0 || index >= _bufCount) return;

  // Wenn wir AUS einem Error-Zustand kommen → erst alles löschen,
  // damit keine roten Restpixel stehen bleiben.
  const bool wasError = (_errBlinkActive || _errSolidActive);

  // Error-Modi sofort beenden
  _errBlinkActive = false;
  _errSolidActive = false;

  if (wasError) {
    for (int i = 0; i < _bufCount; ++i) _buf[i] = 0;  // Buffer leer
    _leds->clear();                                   // physisch leer
  }

  // Gewünschten Pixel setzen
  _buf[index] = color;
  _leds->setPixelColor(index, color);
  neopixelShowSafe(_leds);

  // Idle kurz blocken (Pulse nicht in Übergangsframe mischen)
  _idleBlockUntil = millis() + 2;
}

void LEDCTRL_FILAMENT::allOff() {
  if (!_leds || !_buf) return;

  for (int i = 0; i < _bufCount; ++i) _buf[i] = 0;
  _leds->clear();
  neopixelShowSafe(_leds);

  _errBlinkActive = false;
  _errSolidActive = false;
  _releaseTs      = 0;

  _lastPulseUpdate = millis() - PULSE_INTERVAL_MS;
  _idleBlockUntil  = millis() + 2;

  FILDBG("allOff()\n");
}

void LEDCTRL_FILAMENT::tagPresenceTick(bool present) {
  const unsigned long now = millis();

  if (present) {
    _lastTagSeen = now;
    if (!_tagHeld) {
      _tagHeld   = true;
      _releaseTs = 0;
      FILDBG("presence: RISING (fil)\n");
    }
  } else {
    if (_tagHeld && (now - _lastTagSeen) > TAG_HELD_GRACE_MS) {
      _tagHeld = false;

      // Timeout nur starten, wenn tatsächlich etwas „aktiv“ ist
      if (_errBlinkActive || _errSolidActive || bufAnyLit()) {
        _releaseTs = now;
        FILDBG("presence: FALLING startTimeout relTs=%lu\n", _releaseTs);
      } else {
        _releaseTs = 0;
        FILDBG("presence: FALLING (no active)\n");
      }
    }
  }
}

// ----------------------------------------------------------------------------
// ERROR SOLID: alle Pixel = LED_COLOR_ERROR
// ----------------------------------------------------------------------------
void LEDCTRL_FILAMENT::errorAll() {
  if (!_leds || !_buf) return;

  _errBlinkActive = false;
  _errSolidActive = true;

  // Alle rot/err füllen (Buffer + Ausgabe)
  const uint32_t neoErr = rgbHexToNeo(_leds, LED_COLOR_ERROR);
  for (int i = 0; i < _bufCount; ++i) _buf[i] = neoErr;
  renderAllFromBuf(_leds);

  // Timeout ab Tag-Entfernung
  _releaseTs = _tagHeld ? 0UL : millis();

  // Idle kurz blocken
  _idleBlockUntil = millis() + 2;
  FILDBG("errorAll (solid)\n");
}

// ----------------------------------------------------------------------------
// ERROR BLINK: erst blinken (LED_COLOR_ERROR), dann – falls noch aktiv – solid-Error
// ----------------------------------------------------------------------------
void LEDCTRL_FILAMENT::errorBlink() {
  if (!_leds) return;

  // Parameter (ggf. später aus Config herausziehbar)
  static const uint16_t MIN_BLINK_MS = 25;
  _errBlinkMs    = (uint16_t)max<int>(MIN_BLINK_MS, 150); // Standard 150 ms
  _errBlinkCount = 3;                                     // 3x An-Aus

  _errBlinkActive = true;
  _errSolidActive = false;
  _errBlinkStart  = millis();
  _errBlinkStep   = 0;

  // Blinkfarbe fest in Neo-Format (unabhängig vom Buffer)
  s_errBlinkColorNeo = rgbHexToNeo(_leds, LED_COLOR_ERROR);

  // Startframe = AN (direkt rendern, ohne Buffer)
  for (int i = 0; i < _leds->numPixels(); ++i) _leds->setPixelColor(i, s_errBlinkColorNeo);
  neopixelShowSafe(_leds);

  // Timeout erst ab Entfernung
  _releaseTs = _tagHeld ? 0UL : millis();

  // Idle blocken
  _idleBlockUntil = millis() + 2;
  FILDBG("errorBlink start ms=%u count=%u\n", _errBlinkMs, _errBlinkCount);
}

void LEDCTRL_FILAMENT::update() {
  if (!_leds) return;
  const unsigned long now = millis();

  // 1) ERROR-BLINK: phasenbasiert; Frames DIREKT rendern (kein Buffer!)
  if (_errBlinkActive) {
    const uint32_t intervals = (uint32_t)((now - _errBlinkStart) / _errBlinkMs); // Halbphasen
    if (intervals != _errBlinkStep) {
      _errBlinkStep = (uint8_t)min<uint32_t>(255U, intervals);
      const bool on = ((intervals & 1U) == 0U); // gerade = AN

      if (on) {
        for (int i = 0; i < _leds->numPixels(); ++i) _leds->setPixelColor(i, s_errBlinkColorNeo);
      } else {
        for (int i = 0; i < _leds->numPixels(); ++i) _leds->setPixelColor(i, 0);
      }
      neopixelShowSafe(_leds);
    }

    // Ende der Blink-Sequenz?
    if (intervals >= (uint32_t)_errBlinkCount * 2U) {
      _errBlinkActive = false;

      // Wenn Timeout noch nicht durch → in ERR-SOLID wechseln und Buffer füllen
      if (_releaseTs == 0 || (now - _releaseTs) < (unsigned long)LED_TIMEOUT) {
        _errSolidActive   = true;
        _lastHoldRefresh  = 0;

        const uint32_t neoErr = rgbHexToNeo(_leds, LED_COLOR_ERROR);
        for (int i = 0; i < _bufCount; ++i) _buf[i] = neoErr;
        renderAllFromBuf(_leds);

        _idleBlockUntil = now + 2;
        FILDBG("errBlink -> errSolid\n");
      } else {
        // Timeout bereits durch → Idle
        allOff();
      }
    }
    // Solange Blinken aktiv ist, kein Idle-Pulse
    return;
  }

  // 2) ERROR-SOLID: halten solange Tag da; sonst nach Timeout aus
  if (_errSolidActive) {
    if (_tagHeld) {
      _releaseTs = 0;
      if (now - _lastHoldRefresh >= HOLD_REFRESH_MS) {
        _lastHoldRefresh = now;
        renderAllFromBuf(_leds);
      }
      return;
    }
    if (_releaseTs != 0 && (now - _releaseTs) >= (unsigned long)LED_TIMEOUT) {
      _errSolidActive = false;
      _releaseTs      = 0;
      allOff(); // schaltet auch Idle wieder ein
    } else {
      if (now - _lastHoldRefresh >= HOLD_REFRESH_MS) {
        _lastHoldRefresh = now;
        renderAllFromBuf(_leds);
      }
    }
    return;
  }

  // 3) Normale Pixel-Anzeige (einzelne LEDs gesetzt) mit Timeout-Gating
  if (bufAnyLit()) {
    if (_tagHeld) {
      _releaseTs = 0;
      if (now - _lastHoldRefresh >= HOLD_REFRESH_MS) {
        _lastHoldRefresh = now;
        renderAllFromBuf(_leds);
      }
      return;
    }
    if (_releaseTs != 0 && (now - _releaseTs) >= (unsigned long)LED_TIMEOUT) {
      allOff(); // löscht Buffer + geht in Idle
    } else {
      if (now - _lastHoldRefresh >= HOLD_REFRESH_MS) {
        _lastHoldRefresh = now;
        renderAllFromBuf(_leds);
      }
    }
    return;
  }

  // 4) IDLE-PULSE (nur wenn nix aktiv + nix leuchtet)
  if (_idlePulseEnabled) {
    if (now < _idleBlockUntil) return;
    if (now - _lastPulseUpdate >= PULSE_INTERVAL_MS) {
      _lastPulseUpdate = now;
      renderIdlePulseFrame(_leds, now, LED_COLOR_PULSE, _minBrightness, _ditherPhase, BREATHS_PER_MIN);
      neopixelShowSafe(_leds);
    }
  }
}

bool LEDCTRL_FILAMENT::isIdle() {
  return (!_errBlinkActive && !_errSolidActive && !bufAnyLit());
}

Adafruit_NeoPixel* LEDCTRL_FILAMENT::rawStrip() {
  return _leds;
}

// ============================================================================
// Config laden
// ============================================================================
void loadLedConfig() {
  // versucht zuerst /config.json, sonst /filament_default.json
  if (!LittleFS.begin()) {
    Serial.println(F("LittleFS.begin() failed!"));
    return;
  }

  String filename = LittleFS.exists("/config.json") ? "/config.json" : "/filament_default.json";
  File f = LittleFS.open(filename, "r");
  if (!f) {
    Serial.println("Failed to open " + filename);
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.println(String("JSON parse failed: ") + err.c_str());
    return;
  }

  LED_COUNT      = doc["options"]["ledCount"]      | LED_COUNT;
  LED_PIN        = doc["options"]["ledPin"]        | LED_PIN;
  LED_BRIGHTNESS = doc["options"]["ledBrightness"] | LED_BRIGHTNESS;
  LED_TIMEOUT    = doc["options"]["ledTimeout"]    | LED_TIMEOUT;

  // LED_COLOR (Standardfarbe für normale Pixel)
  if (doc["options"]["ledColor"].is<JsonArray>()) {
    JsonArray c = doc["options"]["ledColor"];
    if (c.size() == 3) {
      const uint8_t r = (uint8_t)c[0].as<int>();
      const uint8_t g = (uint8_t)c[1].as<int>();
      const uint8_t b = (uint8_t)c[2].as<int>();
      LED_COLOR = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
  }

  // LED_COLOR_ERROR (eigene Fehlerfarbe)
  if (doc["options"]["ledColorError"].is<JsonArray>()) {
    JsonArray e = doc["options"]["ledColorError"];
    if (e.size() == 3) {
      const uint8_t r = (uint8_t)e[0].as<int>();
      const uint8_t g = (uint8_t)e[1].as<int>();
      const uint8_t b = (uint8_t)e[2].as<int>();
      LED_COLOR_ERROR = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
  }

  // LED_COLOR_PULSE (Idle)
  if (doc["options"]["ledColorPulse"].is<JsonArray>()) {
    JsonArray p = doc["options"]["ledColorPulse"];
    if (p.size() == 3) {
      const uint8_t r = (uint8_t)p[0].as<int>();
      const uint8_t g = (uint8_t)p[1].as<int>();
      const uint8_t b = (uint8_t)p[2].as<int>();
      LED_COLOR_PULSE = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
  }

  LEDCTRL_FILAMENT::init(LED_COUNT, LED_PIN, LED_TIMEOUT, LED_BRIGHTNESS);

  Serial.println(F("LED Config loaded"));
  Serial.print(F("  LED_COUNT = "));         Serial.println(LED_COUNT);
  Serial.print(F("  LED_PIN = "));           Serial.println(LED_PIN);
  Serial.print(F("  LED_BRIGHTNESS = "));    Serial.println(LED_BRIGHTNESS);
  Serial.print(F("  LED_TIMEOUT = "));       Serial.println(LED_TIMEOUT);
  Serial.print(F("  LED_COLOR = 0x"));       Serial.println(LED_COLOR, HEX);
  Serial.print(F("  LED_COLOR_ERROR = 0x")); Serial.println(LED_COLOR_ERROR, HEX);
  Serial.print(F("  LED_COLOR_PULSE = 0x")); Serial.println(LED_COLOR_PULSE, HEX);
}
