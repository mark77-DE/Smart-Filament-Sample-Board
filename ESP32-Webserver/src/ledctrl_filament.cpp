#include "globals.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>
#include "ledctrl_filament.h"
#include "ledctrl_nfc.h"
#include "neopixel_guard.h"
#include "config.h"
#include "mqtt_manager.h"
#include "pins.h"
#include "led_config.h"

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
// int       LED_PIN         = 4;
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
// FIX: Idle-FPS entschärfen (ca. 30 FPS)
const uint16_t     LEDCTRL_FILAMENT::PULSE_INTERVAL_MS = 33;
const uint16_t     LEDCTRL_FILAMENT::BREATHS_PER_MIN   = 15;
uint8_t            LEDCTRL_FILAMENT::_ditherPhase      = 0;

// Idle-Blocker (wirkt nur im Idle)
unsigned long      LEDCTRL_FILAMENT::_idleBlockUntil   = 0;

// FIX: Netzlast-Pause (Idle-Frames aussetzen)
unsigned long      LEDCTRL_FILAMENT::_netPauseUntil    = 0;

// WebIF-Hold: simulierte Präsenz (damit Timeout danach greift)
static unsigned long s_webifHoldUntil = 0;

bool LEDCTRL_FILAMENT::_standby = false;



// ============================================================================
// Kleine Helper
// ============================================================================
// FIX: robustes Doppelt-Senden für kritische Frames (Transitions/Reassert)
static inline void forceShow(Adafruit_NeoPixel* s) {
  if (!s) return;
  neopixelShowSafe(s);
  delayMicroseconds(300);
  neopixelShowSafe(s);
}

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
  // FIX: kritische Frames doppelt
  forceShow(s);
}

bool LEDCTRL_FILAMENT::bufAnyLit() {
  if (!_buf) return false;
  for (int i = 0; i < _bufCount; ++i) {
    if (_buf[i] != 0) return true;
  }
  return false;
}

// ============================================================================
// Idle-Pulse Frame (smooth + jitter-robust)
// ============================================================================
// Änderungen:
//  - Dither-Phase ist ZEITBASIERT (now / pulseIntervalMs), nicht framebasiert
//  - Kein ditherPhase++ mehr am Ende (verhindert "shimmer" bei Loop-Jitter)
//  - setBrightness() NICHT jedes Frame (weniger Overhead / weniger Jitter)
//
// Hinweis: ditherPhase bleibt als Referenz-Parameter drin (API-kompatibel),
// wird aber nur noch als "Output" für Kompatibilität/Debug gesetzt.
static void renderIdlePulseFrame(Adafruit_NeoPixel* s,
                                 unsigned long now,
                                 uint32_t pulseRgbHex,
                                 float minBrightness,
                                 uint8_t& ditherPhase,
                                 uint16_t breathsPerMin,
                                 uint16_t pulseIntervalMs)
{
  if (!s) return;

  // NICHT pro Frame: s->setBrightness(...)
  // (Brightness wird in init() / bei Änderungen gesetzt)

  const uint8_t low8 = (uint8_t)constrain((int)lroundf(minBrightness * 255.0f), 0, 255);
  const uint8_t lvl  = breath8(breathsPerMin, now, low8, 255);
  const uint8_t glvl = Adafruit_NeoPixel::gamma8(lvl);

  const uint8_t r0 = (pulseRgbHex >> 16) & 0xFF;
  const uint8_t g0 = (pulseRgbHex >>  8) & 0xFF;
  const uint8_t b0 =  pulseRgbHex        & 0xFF;

  // ✅ Dither-Phase ZEITBASIERT (stabil bei Loop-/Netz-Jitter)
  if (pulseIntervalMs == 0) pulseIntervalMs = 1;
  ditherPhase = (uint8_t)((now / pulseIntervalMs) & 0x0F);

  auto dimDither8 = [](uint8_t base, uint8_t dim, uint8_t thr) -> uint8_t {
    const uint32_t v12 = ((uint32_t)base * (uint32_t)dim * 16U + 127U) / 255U;
    uint8_t out = (uint8_t)(v12 >> 4);
    if ((v12 & 0x0F) > thr && out < 255) out++;
    return out;
  };

  const int n = (int)s->numPixels();
  for (int i = 0; i < n; ++i) {
    const uint8_t thr = BAYER4[(ditherPhase + (i & 0x0F)) & 0x0F];

    const uint8_t r = dimDither8(r0, glvl, thr);
    const uint8_t g = dimDither8(g0, glvl, thr);
    const uint8_t b = dimDither8(b0, glvl, thr);

    s->setPixelColor(i, s->Color(r, g, b));
  }

  // ❌ KEIN ditherPhase++ mehr!
}



