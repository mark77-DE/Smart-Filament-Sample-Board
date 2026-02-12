#include <Arduino.h>
#include <math.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include "ledctrl_nfc.h"
#include "neopixel_guard.h"

// ============================================================================
// Debug
// ============================================================================
#ifdef LED_NFC_DEBUG
  #define DBG(...) do { Serial.printf("[NFCLED][t=%lu] ", millis()); Serial.printf(__VA_ARGS__); } while (0)
  static const char* stName(uint8_t s) {
    switch (s) {
      case 0: return "OFF";
      case 1: return "SUCCESS_BLINK";
      case 2: return "SUCCESS";
      case 3: return "ERROR";
      default: return "?";
    }
  }
#else
  #define DBG(...) do {} while (0)
  static inline const char* stName(uint8_t) { return ""; }
#endif

// ============================================================================
// Öffentliche Konfiguration (via loadNfcLedConfigV2 / config_v2.json)
// ============================================================================
int           NFC_LED_COUNT       = 8;
int           NFC_LED_PIN         = 15;
int           NFC_LED_BRIGHTNESS  = 255;
unsigned long NFC_LED_TIMEOUT     = 6000;     // Timeout in ms (erst ab Tag-Entfernung)

uint32_t NFC_LED_COLOR_SUCCESS = 0x00FF00;    // 0xRRGGBB
uint32_t NFC_LED_COLOR_ERROR   = 0xFF0000;    // 0xRRGGBB
uint32_t NFC_LED_COLOR_PULSE   = 0x0033AA;    // 0xRRGGBB

bool     NFC_LED_SUCCESS_BLINK_ENABLED = true;
uint8_t  NFC_LED_SUCCESS_BLINK_COUNT   = 3;
uint16_t NFC_LED_SUCCESS_BLINK_MS      = 150;

// ============================================================================
// Interner Zustand
// ============================================================================
enum LedState : uint8_t { LED_OFF, LED_SUCCESS_BLINK, LED_SUCCESS, LED_ERROR };
static LedState currentState = LED_OFF;

// --- Blink (phasenbasiert) ---
static uint8_t       s_successBlinkStep = 0;     // Half-steps seit Start
static bool          s_successBlinkOn   = false; // gerade=AN / ungerade=AUS
static unsigned long s_blinkStartTs     = 0;     // feste Startzeit (Phasenanker)
static uint16_t      s_blinkMs          = 150;   // verwendetes Blink-Intervall
static constexpr uint16_t MIN_BLINK_MS  = 25;    // Untergrenze für Blink-Intervall
static constexpr uint8_t  MAX_BLINK_COUNT = 10;  // Obergrenze Blink-Zyklen

// --- Präsenz/Sticky-Hold ---
static bool          s_tagHeld          = false; // Tag physisch vor Ort (mit Grace)
static unsigned long s_lastTagSeen      = 0;     // Zeitpunkt der letzten Roh-Erkennung
static const uint16_t TAG_HELD_GRACE_MS = 200;   // gegen kurze Mess-Lücken

// --- Reassert (gegen halbe Frames / RMT-Glitches) ---
static bool          s_holdActive       = false;
static uint32_t      s_holdColorNeo     = 0;     // aktuell „stabil“ anzuzeigende Farbe
static unsigned long s_lastHoldRefresh  = 0;
static const uint16_t HOLD_REFRESH_MS   = 25;    // Reassert-Intervall

// --- Timeout (startet erst nach Tag-Entfernung) ---
static unsigned long s_releaseTs        = 0;     // 0 = kein Timeout aktiv

// --- Idle-Pulse (Breathing) ---
static bool          idlePulseEnabled   = true;
static float         minBrightness      = 0.30f; // minimaler Helligkeitsfaktor [0..1]
static unsigned long s_lastPulseUpdate  = 0;
// FIX: Idle-FPS entschärfen (ca. 40 FPS)
static const uint16_t PULSE_INTERVAL_MS = 25;
static const uint16_t BREATHS_PER_MIN   = 15;
static const uint8_t  BAYER4[16]        = {
  0,8,2,10, 12,4,14,6, 3,11,1,9, 15,7,13,5
};
static uint8_t ditherPhase = 0;

// --- Idle-Blocker (wirkt NUR im Idle) ---
static unsigned long s_idleBlockUntil   = 0;

