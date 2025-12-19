#include "gpio_hardware.h"

// ------------------------ Debug ------------------------
#ifdef GPIO_HW_DEBUG
  #define GDBG(...) do { Serial.printf("[GPIO][t=%lu] ", millis()); Serial.printf(__VA_ARGS__); } while (0)
#else
  #define GDBG(...) do {} while (0)
#endif

// ============================================================================
// Konfig-Defaults (greifen, falls Keys in config.json fehlen)
// ============================================================================
static int   CFG_BTN_PIN            = 32;    // -1 = deaktiviert
static bool  CFG_BTN_PULLUP         = true;  // interner PullUp -> active-low
static int   CFG_BTN_DEBOUNCE_MS    = 30;
static int   CFG_BTN_LONG_MS        = 800;
static int   CFG_BTN_DOUBLE_MS      = 400;
static int   CFG_BTN_HOLD_MS        = 250;

static int   CFG_BUZ_PIN            = 33;    // -1 = deaktiviert
static bool  CFG_BUZ_ACTIVE_HIGH    = true;  // HIGH = an (bei aktivem Buzzer)
static bool  CFG_BUZ_PASSIVE        = false; // false=aktiv (on/off), true=passiv (PWM/tone)
static int   CFG_BUZ_FREQ_HZ        = 4000;
static int   CFG_BUZ_SINGLE_ON_MS   = 80;
static int   CFG_BUZ_DOUBLE_ON_MS   = 60;
static int   CFG_BUZ_DOUBLE_GAP_MS  = 80;
static int   CFG_BUZ_ERR_ON_MS      = 50;
static int   CFG_BUZ_ERR_GAP_MS     = 60;
static int   CFG_BUZ_ERR_COUNT      = 3;

// ============================================================================
// Interner Button-State (entprellt + Erkennung)
// ============================================================================
static bool           s_btnEnabled      = false;
static bool           s_btnActiveLow    = true;    // aus PullUp abgeleitet
static bool           s_btnStable       = false;   // entprellter Zustand (pressed = true)
static bool           s_btnPrevStable   = false;
static unsigned long  s_btnChangeTs     = 0;       // Zeit der letzten ROH-Änderung
static bool           s_btnRawLast      = false;

static unsigned long  s_pressStartTs    = 0;
static bool           s_longFired       = false;
static unsigned long  s_lastHoldTick    = 0;

// Double-Click-Fenster
static bool           s_doubleArmed     = false;
static unsigned long  s_doubleUntilTs   = 0;
static bool           s_shortCandidate  = false;

// Event-Flags (Getter liefern true genau einmal)
static bool s_evShort  = false;
static bool s_evLong   = false;
static bool s_evDouble = false;
static bool s_evHold   = false;

// ============================================================================
// Interner Buzzer-State (Sequencer)
// ============================================================================
#ifdef ARDUINO_ARCH_ESP32
  static const int LEDC_CH   = 6;  // fixer Kanal
  static const int LEDC_BITS = 10; // 10-bit duty
#endif

struct Step { bool on; uint16_t ms; };

static bool           s_buzEnabled    = false;
static unsigned long  s_buzStepUntil  = 0;
static uint8_t        s_buzPos        = 0;
static uint8_t        s_buzLen        = 0;
static Step           s_buzSeq[8];        // reicht für Muster

// ----------------------------------------------------------------------------
// Buzzer Low-Level
// ----------------------------------------------------------------------------
static inline void buzzer_output(bool on) {
  if (!s_buzEnabled) return;

  if (CFG_BUZ_PASSIVE) {
    // PASSIVER BUZZER -> Ton erzeugen (PWM)
  #ifdef ARDUINO_ARCH_ESP32
    if (on) {
      ledcWriteTone(LEDC_CH, (double)CFG_BUZ_FREQ_HZ);
    } else {
      ledcWriteTone(LEDC_CH, 0);
    }
  #else
    if (CFG_BUZ_PIN < 0) return;
    if (on) tone((uint8_t)CFG_BUZ_PIN, (unsigned)CFG_BUZ_FREQ_HZ);
    else    noTone((uint8_t)CFG_BUZ_PIN);
  #endif
    return;
  }

  // AKTIVER BUZZER -> Pegel schalten
  if (CFG_BUZ_PIN < 0) return;
  pinMode(CFG_BUZ_PIN, OUTPUT);
  if (on) {
    digitalWrite(CFG_BUZ_PIN, CFG_BUZ_ACTIVE_HIGH ? HIGH : LOW);
  } else {
    digitalWrite(CFG_BUZ_PIN, CFG_BUZ_ACTIVE_HIGH ? LOW : HIGH);
  }
}

