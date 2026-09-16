#include "nfc.h"
#include "ledctrl_nfc.h"
#include "ledctrl_filament.h"
#include "config.h"

// ============================================================================
// Debug
// ============================================================================
#ifdef NFC_DEBUG
  #define DBG(...) do { Serial.printf("[NFC][t=%lu] ", millis()); Serial.printf(__VA_ARGS__); } while (0)
#else
  #define DBG(...) do {} while (0)
#endif

// ============================================================================
// Weak default hooks (can be overridden by the application)
// ============================================================================
__attribute__((weak)) void NFC_OnPreempt(const String&) { }
__attribute__((weak)) void NFC_OnActive() { }

// ============================================================================
// Local PN532 access
// ============================================================================
static Adafruit_PN532* _nfc = nullptr;

// Source of the UID trigger (used in handleUID, definition is external)
enum class UidSource : uint8_t;
extern void handleUID(const String& uidStr, UidSource src);

// ============================================================================
// Guards / State
// ============================================================================
// Edge-/Hold-Tracking
static bool          s_prevTagPresent  = false;   // Presence status of the previous tick
static bool          s_holdActive      = false;   // we are in the hold state (same tag)
static String        s_holdUid;                   // Last triggered UID (for idle debounce)
static unsigned long s_lastTriggerMs   = 0;       // Last handleUID() time
static unsigned long s_lastSeenMs      = 0;       // Last raw detection (ms)

// Block retriggering while an effect is running
static bool          s_lockActive      = false;   // blocks retrigger for the same UID until LEDs are idle

// Which UID currently "owns" the LED controller (while not idle)?
static String        s_busyUid;

// Debug throttle for raw logs
static bool          s_prevRaw         = false;
static unsigned long s_lastRaw1LogMs   = 0;
static constexpr uint16_t RAW1_PERIOD_MS        = 300; // min. alle 300 ms „raw=1“-Log

// ============================================================================
// Tuning-Parameter
// ============================================================================
// Suppress duplicate triggers for the same UID when LEDs are idle
// (e.g. immediately after a timeout).
static constexpr uint16_t RETRIGGER_DEBOUNCE_MS = 300;

// Sticky grace time against short detection gaps so a held tag does not keep
// toggling between falling and rising.
static constexpr uint16_t HOLD_GRACE_MS         = 300;

// Preemption protection: a new tag may only preempt an ongoing effect if at least
// this much time has elapsed since the last trigger.
static constexpr uint16_t PREEMPT_MIN_GAP_MS    = 350;

namespace NFC {

// ============================================================================
// Initialisierung der PN532-Hardware
// ============================================================================
uint32_t init(Adafruit_PN532* nfc) {
  _nfc = nfc;
  _nfc->begin();

  const uint32_t version = _nfc->getFirmwareVersion();
  if (!version) {
    Serial.println(F("[NFC] getFirmwareVersion FAILED (wiring?)"));
  } else if (CONFIGV2.system.debugMode) {
    Serial.print(F("[NFC] PN532 FW ")); Serial.print((version >> 24) & 0xFF);
    Serial.print('.');                  Serial.print((version >> 16) & 0xFF);
    Serial.print(F(" chip=0x"));        Serial.println(version & 0xFFFF, HEX);
  }

  // Normalmodus
  _nfc->SAMConfig();

  if(CONFIGV2.system.debugMode)
  {
      Serial.println(F("[NFC] init done"));
  }

#ifdef NFC_DEBUG
  DBG("Debug enabled\n");
#endif

  return version;
}

// ============================================================================
// One-time block reader (debug/tools): returns UID as a string or "".
// ============================================================================
String checkTag() {
  if (!_nfc) return "";
  uint8_t uid[7];
  uint8_t len = 0;

  if (_nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len)) {
    String s;
    for (uint8_t i = 0; i < len; i++) {
      if (i) s += ':';
      if (uid[i] < 0x10) s += '0';
      s += String(uid[i], HEX);
    }
    s.toUpperCase();
    Serial.print(F("[NFC] Found UID: ")); Serial.println(s);
    return s;
  }
  return "";
}