// --- Debounce für Success-Trigger (gegen Doppeltrigger) ---
static unsigned long s_lastSuccessCmdTs = 0;
static const uint16_t SUCCESS_DEBOUNCE_MS = 200;

// FIX: Netzlast-Pause (Idle-Frames aussetzen)
unsigned long LEDCTRL_NFC::s_netPauseUntil = 0;


// standby: Alle LEDs aus, kein Update (auch kein Idle-Pulse)
static bool _standby = false;
bool LEDCTRL_NFC::_standby = false;

// ============================================================================
// Strip-Instanz (intern) – implementiert in ledctrl_nfc.h
// ============================================================================
Adafruit_NeoPixel* LEDCTRL_NFC::_leds = nullptr;

// Optionaler Zugriff (nur weiterreichen/lesen)
Adafruit_NeoPixel* LEDCTRL_NFC::rawStrip() { return _leds; }

// ============================================================================
// Helpers (file-scope)
// ============================================================================
static inline int pixCount() {
  return LEDCTRL_NFC::rawStrip() ? (int)LEDCTRL_NFC::rawStrip()->numPixels() : 0;
}

static inline uint32_t rgbHexToNeo(uint32_t rgb) {
  const uint8_t r = (rgb >> 16) & 0xFF;
  const uint8_t g = (rgb >>  8) & 0xFF;
  const uint8_t b =  rgb        & 0xFF;
  return LEDCTRL_NFC::rawStrip()->Color(r, g, b);
}

static inline void renderAll(uint32_t neo) {
  const int n = pixCount();
  for (int i = 0; i < n; ++i) LEDCTRL_NFC::rawStrip()->setPixelColor(i, neo);
}

static inline void forceFill(uint32_t neo) {
  // Doppelt senden (kurzer Abstand), um Glitches/halbe Frames zu vermeiden
  renderAll(neo);
  neopixelShowSafe(LEDCTRL_NFC::rawStrip());
  delayMicroseconds(300);
  neopixelShowSafe(LEDCTRL_NFC::rawStrip());
}

// FIX: generisches Doppel-show wenn kein Voll-Fill (Blink/Idle/Einzelpixel)
static inline void forceShowNfc() {
  if (!LEDCTRL_NFC::rawStrip()) return;
  neopixelShowSafe(LEDCTRL_NFC::rawStrip());
  delayMicroseconds(300);
  neopixelShowSafe(LEDCTRL_NFC::rawStrip());
}

static uint8_t breath8(uint16_t bpm, uint32_t nowMs, uint8_t low, uint8_t high) {
  const float periodMs = 60000.0f / (float)bpm;
  float phase01 = fmodf((float)nowMs, periodMs) / periodMs;
  float s = (sinf(phase01 * 2.0f * PI) + 1.0f) * 0.5f;
  uint16_t v = (uint16_t)(low + s * (float)(high - low) + 0.5f);
  return (uint8_t)min<uint16_t>(v, 255);
}

static void renderIdlePulseFrame(unsigned long now) {
  if (!idlePulseEnabled || !LEDCTRL_NFC::rawStrip()) return;


  const uint8_t low8 = (uint8_t)constrain(
      (int)lroundf(minBrightness * 255.0f), 0, 255);

  const uint8_t lvl  = breath8(BREATHS_PER_MIN, now, low8, 255);
  const uint8_t glvl = Adafruit_NeoPixel::gamma8(lvl);

  const uint8_t r0 = (NFC_LED_COLOR_PULSE >> 16) & 0xFF;
  const uint8_t g0 = (NFC_LED_COLOR_PULSE >>  8) & 0xFF;
  const uint8_t b0 = (NFC_LED_COLOR_PULSE      ) & 0xFF;

  // 🔧 Dither-Phase ZEITBASIERT (kein Frame-Jitter mehr)
  const uint8_t ditherPhase =
      (uint8_t)((now / PULSE_INTERVAL_MS) & 0x0F);

  auto dimDither8 = [](uint8_t base, uint8_t dim, uint8_t thr) -> uint8_t {
    const uint32_t v12 = ((uint32_t)base * (uint32_t)dim * 16U + 127U) / 255U;
    uint8_t out = (uint8_t)(v12 >> 4);
    if ((v12 & 0x0F) > thr && out < 255) out++;
    return out;
  };

  const int n = pixCount();
  for (int i = 0; i < n; ++i) {
    const uint8_t thr =
        BAYER4[(ditherPhase + (i & 0x0F)) & 0x0F];

    const uint8_t r = dimDither8(r0, glvl, thr);
    const uint8_t g = dimDither8(g0, glvl, thr);
    const uint8_t b = dimDither8(b0, glvl, thr);

    LEDCTRL_NFC::rawStrip()->setPixelColor(
        i, LEDCTRL_NFC::rawStrip()->Color(r, g, b));
  }
}