static inline void buzzer_start_sequence(const Step* seq, uint8_t len) {
  if (!s_buzEnabled || len == 0) return;

  s_buzLen = (len > (uint8_t)(sizeof(s_buzSeq)/sizeof(s_buzSeq[0])))
             ? (uint8_t)(sizeof(s_buzSeq)/sizeof(s_buzSeq[0])) : len;

  for (uint8_t i=0; i<s_buzLen; ++i) s_buzSeq[i] = seq[i];

  s_buzPos       = 0;
  buzzer_output(s_buzSeq[0].on);
  s_buzStepUntil = millis() + s_buzSeq[0].ms;
}

// ============================================================================
// Public: Init
// ============================================================================
void gpiohw_init() {
  // --- Button aus CONFIG übernehmen (falls vorhanden); sonst Defaults ---
  #ifdef CONFIG_HAS_GPIO
    CFG_BTN_PIN         = CONFIG.button.pin;
    CFG_BTN_PULLUP      = CONFIG.button.pullup;
    CFG_BTN_DEBOUNCE_MS = CONFIG.button.debounceMs;
    CFG_BTN_LONG_MS     = CONFIG.button.longMs;
    CFG_BTN_DOUBLE_MS   = CONFIG.button.doubleGapMs;
    CFG_BTN_HOLD_MS     = CONFIG.button.holdRepeatMs;

    CFG_BUZ_PIN            = CONFIG.buzzer.pin;
    CFG_BUZ_ACTIVE_HIGH    = CONFIG.buzzer.activeHigh;
    CFG_BUZ_PASSIVE        = CONFIG.buzzer.passive;
    CFG_BUZ_FREQ_HZ        = CONFIG.buzzer.freqHz;
    CFG_BUZ_SINGLE_ON_MS   = CONFIG.buzzer.singleMs;
    CFG_BUZ_DOUBLE_ON_MS   = CONFIG.buzzer.doubleOnMs;
    CFG_BUZ_DOUBLE_GAP_MS  = CONFIG.buzzer.doubleGapMs;
    CFG_BUZ_ERR_ON_MS      = CONFIG.buzzer.errorOnMs;
    CFG_BUZ_ERR_GAP_MS     = CONFIG.buzzer.errorGapMs;
    CFG_BUZ_ERR_COUNT      = CONFIG.buzzer.errorCount;
  #endif

  // --- Button einrichten ---
  s_btnEnabled   = (CFG_BTN_PIN >= 0);
  s_btnActiveLow = CFG_BTN_PULLUP;

  if (s_btnEnabled) {
    pinMode(CFG_BTN_PIN, CFG_BTN_PULLUP ? INPUT_PULLUP : INPUT);

    s_btnRawLast     = s_btnActiveLow ? (digitalRead(CFG_BTN_PIN) == LOW)
                                      : (digitalRead(CFG_BTN_PIN) == HIGH);
    s_btnStable      = s_btnRawLast;
    s_btnPrevStable  = s_btnRawLast;
    s_btnChangeTs    = millis();

    s_pressStartTs   = 0;
    s_longFired      = false;
    s_doubleArmed    = false;
    s_shortCandidate = false;

    s_evShort = s_evLong = s_evDouble = s_evHold = false;
  }

  // --- Buzzer einrichten ---
  s_buzEnabled = (CFG_BUZ_PIN >= 0);
  if (s_buzEnabled) {
  #ifdef ARDUINO_ARCH_ESP32
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(CFG_BUZ_PIN, CFG_BUZ_FREQ_HZ, LEDC_BITS);
  #else
    ledcSetup(LEDC_CH, CFG_BUZ_FREQ_HZ, LEDC_BITS);
    ledcAttachPin(CFG_BUZ_PIN, LEDC_CH);
  #endif
  ledcWriteTone(LEDC_CH, 0);
#endif

  }

  GDBG("init: btnPin=%d pullup=%d buzPin=%d passive=%d freq=%dHz\n",
       CFG_BTN_PIN, (int)CFG_BTN_PULLUP, CFG_BUZ_PIN, (int)CFG_BUZ_PASSIVE, CFG_BUZ_FREQ_HZ);
}

// ============================================================================
// Public: Tick / Update
// ============================================================================
void gpiohw_update() {
  gpiohw_tick(millis());
}