// ============================================================================
// Internal helper: end the hold state
// ============================================================================
static inline void onHoldEnded() {
  s_holdActive     = false;
  s_holdUid        = String();
  s_lastTriggerMs  = 0;
  DBG("HoldEnded\n");
}

// ============================================================================
// Reset all guards (useful for global resets / restarts)
// ============================================================================
void resetGuard() {
  s_prevTagPresent = false;
  s_lastSeenMs     = 0;
  s_prevRaw        = false;
  s_lastRaw1LogMs  = 0;
  s_lockActive     = false;
  s_busyUid        = String();
  onHoldEnded();
  DBG("resetGuard\n");
}

// ============================================================================
// tick(..)
// Non-blocking NFC poll + guards + trigger decision.
// - now            : current time (millis())
// - isActive       : set to true when a tag is present
// - lastTagTime    : timestamp of the last presence (for external timeouts)
// - tagPresentOut  : returns the grace-filtered presence status to the caller
// ============================================================================
static uint32_t lastPoll = 0;
void tick(unsigned long now,
          bool& isActive,
          unsigned long& lastTagTime,
          bool& tagPresentOut)
{

  
if (now - lastPoll < 50) return;
lastPoll = now;

  if (!_nfc) { tagPresentOut = false; return; }

  // --------------------------------------------------------------------------
  // 1) Roh lesen (non-blocking Pattern)
  // --------------------------------------------------------------------------
  bool     tagPresentRaw = false;
  uint8_t  uid[7]        = {0};
  uint8_t  uidLength     = 0;
  String   uidStr;

  _nfc->startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A);
  if (_nfc->readDetectedPassiveTargetID(uid, &uidLength) && uidLength > 0) {
    tagPresentRaw = true;

    uidStr.reserve(uidLength * 3);
    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10) uidStr += '0';
      uidStr += String(uid[i], HEX);
      if (i != uidLength - 1) uidStr += ':';
    }
    uidStr.toUpperCase();
    s_lastSeenMs = now;

    // raw=1 throttle: auf Flankenlog + periodisch
    if (!s_prevRaw) {
      DBG("raw=1 uid=%s (rise)\n", uidStr.c_str());
      s_lastRaw1LogMs = now;
    } else if (now - s_lastRaw1LogMs >= RAW1_PERIOD_MS) {
      DBG("raw=1 uid=%s\n", uidStr.c_str());
      s_lastRaw1LogMs = now;
    }
  } else {
    // raw=0 nur bei 1→0
    if (s_prevRaw) DBG("raw=0 (fall)\n");
  }
  s_prevRaw = tagPresentRaw;

  // --------------------------------------------------------------------------
  // 2) Grace ("sticky presence") against short gaps
  // --------------------------------------------------------------------------
  bool tagPresent = tagPresentRaw;
  if (!tagPresent && s_holdActive && (now - s_lastSeenMs) < HOLD_GRACE_MS) {
    tagPresent = true;
    DBG("graceHold (dt=%lu < %u)\n", now - s_lastSeenMs, (unsigned)HOLD_GRACE_MS);
  }

  // --------------------------------------------------------------------------
  // 3) Pass presence information to the LED controller first (controls the timeout)
  // --------------------------------------------------------------------------
  LEDCTRL_NFC::tagPresenceTick(tagPresent);

  // 3.1) Release lock & busyUid as soon as the LEDs are idle
  if (!LEDCTRL_NFC::isIdle()) {
    // effect is running -> busy remains set
  } else {
    if (s_lockActive || s_busyUid.length()) {
      s_lockActive = false;
      s_busyUid    = String();
      DBG("unlock (led idle)\n");
    }
  }

  // --------------------------------------------------------------------------
  // 4) Rising edge: decide whether to trigger handleUID()
  // --------------------------------------------------------------------------
  if (tagPresent && !s_prevTagPresent) {
    
    LEDCTRL_FILAMENT::standBy(false);
    LEDCTRL_NFC::standBy(false);

    const bool ledIdle   = LEDCTRL_NFC::isIdle();
    const bool haveFresh = tagPresentRaw; // nur mit frischer UID triggern

    // LEDs are not idle → preemption logic (new UID may override the current effect)
    if (!ledIdle) {
      if (haveFresh) {
        if (uidStr == s_busyUid) {
          DBG("RISING ignored (led not idle, same busy uid)\n");
        } else {
          const bool gapOk = (now - s_lastTriggerMs) >= PREEMPT_MIN_GAP_MS;
          if (!gapOk) {
            DBG("RISING new uid=%s but preempt blocked (gap %lums < %u)\n",
                uidStr.c_str(), now - s_lastTriggerMs, (unsigned)PREEMPT_MIN_GAP_MS);
          } else {
            DBG("RISING PREEMPT new uid=%s (led not idle)\n", uidStr.c_str());
            NFC_OnPreempt(uidStr);
            handleUID(uidStr, (UidSource)0 /* NFC */);

            s_holdActive    = true;
            s_holdUid       = uidStr;
            s_busyUid       = uidStr;        // << neuer „Besitzer“ des Effekts
            s_lastTriggerMs = now;
            s_lockActive    = true;          // gegen „same uid“ retrigger

            // Der SAMConfig()-Call wird im Projekt oft als „sanfter Kick“ genutzt,
            // um den Reader in den gewohnten Pollingzustand zu versetzen.
            _nfc->SAMConfig();

            lastTagTime = now;
            isActive    = true;
          }
        }
      } else {
        DBG("RISING ignored (led not idle, no fresh uid)\n");
      }

      s_prevTagPresent = tagPresent;
      tagPresentOut    = tagPresent;
      return;
    }

    // LEDs idle → normal trigger path
    if (haveFresh) {
      const bool sameHoldSameUid = s_holdActive && (s_holdUid == uidStr);
      const bool tooFast         = (now - s_lastTriggerMs) < RETRIGGER_DEBOUNCE_MS;

      if (sameHoldSameUid && tooFast) {
        DBG("debounce: skip (same uid, %lums)\n", now - s_lastTriggerMs);
      } else {
        DBG("RISING uid=%s dtSinceLastTrig=%lu\n", uidStr.c_str(), now - s_lastTriggerMs);
        handleUID(uidStr, (UidSource)0 /* NFC */);

        s_holdActive    = true;
        s_holdUid       = uidStr;
        s_busyUid       = uidStr;          // << Besitzer setzen (bis idle)
        s_lastTriggerMs = now;
        s_lockActive    = true;

        _nfc->SAMConfig();

        lastTagTime = now;
        isActive    = true;
        DBG("handleUID fired\n");
      }
    } else {
      DBG("RISING by grace (no fresh UID) -> no trigger\n");
    }
  }

  // --------------------------------------------------------------------------
  // 5) Keep-alive for external logic (display, etc.)
  // --------------------------------------------------------------------------
  if (tagPresent) {
    lastTagTime = now;
    isActive    = true;
    NFC_OnActive();
  }

  // --------------------------------------------------------------------------
  // 6) Falling-Edge → Hold & Debounce freigeben
  // --------------------------------------------------------------------------
  if (!tagPresent && s_prevTagPresent) {
    DBG("FALLING (dtSinceLastSeen=%lu)\n", now - s_lastSeenMs);
    onHoldEnded();
  }

  // --------------------------------------------------------------------------
  // 7) Abschluss
  // --------------------------------------------------------------------------
  s_prevTagPresent = tagPresent;
  tagPresentOut    = tagPresent;
}


String currentHoldUid() {
  return s_holdUid;
}

} // namespace NFC