static inline void dbgState(const char* where, LedState s) {
  (void)where; (void)s;
#ifdef LED_NFC_DEBUG
  DBG("%-12s state=%s(%u) held=%u relTs=%lu\n",
      where, stName((uint8_t)s), (unsigned)s, (unsigned)s_tagHeld, s_releaseTs);
#endif
}

// ============================================================================
// Public API
// ============================================================================
void LEDCTRL_NFC::init(int count, int pin, int timeout_ms, int brightness,
                       uint32_t colorSuccess, uint32_t colorError, uint32_t colorPulse,
                       bool successBlinkEnabled, int successBlinkCount, int successBlinkMs) {

  NFC_LED_COUNT       = max(0, count);
  NFC_LED_PIN         = pin;
  NFC_LED_TIMEOUT     = (unsigned long)max(0, timeout_ms);
  NFC_LED_BRIGHTNESS  = constrain(brightness, 0, 255);
  NFC_LED_COLOR_SUCCESS = colorSuccess;
  NFC_LED_COLOR_ERROR   = colorError;
  NFC_LED_COLOR_PULSE   = colorPulse;
  NFC_LED_SUCCESS_BLINK_ENABLED = successBlinkEnabled;
  NFC_LED_SUCCESS_BLINK_COUNT   = min<uint8_t>(successBlinkCount, MAX_BLINK_COUNT);
  NFC_LED_SUCCESS_BLINK_MS      = successBlinkMs;

  // Vorherigen Strip aufräumen
  if (_leds) {
    _leds->setBrightness(255);
    _leds->clear();
    neopixelShowSafe(_leds);
    delete _leds;
    _leds = nullptr;
  }

  if (NFC_LED_COUNT <= 0) {
    currentState = LED_OFF;
    return;
  }

  // Pin vorbereiten
  pinMode(NFC_LED_PIN, OUTPUT);
  digitalWrite(NFC_LED_PIN, LOW);

  // Strip anlegen
  _leds = new Adafruit_NeoPixel(NFC_LED_COUNT, NFC_LED_PIN, NEO_GRB + NEO_KHZ800);
  _leds->begin();
  _leds->clear();
  _leds->setBrightness(NFC_LED_BRIGHTNESS);
  neopixelShowSafe(_leds);

  // Zustand zurücksetzen
  s_lastPulseUpdate  = millis();
  s_lastSuccessCmdTs = 0;
  s_holdActive       = false;
  s_tagHeld          = false;
  s_releaseTs        = 0;
  currentState       = LED_OFF;
  s_netPauseUntil    = 0; // FIX

  dbgState("init()", currentState);
}

void LEDCTRL_NFC::setPixel(int index, uint32_t color) {
  if (!_leds) return;
  const int n = pixCount();
  if (index < 0 || index >= n) return;
  _leds->setPixelColor(index, color);
  // FIX: Doppel-show bei Einzelpixel
  forceShowNfc();
}

void LEDCTRL_NFC::allOff() {
  if (!_leds) return;

  _leds->setBrightness(NFC_LED_BRIGHTNESS);
  _leds->clear();

  s_holdActive      = false;
  s_releaseTs       = 0;
  currentState      = LED_OFF;

  s_idleBlockUntil  = 0;
  s_lastPulseUpdate = millis() - PULSE_INTERVAL_MS;
  neopixelShowSafe(_leds);

  dbgState("allOff()", currentState);
}

