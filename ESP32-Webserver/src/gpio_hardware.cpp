#include "gpio_hardware.h"
#include "config.h"
#include "pins.h"

// ------------------------ Debug ------------------------
#ifdef GPIO_HW_DEBUG
  #define GDBG(...) do { Serial.printf("[GPIO][t=%lu] ", millis()); Serial.printf(__VA_ARGS__); } while (0)
#else
  #define GDBG(...) do {} while (0)
#endif

// ============================================================================
// Konfig-Defaults (greifen, falls Keys in config_v2.json fehlen)
// ============================================================================

static bool  CFG_BTN_PULLUP         = true;  // interner PullUp -> active-low
static int   CFG_BTN_DEBOUNCE_MS    = 30;
static int   CFG_BTN_LONG_MS        = 800;
static int   CFG_BTN_DOUBLE_MS      = 400;
static int   CFG_BTN_HOLD_MS        = 250;


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
static bool s_evShort       = false;
static bool s_evLong        = false;
static bool s_evDouble      = false;
static bool s_evHold        = false;
static bool s_evTapRelease  = false;  // sofortiges Release-Event (beim Loslassen)

// ============================================================================
// Interner Buzzer-State (Sequencer)
// ============================================================================
#ifdef ARDUINO_ARCH_ESP32
  #include <esp_arduino_version.h>
  static const int LEDC_CH   = 6;   // fixer Fallback-Kanal für alte Cores
  static const int LEDC_BITS = 10;  // 10-bit duty
  static int s_ledcChannel   = -1;  // tatsächlich verwendeter Kanal (v3 liefert ihn)
#endif

struct Step { bool on; uint16_t ms; };

static bool           s_buzEnabled    = false;
static bool           s_buzInitialized = false;
static unsigned long  s_buzStepUntil  = 0;
static uint8_t        s_buzPos        = 0;
static uint8_t        s_buzLen        = 0;
static Step           s_buzSeq[8];        // reicht für Muster