// ============================================================================
// Public API
// ============================================================================
void LEDCTRL_FILAMENT::init(int count, int timeout_ms, int brightness, u_int32_t color, uint32_t colorError, uint32_t colorPulse) {
  LED_COUNT      = max(0, count);
  
  LED_TIMEOUT    = max(0, timeout_ms);
  LED_BRIGHTNESS = constrain(brightness, 0, 255);
  LED_COLOR      = color;
  LED_COLOR_ERROR= colorError; 
  LED_COLOR_PULSE= colorPulse;

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

  _leds = new Adafruit_NeoPixel(LED_COUNT, LED_PIN, LED_NEO_PIXEL_TYPE);
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
  _netPauseUntil   = 0; // FIX

  FILDBG("init: count=%d pin=%d bright=%d timeout=%d color=%d error=%d pulse=%d\n", LED_COUNT, LED_PIN, LED_BRIGHTNESS, LED_TIMEOUT, LED_COLOR, LED_COLOR_ERROR, LED_COLOR_PULSE);
}

void LEDCTRL_FILAMENT::setPixel(int index, uint32_t color) {
  
  if (!_leds || !_buf) return;
  if (index < 0 || index >= _bufCount) return;

  LEDCTRL_FILAMENT::standBy(false);
  LEDCTRL_NFC::standBy(false);
  

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
  // FIX: Transitions doppelt
  forceShow(_leds);

  // Idle kurz blocken (Pulse nicht in Übergangsframe mischen)
  _idleBlockUntil = millis() + 2;
}

void LEDCTRL_FILAMENT::allOff() {
  if (!_leds || !_buf) return;

  for (int i = 0; i < _bufCount; ++i) _buf[i] = 0;
  _leds->clear();
  // FIX: Transitions doppelt
  forceShow(_leds);

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
  renderAllFromBuf(_leds); // (doppelt)

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
  // FIX: Blink-Kante doppelt
  forceShow(_leds);

  // Timeout erst ab Entfernung
  _releaseTs = _tagHeld ? 0UL : millis();

  // Idle blocken
  _idleBlockUntil = millis() + 2;
  FILDBG("errorBlink start ms=%u count=%u\n", _errBlinkMs, _errBlinkCount);
}