void gpiohw_tick(unsigned long now) {
  // -------- Button ----------
  if (s_btnEnabled) {
    const bool raw = s_btnActiveLow ? (digitalRead(CFG_BTN_PIN) == LOW)
                                    : (digitalRead(CFG_BTN_PIN) == HIGH);

    if (raw != s_btnRawLast) {
      s_btnRawLast  = raw;
      s_btnChangeTs = now; // Beginn einer potenziellen Flanke
    }

    // Entprellung: Zustand erst übernehmen, wenn stabil > debounce
    if ( (now - s_btnChangeTs) >= (unsigned long)CFG_BTN_DEBOUNCE_MS && raw != s_btnStable ) {
      s_btnPrevStable = s_btnStable;
      s_btnStable     = raw;

      if (s_btnStable) {
        // Rising (gedrückt)
        s_pressStartTs  = now;         // <<< FIX: Start-Zeitpunkt nur hier setzen
        s_longFired     = false;
        s_lastHoldTick  = now;
        // Short-Event wird erst beim Release/Timeout finalisiert
      } else {
        // Falling (losgelassen)
        if (!s_longFired) {
          // Short-Kandidat
          if (s_doubleArmed && now <= s_doubleUntilTs) {
            // -> DOUBLE
            s_doubleArmed    = false;
            s_shortCandidate = false;
            s_evDouble       = true;
          } else {
            // Double-Fenster öffnen; Short evtl. später finalisieren
            s_shortCandidate = true;
            s_doubleArmed    = true;
            s_doubleUntilTs  = now + CFG_BTN_DOUBLE_MS;
          }
        }
        s_pressStartTs = 0;            // <<< FIX: Baseline löschen, damit kein Phantom-Long
      }
    }

    // Long / Hold während gedrückt
    if (s_btnStable) {
      // <<< FIX: beide Events nur nach echter Rising-Edge (s_pressStartTs != 0)
      if (s_pressStartTs != 0 &&
          !s_longFired &&
          (now - s_pressStartTs) >= (unsigned long)CFG_BTN_LONG_MS) {
        s_longFired     = true;
        s_doubleArmed   = false;       // Long verdrängt Double
        s_shortCandidate= false;
        s_evLong        = true;
        s_lastHoldTick  = now;
      }
      if (s_pressStartTs != 0 &&
          s_longFired &&
          (now - s_lastHoldTick) >= (unsigned long)CFG_BTN_HOLD_MS) {
        s_lastHoldTick = now;
        s_evHold       = true;
      }
    } else {
      // nicht gedrückt -> Double-Click Fenster verwalten
      if (s_doubleArmed && now > s_doubleUntilTs) {
        s_doubleArmed = false;
        if (s_shortCandidate) {
          s_shortCandidate = false;
          s_evShort = true;
        }
      }
    }
  }

  // -------- Buzzer ----------
  if (s_buzEnabled && s_buzLen > 0) {
    if ((long)(now - s_buzStepUntil) >= 0) {
      // zum nächsten Step wechseln
      ++s_buzPos;
      if (s_buzPos >= s_buzLen) {
        // Sequenz fertig
        buzzer_output(false);
        s_buzLen = 0;
      } else {
        buzzer_output(s_buzSeq[s_buzPos].on);
        s_buzStepUntil = now + s_buzSeq[s_buzPos].ms;
      }
    }
  }
}

// ============================================================================
// Public: Button-Event Getter (auto-reset)
// ============================================================================
bool button_short_press()  { bool v = s_evShort;  s_evShort  = false; return v; }
bool button_double_press() { bool v = s_evDouble; s_evDouble = false; return v; }
bool button_long_press()   { bool v = s_evLong;   s_evLong   = false; return v; }
bool button_hold()         { bool v = s_evHold;   s_evHold   = false; return v; }

// ============================================================================
// Public: Buzzer-APIs (Sequenzen)
// ============================================================================
void buzzer_single_beep() {
  if (!s_buzEnabled) return;
  const Step seq[] = {
    { true,  (uint16_t)CFG_BUZ_SINGLE_ON_MS },
    { false, 10 }
  };
  buzzer_start_sequence(seq, 2);
}

void buzzer_double_beep() {
  if (!s_buzEnabled) return;
  const Step seq[] = {
    { true,  (uint16_t)CFG_BUZ_DOUBLE_ON_MS },
    { false, (uint16_t)CFG_BUZ_DOUBLE_GAP_MS },
    { true,  (uint16_t)CFG_BUZ_DOUBLE_ON_MS },
    { false, 10 }
  };
  buzzer_start_sequence(seq, 4);
}

void buzzer_error_beep() {
  if (!s_buzEnabled) return;
  const int n = max(1, min(8, CFG_BUZ_ERR_COUNT)); // limit für Sequenzpuffer
  Step seq[2*8 + 1];
  int  idx = 0;
  for (int i=0; i<n; ++i) {
    seq[idx++] = { true,  (uint16_t)CFG_BUZ_ERR_ON_MS  };
    seq[idx++] = { false, (uint16_t)CFG_BUZ_ERR_GAP_MS };
  }
  seq[idx++] = { false, 10 };
  buzzer_start_sequence(seq, (uint8_t)idx);
}

void buzzer_stop() {
  s_buzLen = 0;
  buzzer_output(false);
}

bool buzzer_busy() {
  return s_buzLen > 0;
}