// ----------------------------------------------------------------------------
// Buzzer Low-Level
// ----------------------------------------------------------------------------
static inline void buzzer_output(bool on) {

  if (!s_buzInitialized) return;
  // WICHTIG: Beim Re-Init wollen wir "AUS" auch dann erzwingen,
  // wenn s_buzEnabled gerade false ist.
  if (!s_buzEnabled && on) return;


  if (CFG_BUZ_PASSIVE) {
  #ifdef ARDUINO_ARCH_ESP32
    if (s_ledcChannel < 0) return; // Sicherheitsnetz
    if (on) ledcWriteTone((uint8_t)s_ledcChannel, (uint32_t)CFG_BUZ_FREQ_HZ);
    else    ledcWriteTone((uint8_t)s_ledcChannel, 0);
  #else
    if (BUZ_PIN < 0) return;
    if (on) tone((uint8_t)BUZ_PIN, (unsigned)CFG_BUZ_FREQ_HZ);
    else    noTone((uint8_t)BUZ_PIN);
  #endif
    return;
  }

  // AKTIVER BUZZER -> Pegel schalten
  if (BUZ_PIN < 0) return;
  if (on)  digitalWrite(BUZ_PIN, CFG_BUZ_ACTIVE_HIGH ? HIGH : LOW);
  else     digitalWrite(BUZ_PIN, CFG_BUZ_ACTIVE_HIGH ? LOW  : HIGH);
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

    s_buzInitialized = false;
    // --- RE-INIT CLEANUP (wichtig bei applyConfig/import) ---
    // Sequencer hart stoppen + Ausgang AUS
    s_buzLen = 0;
    s_buzPos = 0;
    s_buzStepUntil = 0;

    // Wenn gerade ein Pattern lief: wirklich abschalten
    buzzer_output(false);


  #ifdef ARDUINO_ARCH_ESP32
    // Falls vorher passiv (LEDC) aktiv war: sauber detach
    if (s_ledcChannel >= 0) {
      ledcWriteTone((uint8_t)s_ledcChannel, 0);
       s_buzInitialized = true;
    #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
      // ESP32 core v3: detach am Pin (wichtig!)
      ledcDetach((uint8_t)BUZ_PIN);
    #else
      // core v2: detach Pin vom Kanal (wichtig!)
      if (BUZ_PIN >= 0) ledcDetachPin((uint8_t)BUZ_PIN);
    #endif
      s_ledcChannel = -1;
    }
  #else
    // AVR/andere: tone sicher aus
    if (BUZ_PIN >= 0) noTone((uint8_t)BUZ_PIN);
  #endif

    // Button-Events/Click-State hart resetten (gegen Phantom-Events nach ReInit)
    gpiohw_reset_click_state();
  // --- Button aus CONFIG übernehmen (falls vorhanden); sonst Defaults ---
  #ifdef CONFIG_HAS_GPIO
   
    CFG_BTN_PULLUP      = CONFIGV2.button.pullup;
    CFG_BTN_DEBOUNCE_MS = CONFIGV2.button.debounceMs;
    CFG_BTN_LONG_MS     = CONFIGV2.button.longMs;
    CFG_BTN_DOUBLE_MS   = CONFIGV2.button.doubleGapMs;
    CFG_BTN_HOLD_MS     = CONFIGV2.button.holdRepeatMs;

   
    CFG_BUZ_ACTIVE_HIGH    = CONFIGV2.buzzer.activeHigh;
    CFG_BUZ_PASSIVE        = CONFIGV2.buzzer.passive;
    CFG_BUZ_FREQ_HZ        = CONFIGV2.buzzer.freqHz;
    CFG_BUZ_SINGLE_ON_MS   = CONFIGV2.buzzer.singleMs;
    CFG_BUZ_DOUBLE_ON_MS   = CONFIGV2.buzzer.doubleOnMs;
    CFG_BUZ_DOUBLE_GAP_MS  = CONFIGV2.buzzer.doubleGapMs;
    CFG_BUZ_ERR_ON_MS      = CONFIGV2.buzzer.errorOnMs;
    CFG_BUZ_ERR_GAP_MS     = CONFIGV2.buzzer.errorGapMs;
    CFG_BUZ_ERR_COUNT      = CONFIGV2.buzzer.errorCount;
  #endif

  // --- Button einrichten ---
  s_btnEnabled   = CONFIGV2.button.enabled || true; // Default: aktiv
  s_btnActiveLow = CFG_BTN_PULLUP;

  if (s_btnEnabled) {
    pinMode(BTN_PIN, CFG_BTN_PULLUP ? INPUT_PULLUP : INPUT);

    s_btnRawLast     = s_btnActiveLow ? (digitalRead(BTN_PIN) == LOW)
                                      : (digitalRead(BTN_PIN) == HIGH);
    s_btnStable      = s_btnRawLast;
    s_btnPrevStable  = s_btnRawLast;
    s_btnChangeTs    = millis();

    s_pressStartTs   = 0;
    s_longFired      = false;
    s_doubleArmed    = false;
    s_shortCandidate = false;

    s_evShort = s_evLong = s_evDouble = s_evHold = false;
    s_evTapRelease = false;
        // Wenn Button beim Init gerade gedrückt ist -> Events blocken bis Release
    if (s_btnStable) {
      s_pressStartTs = 0;
      s_longFired = true;
      s_doubleArmed = false;
      s_shortCandidate = false;
    }

    
  }

  // --- Buzzer einrichten ---
  s_buzEnabled = CONFIGV2.buzzer.enabled || true; // Default: aktiv
  if (s_buzEnabled) {
    // Sicherheits-AUS nach (Re-)Init (verhindert "spinnt nach Import")
    s_buzLen = 0;
    s_buzPos = 0;
    s_buzStepUntil = 0;
    buzzer_output(false);

  #ifdef ARDUINO_ARCH_ESP32
    if (CFG_BUZ_PASSIVE) {
    #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
      s_ledcChannel = ledcAttach((uint8_t)BUZ_PIN, (uint32_t)CFG_BUZ_FREQ_HZ, (uint8_t)LEDC_BITS);
    #else
      s_ledcChannel = LEDC_CH;
      ledcSetup(s_ledcChannel, (double)CFG_BUZ_FREQ_HZ, LEDC_BITS);
      ledcAttachPin((uint8_t)BUZ_PIN, s_ledcChannel);
    #endif
      if (s_ledcChannel >= 0) ledcWriteTone((uint8_t)s_ledcChannel, 0); // sicher aus
    } else {
      pinMode(BUZ_PIN, OUTPUT);
      digitalWrite(BUZ_PIN, CFG_BUZ_ACTIVE_HIGH ? LOW : HIGH); // AUS
      s_buzInitialized = true;
    }
  #else
    pinMode(BUZ_PIN, OUTPUT);
    if (CFG_BUZ_PASSIVE) noTone((uint8_t)BUZ_PIN);
    else                 digitalWrite(BUZ_PIN, CFG_BUZ_ACTIVE_HIGH ? LOW : HIGH);
  #endif
  }

  GDBG("init: btnPin=%d pullup=%d buzPin=%d passive=%d freq=%dHz ch=%d\n",
       BTN_PIN, (int)CFG_BTN_PULLUP, BUZ_PIN, (int)CFG_BUZ_PASSIVE, CFG_BUZ_FREQ_HZ,
  #ifdef ARDUINO_ARCH_ESP32
       s_ledcChannel
  #else
       -1
  #endif
  );
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
    const bool raw = s_btnActiveLow ? (digitalRead(BTN_PIN) == LOW)
                                    : (digitalRead(BTN_PIN) == HIGH);

    if (raw != s_btnRawLast) {
      s_btnRawLast  = raw;
      s_btnChangeTs = now; // Beginn einer potenziellen Flanke
    }

    // Entprellung: Zustand erst übernehmen, wenn stabil > debounce
    if ( (now - s_btnChangeTs) >= (unsigned long)CFG_BTN_DEBOUNCE_MS && raw != s_btnStable ) {
      s_btnPrevStable = s_btnStable;
      s_btnStable     = raw;

      if (s_btnStable) {
        // ---------------- Rising (gedrückt) ----------------
        s_pressStartTs   = now;
        s_longFired      = false;
        s_lastHoldTick   = now;

        // <<< FIX 1: Reste aus einem vorigen Short/Double _sofort_ verwerfen
        //            (verhindert „jeder zweite Versuch“)
        s_doubleArmed    = false;
        s_shortCandidate = false;
        s_evTapRelease   = false; // altes Tap-Release sicher löschen

      } else {
        // ---------------- Falling (losgelassen) ----------------
        if (!s_longFired) {
          // <<< sofortiges Release-Event setzen (für "Cancel now")
          s_evTapRelease = true;

          // vorhandene Short/Double-Logik beibehalten
          if (s_doubleArmed && now <= s_doubleUntilTs) {
            s_doubleArmed    = false;
            s_shortCandidate = false;
            s_evDouble       = true;
          } else {
            s_shortCandidate = true;
            s_doubleArmed    = true;
            s_doubleUntilTs  = now + CFG_BTN_DOUBLE_MS;
          }
        }
        s_pressStartTs = 0; // Baseline löschen
      }
    }

    // Long / Hold während gedrückt
    if (s_btnStable) {
      if (s_pressStartTs != 0 &&
          !s_longFired &&
          (now - s_pressStartTs) >= (unsigned long)CFG_BTN_LONG_MS) {
        s_longFired      = true;
        s_doubleArmed    = false;       // Long verdrängt Double
        s_shortCandidate = false;
        s_evLong         = true;
        s_lastHoldTick   = now;
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
      ++s_buzPos;
      if (s_buzPos >= s_buzLen) {
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
bool button_short_press()   { bool v = s_evShort;      s_evShort = false;      return v; }
bool button_double_press()  { bool v = s_evDouble;     s_evDouble = false;     return v; }
bool button_long_press()    { bool v = s_evLong;       s_evLong = false;       return v; }
bool button_hold()          { bool v = s_evHold;       s_evHold = false;       return v; }
bool button_tap_release()   { bool v = s_evTapRelease; s_evTapRelease = false; return v; } // sofort beim Loslassen

// ============================================================================
// Public: Click-Logik hart zurücksetzen (Quality-of-Life für Cancel)
// ============================================================================
void gpiohw_reset_click_state() {
  // <<< FIX 2: Alles, was einen Folge-Long stören könnte, wird gelöscht
  s_doubleArmed    = false;
  s_shortCandidate = false;
  s_evShort        = false;
  s_evDouble       = false;
  s_evTapRelease   = false;
  s_evHold         = false;
  s_evLong         = false;
  s_pressStartTs   = 0;
  s_longFired      = false;
  // s_btnStable bleibt unverändert (echter physischer Zustand)
}

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