void LEDCTRL_FILAMENT::update() {
  if (!_leds) return;

  if (_standby) {
    return;
  }


  const unsigned long now = millis();

    // --- WebIF-Hold Ablauf: virtuelle "Tag-Entfernung" auslösen ---
    if (s_webifHoldUntil != 0 && (int32_t)(now - s_webifHoldUntil) >= 0) {
      s_webifHoldUntil = 0;

      // Simuliere: Tag wurde entfernt → Timeout kann starten
      // Wir setzen direkt _tagHeld=false und starten Release-Timer, falls was aktiv ist.
      if (_tagHeld) {
        _tagHeld = false;
        if (_errBlinkActive || _errSolidActive || bufAnyLit()) {
          _releaseTs = now;
        } else {
          _releaseTs = 0;
        }
      }
    }

  // 1) ERROR-BLINK
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
      // FIX: Blink-Kante doppelt
      forceShow(_leds);
    }

    if (intervals >= (uint32_t)_errBlinkCount * 2U) {
      _errBlinkActive = false;

      if (_releaseTs == 0 || (now - _releaseTs) < (unsigned long)LED_TIMEOUT) {
        _errSolidActive   = true;
        _lastHoldRefresh  = 0;

        const uint32_t neoErr = rgbHexToNeo(_leds, LED_COLOR_ERROR);
        for (int i = 0; i < _bufCount; ++i) _buf[i] = neoErr;
        renderAllFromBuf(_leds); // (doppelt)

        _idleBlockUntil = now + 2;
        FILDBG("errBlink -> errSolid\n");
      } else {
        allOff();
      }
    }
    return;
  }

  // 2) ERROR-SOLID
  if (_errSolidActive) {
    if (_tagHeld) {
      _releaseTs = 0;
      if (now - _lastHoldRefresh >= HOLD_REFRESH_MS) {
        _lastHoldRefresh = now;
        renderAllFromBuf(_leds); // (doppelt)
      }
      return;
    }
    if (_releaseTs != 0 && (now - _releaseTs) >= (unsigned long)LED_TIMEOUT) {
      _errSolidActive = false;
      _releaseTs      = 0;
      allOff();
    } else {
      if (now - _lastHoldRefresh >= HOLD_REFRESH_MS) {
        _lastHoldRefresh = now;
        renderAllFromBuf(_leds); // (doppelt)
      }
    }
    return;
  }

  // 3) Normale Pixel-Anzeige
  if (bufAnyLit()) {
    if (_tagHeld) {
      _releaseTs = 0;
      if (now - _lastHoldRefresh >= HOLD_REFRESH_MS) {
        _lastHoldRefresh = now;
        renderAllFromBuf(_leds); // (doppelt)
      }
      return;
    }
    if (_releaseTs != 0 && (now - _releaseTs) >= (unsigned long)LED_TIMEOUT) {
      allOff();
    } else {
      if (now - _lastHoldRefresh >= HOLD_REFRESH_MS) {
        _lastHoldRefresh = now;
        renderAllFromBuf(_leds); // (doppelt)
      }
    }
    return;
  }

  // 4) IDLE-PULSE (nur wenn nix aktiv + nix leuchtet)
  if (_idlePulseEnabled) {
    if (now < _idleBlockUntil || now < _netPauseUntil) return;

    // Stabiler Takt: nicht auf "now" snappen
    while ((uint32_t)(now - _lastPulseUpdate) >= PULSE_INTERVAL_MS) {
      _lastPulseUpdate += PULSE_INTERVAL_MS;

      renderIdlePulseFrame(_leds, _lastPulseUpdate, LED_COLOR_PULSE, _minBrightness, _ditherPhase,
                     BREATHS_PER_MIN, PULSE_INTERVAL_MS);

      neopixelShowSafe(_leds); // im Idle bewusst nur 1x
    }
  }

}

bool LEDCTRL_FILAMENT::isIdle() {
  return (!_errBlinkActive && !_errSolidActive && !bufAnyLit());
}

Adafruit_NeoPixel* LEDCTRL_FILAMENT::rawStrip() {
  return _leds;
}

// FIX: Netz busy → Idle kurz pausieren
void LEDCTRL_FILAMENT::netBusyHint(uint16_t ms) {
  const unsigned long now = millis();
  const unsigned long until = now + (unsigned long)ms;
  if (until > _netPauseUntil) _netPauseUntil = until;
}

void LEDCTRL_FILAMENT::webifHoldFor(uint16_t ms) {
  const unsigned long now = millis();
  s_webifHoldUntil = now + (unsigned long)ms;

  // Virtuell "Tag ist da" → verhindert, dass sofort Timeout läuft
  _tagHeld = true;
  _lastTagSeen = now;
  _releaseTs = 0;

  // Idle kurz blocken, damit Pulse nicht reinmischt
  _idleBlockUntil = now + 2;
}


void LEDCTRL_FILAMENT::standBy(bool state) {
  if (_standby == state) return;
  _standby = state;

  if (_standby) {
    // 🔇 ALLES hart stoppen
    _idlePulseEnabled = false;
    _errBlinkActive  = false;
    _errSolidActive  = false;
    _tagHeld         = false;
    _releaseTs       = 0;

    allOff();

    if(CONFIGV2.system.debugMode) {
      Serial.println("Standby ON: LEDs OFF, update blocked");
    } 
    
  } else {
    // ▶️ Wieder freigeben
    _idlePulseEnabled = true;
    _lastPulseUpdate  = millis();
    _idleBlockUntil   = millis() + 2;

    

    if(CONFIGV2.system.debugMode) {
      Serial.println("Standby OFF: normal operation resumed");
    }

  }

  // MQTT-Status senden
  publishAnimationStatus(!state); // true=ON, false=OFF


}