// ---------------------------------------------------------------------------
// Präsenz-Tracking (Timeout startet erst bei echter Entfernung)
// ---------------------------------------------------------------------------
void LEDCTRL_NFC::tagPresenceTick(bool present) {

  if (_standby && present) {
        standBy(false);  // Wake-Up bei Tag-Erkennung
  }


  const unsigned long now = millis();

  if (present) {
    s_lastTagSeen = now;
    if (!s_tagHeld) {
      s_tagHeld    = true;
      s_releaseTs  = 0;
      DBG("presence: RISING  state=%s relTs->0\n", stName((uint8_t)currentState));
    }
  } else {
    if (s_tagHeld && (now - s_lastTagSeen) > TAG_HELD_GRACE_MS) {
      s_tagHeld = false;
      if (currentState == LED_SUCCESS || currentState == LED_ERROR) {
        s_releaseTs = now;
        DBG("presence: FALLING startTimeout relTs=%lu\n", s_releaseTs);
      } else {
        s_releaseTs = 0;
        DBG("presence: FALLING (no active hold state)\n");
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Erfolg (Blink → Solid) bzw. Fehler (Solid)
// ---------------------------------------------------------------------------
void LEDCTRL_NFC::confirmSuccess() {
  if (!_leds) return;

  const unsigned long now = millis();
  if (now - s_lastSuccessCmdTs < SUCCESS_DEBOUNCE_MS) return; // Doppeltrigger verhindern
  s_lastSuccessCmdTs = now;

  _leds->setBrightness(NFC_LED_BRIGHTNESS);

  // Optionaler Blink vor „Solid Success“
  if (NFC_LED_SUCCESS_BLINK_ENABLED &&
      NFC_LED_SUCCESS_BLINK_COUNT > 0 &&
      currentState != LED_SUCCESS_BLINK)
  {
    s_blinkMs = (uint16_t)max<int>(NFC_LED_SUCCESS_BLINK_MS, MIN_BLINK_MS);

    currentState        = LED_SUCCESS_BLINK;
    s_successBlinkStep  = 0;
    s_blinkStartTs      = now;
    s_successBlinkOn    = true;
    s_holdActive        = false;
    s_releaseTs         = 0;

    renderAll(rgbHexToNeo(NFC_LED_COLOR_SUCCESS));
    // FIX: Blink-Start doppelt
    forceShowNfc();

    DBG("confirmSuccess(BLINK) state=%s held=%u relTs=%lu blinkMs=%u count=%u\n",
        stName((uint8_t)currentState), (unsigned)s_tagHeld, s_releaseTs, s_blinkMs, NFC_LED_SUCCESS_BLINK_COUNT);
    return;
  }

  // Fallback: direkt stabiler Success (Solid)
  currentState       = LED_SUCCESS;
  s_holdActive       = true;
  s_holdColorNeo     = rgbHexToNeo(NFC_LED_COLOR_SUCCESS);
  s_lastHoldRefresh  = 0;
  s_releaseTs        = s_tagHeld ? 0 : now;

  forceFill(s_holdColorNeo);
  dbgState("confirmSuccess(SOLID)", currentState);
}

void LEDCTRL_NFC::confirmError() {
  if (!_leds) return;

  const unsigned long now = millis();
  _leds->setBrightness(NFC_LED_BRIGHTNESS);

  currentState       = LED_ERROR;
  s_holdActive       = true;
  s_holdColorNeo     = rgbHexToNeo(NFC_LED_COLOR_ERROR);
  s_lastHoldRefresh  = 0;
  s_idleBlockUntil   = now + 2;
  s_releaseTs        = s_tagHeld ? 0 : now;

  forceFill(s_holdColorNeo);
  dbgState("confirmError(SOLID)", currentState);
}

// Backwards-Compat
void LEDCTRL_NFC::showSuccess() { confirmSuccess(); }
void LEDCTRL_NFC::showError()   { confirmError();   }

// ---------------------------------------------------------------------------
// Periodisches Ticken (in loop aufrufen)
// ---------------------------------------------------------------------------
void LEDCTRL_NFC::update() {
  if (!_leds) return;
  if (_standby) return;

  const unsigned long now = millis();

  // === SUCCESS (Solid)
  if (currentState == LED_SUCCESS) {
    if (s_tagHeld) {
      s_releaseTs = 0;
      if (s_holdActive && now - s_lastHoldRefresh >= HOLD_REFRESH_MS) {
        s_lastHoldRefresh = now;
        forceFill(s_holdColorNeo);
      }
      return;
    }

    if (s_releaseTs != 0 && (now - s_releaseTs) >= NFC_LED_TIMEOUT) {
      currentState      = LED_OFF;
      s_holdActive      = false;
      s_releaseTs       = 0;
      s_lastPulseUpdate = now - PULSE_INTERVAL_MS;
      s_idleBlockUntil  = now + 2;
      dbgState("timeout->OFF", currentState);
    } else {
      if (s_holdActive && now - s_lastHoldRefresh >= HOLD_REFRESH_MS) {
        s_lastHoldRefresh = now;
        forceFill(s_holdColorNeo);
      }
    }
  }

  // === ERROR (Solid)
  if (currentState == LED_ERROR) {
    if (s_tagHeld) {
      s_releaseTs = 0;
      if (s_holdActive && now - s_lastHoldRefresh >= HOLD_REFRESH_MS) {
        s_lastHoldRefresh = now;
        forceFill(s_holdColorNeo);
      }
      return;
    }

    if (s_releaseTs != 0 && (now - s_releaseTs) >= NFC_LED_TIMEOUT) {
      currentState      = LED_OFF;
      s_holdActive      = false;
      s_releaseTs       = 0;
      s_lastPulseUpdate = now - PULSE_INTERVAL_MS;
      s_idleBlockUntil  = now + 2;
      dbgState("timeout->OFF", currentState);
    } else {
      if (s_holdActive && now - s_lastHoldRefresh >= HOLD_REFRESH_MS) {
        s_lastHoldRefresh = now;
        forceFill(s_holdColorNeo);
      }
    }
  }

  // === SUCCESS BLINK
  if (currentState == LED_SUCCESS_BLINK) {
    const uint32_t intervals = (uint32_t)((now - s_blinkStartTs) / s_blinkMs);

    if (intervals != s_successBlinkStep) {
      s_successBlinkStep = (uint8_t)min<uint32_t>(255U, intervals);
      s_successBlinkOn   = ((intervals & 1U) == 0U);

      const uint32_t cOn  = rgbHexToNeo(NFC_LED_COLOR_SUCCESS);
      const uint32_t cOff = 0;
      renderAll(s_successBlinkOn ? cOn : cOff);
      // FIX: Blink-Kante doppelt
      forceShowNfc();
    }

    if (intervals >= (uint32_t)NFC_LED_SUCCESS_BLINK_COUNT * 2U) {
      const uint32_t cOn = rgbHexToNeo(NFC_LED_COLOR_SUCCESS);

      currentState      = LED_SUCCESS;
      s_holdActive      = true;
      s_holdColorNeo    = cOn;
      s_lastHoldRefresh = 0;
      s_releaseTs       = s_tagHeld ? 0 : now;
      s_idleBlockUntil  = now + 2;

      forceFill(cOn);
      dbgState("blink->SUCCESS", currentState);
    }
    return;
  }

  // === IDLE (Breathing Pulse)
  if (currentState == LED_OFF) {
    if (now < s_idleBlockUntil || now < s_netPauseUntil) return;

    // Stabiler Takt: nicht auf "now" snappen, sondern Intervalle nachholen
    while ((uint32_t)(now - s_lastPulseUpdate) >= PULSE_INTERVAL_MS) {
      s_lastPulseUpdate += PULSE_INTERVAL_MS;

      renderIdlePulseFrame(s_lastPulseUpdate);
      neopixelShowSafe(_leds);  // im Idle weiterhin nur 1x show
    }
  }

  
}

bool LEDCTRL_NFC::isIdle() {
  return currentState == LED_OFF;
}

// FIX: Netz busy → Idle kurz pausieren
void LEDCTRL_NFC::netBusyHint(uint16_t ms) {
  const unsigned long now = millis();
  const unsigned long until = now + (unsigned long)ms;
  if (until > s_netPauseUntil) s_netPauseUntil = until;
}



void LEDCTRL_NFC::standBy(bool state) {
    if (_standby == state) return;
    _standby = state;

    if (_standby) {
        idlePulseEnabled = false;
        s_holdActive     = false;
        s_tagHeld        = false;
        s_releaseTs      = 0;
        currentState     = LED_OFF;
        allOff();
        DBG("Standby ON: LEDs OFF");
    } else {
        idlePulseEnabled   = true;
        s_lastPulseUpdate  = millis();
        s_idleBlockUntil   = millis() + 2;
        DBG("Standby OFF: normal operation resumed");
    }
}
