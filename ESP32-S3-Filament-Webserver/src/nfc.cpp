#include "nfc.h"
#include "ledctrl_nfc.h"
#include "ledctrl_filament.h"

// ============================================================================
// Debug
// ============================================================================
#ifdef NFC_DEBUG
  #define DBG(...) do { Serial.printf("[NFC][t=%lu] ", millis()); Serial.printf(__VA_ARGS__); } while (0)
#else
  #define DBG(...) do {} while (0)
#endif

// ============================================================================
// Schwache Default-Hooks (können in der Applikation überschrieben werden)
// ============================================================================
__attribute__((weak)) void NFC_OnPreempt(const String&) { }
__attribute__((weak)) void NFC_OnActive() { }

// ============================================================================
// Lokaler PN532-Zugriff
// ============================================================================
static Adafruit_PN532* _nfc = nullptr;

// Quelle des UID-Triggers (wird in handleUID genutzt, Definition liegt extern)
enum class UidSource : uint8_t;
extern void handleUID(const String& uidStr, UidSource src);

// ============================================================================
// Guards / State
// ============================================================================
// Edge-/Hold-Tracking
static bool          s_prevTagPresent  = false;   // Präsenz-Status des letzten Ticks
static bool          s_holdActive      = false;   // wir sind „im Hold“ (selbes Tag)
static String        s_holdUid;                   // letzte getriggerte UID (für Debounce in Idle)
static unsigned long s_lastTriggerMs   = 0;       // letzter handleUID()-Zeitpunkt
static unsigned long s_lastSeenMs      = 0;       // letzte Roh-Erkennung (ms)

// Sperre gegen Retrigger während Effekt läuft
static bool          s_lockActive      = false;   // blockt (same uid) retrigger bis LEDs idle

// Welche UID „besitzt“ aktuell den LED-Controller (solange nicht idle)?
static String        s_busyUid;

// Debug-Throttle für Roh-Logs
static bool          s_prevRaw         = false;
static unsigned long s_lastRaw1LogMs   = 0;
static constexpr uint16_t RAW1_PERIOD_MS        = 300; // min. alle 300 ms „raw=1“-Log

// ============================================================================
// Tuning-Parameter
// ============================================================================
// Unterdrückt Doppel-Trigger derselben UID, wenn die LEDs idle sind
// (z. B. direkt nach einem Timeout).
static constexpr uint16_t RETRIGGER_DEBOUNCE_MS = 300;

// „Klebezeit“ gegen kurze Erkennungslücken, damit ein gehaltenes Tag
// nicht ständig falling/rising erzeugt.
static constexpr uint16_t HOLD_GRACE_MS         = 300;

// Preemption-Schutz: Neues Tag darf einen laufenden Effekt nur überfahren,
// wenn seit dem letzten Trigger mindestens diese Zeit vergangen ist.
static constexpr uint16_t PREEMPT_MIN_GAP_MS    = 350;

namespace NFC {

// ============================================================================
// Initialisierung der PN532-Hardware
// ============================================================================
void init(Adafruit_PN532* nfc) {
  _nfc = nfc;
  _nfc->begin();

  const uint32_t version = _nfc->getFirmwareVersion();
  if (!version) {
    Serial.println(F("[NFC] getFirmwareVersion FAILED (wiring?)"));
  } else {
    Serial.print(F("[NFC] PN532 FW ")); Serial.print((version >> 24) & 0xFF);
    Serial.print('.');                  Serial.print((version >> 16) & 0xFF);
    Serial.print(F(" chip=0x"));        Serial.println(version & 0xFFFF, HEX);
  }

  // Normalmodus
  _nfc->SAMConfig();
  Serial.println(F("[NFC] init done"));
#ifdef NFC_DEBUG
  DBG("Debug enabled\n");
#endif
}

// ============================================================================
// Einmaliger Block-Leser (Debug/Tools): Gibt UID als String zurück oder "".
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
// Interner Helper: Hold-Zustand beenden
// ============================================================================
static inline void onHoldEnded() {
  s_holdActive     = false;
  s_holdUid        = String();
  s_lastTriggerMs  = 0;
  DBG("HoldEnded\n");
}

// ============================================================================
// Reset aller Guards (z. B. bei globalem Reset/Neustart sinnvoll)
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
// Nicht-blockierender NFC-Poll + Guards + Trigger-Entscheidung.
// - now            : aktuelle Zeit (millis())
// - isActive       : wird auf true gesetzt, wenn ein Tag präsent ist
// - lastTagTime    : Zeitpunkt der letzten Präsenz (für äußere Timeouts)
// - tagPresentOut  : gibt den (gegraceten) Präsenzstatus an den Aufrufer zurück
// ============================================================================
void tick(unsigned long now,
          bool& isActive,
          unsigned long& lastTagTime,
          bool& tagPresentOut)
{
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
  // 2) Grace („Sticky Presence“) gegen kurze Lücken
  // --------------------------------------------------------------------------
  bool tagPresent = tagPresentRaw;
  if (!tagPresent && s_holdActive && (now - s_lastSeenMs) < HOLD_GRACE_MS) {
    tagPresent = true;
    DBG("graceHold (dt=%lu < %u)\n", now - s_lastSeenMs, (unsigned)HOLD_GRACE_MS);
  }

  // --------------------------------------------------------------------------
  // 3) Präsenz-Information zuerst an den LED-Controller (steuert den Timeout)
  // --------------------------------------------------------------------------
  LEDCTRL_NFC::tagPresenceTick(tagPresent);

  // 3.1) Lock & busyUid freigeben, sobald LEDs idle sind
  if (!LEDCTRL_NFC::isIdle()) {
    // Effekt läuft -> busy bleibt gesetzt
  } else {
    if (s_lockActive || s_busyUid.length()) {
      s_lockActive = false;
      s_busyUid    = String();
      DBG("unlock (led idle)\n");
    }
  }

  // --------------------------------------------------------------------------
  // 4) Rising-Edge: jetzt entscheiden, ob wir handleUID() auslösen
  // --------------------------------------------------------------------------
  if (tagPresent && !s_prevTagPresent) {
    
    LEDCTRL_FILAMENT::standBy(false);
    LEDCTRL_NFC::standBy(false);

    const bool ledIdle   = LEDCTRL_NFC::isIdle();
    const bool haveFresh = tagPresentRaw; // nur mit frischer UID triggern

    // LEDs NICHT idle → Preemption-Logik (neue UID darf evtl. „überfahren“)
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

    // LEDs idle → regulärer Trigger-Pfad
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
  // 5) Keep-Alive für äußere Logik (Display, etc.)
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

} // namespace NFC
