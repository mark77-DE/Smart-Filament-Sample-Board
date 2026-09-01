#pragma once
#include <Arduino.h>
#include "led_config.h"

// Vorwärtsdeklaration genügt hier (die eigentliche Header-Datei wird in der .cpp inkludiert)
class Adafruit_NeoPixel;

// ============================================================================
// Öffentliche Konfig-Variablen
//  -> werden von loadLedConfigV2() aus /config_v2.json (oder /filament_default.json)
//     gelesen und anschließend per LEDCTRL_FILAMENT::init(...) angewendet
// ============================================================================
extern int      LED_COUNT;        // Anzahl Pixel des Filament-Strips
extern int      LED_TIMEOUT;      // Timeout in ms (erst ab Tag-Entfernung)
extern int      LED_BRIGHTNESS;   // Helligkeit [0..255]

// Standardfarbe für „normale“ Pixel (z. B. setPixel)
extern uint32_t LED_COLOR;        // 0xRRGGBB

// Eigenständige Fehlerfarbe (für errorBlink/errorAll)
extern uint32_t LED_COLOR_ERROR;  // 0xRRGGBB

// Farbe für den Idle-Breath-Pulse
extern uint32_t LED_COLOR_PULSE;  // 0xRRGGBB

// ============================================================================
// LEDCTRL_FILAMENT – Steuerung für den Filament-LED-Strip
//  - Unterstützt: Einzelpixel setzen, Fehler-Blink/Solid, Idle-Breath
//  - Timeout-Handling wird an NFC-Präsenz-Events gekoppelt (tagPresenceTick)
// ============================================================================
class LEDCTRL_FILAMENT {
public:
  // --------------------------------------------------------------------------
  // Lebenszyklus / Ticking
  // --------------------------------------------------------------------------

  /**
   * @brief Strip initialisieren und internen Zustand zurücksetzen.
   * @param count       Anzahl Pixel
   * @param timeout_ms  Timeout in Millisekunden (wirkt erst ab Tag-Entfernung)
   * @param brightness  Helligkeit [0..255]
   * @param color       Standardfarbe 0xRRGGBB
   * @param colorError  Fehlerfarbe 0xRRGGBB
   * @param colorPulse  Idle-Pulse-Farbe 0xRRGGBB
   * @param type        NeoPixel-Typ (z. B. NEO_GRBW + NEO_KHZ800)
   */
  static void init(int count, int timeout_ms, int brightness, uint32_t color, uint32_t colorError, uint32_t colorPulse, neoPixelType pixelType);

  /**
   * @brief In der main-Loop zyklisch aufrufen.
   *        Aktualisiert Blink/Solid/Idle-Animationen und Timeout-Logik.
   */
  static void update();

  // --------------------------------------------------------------------------
  // Präsenz-Tracking (kopplung an NFC-Tag-Halten/Entfernen)
  // --------------------------------------------------------------------------

  /**
   * @brief NFC-Tag-Präsenz melden (true = Tag da, false = entfernt).
   *        Der Timeout wird erst gestartet, wenn das Tag entfernt wurde
   *        (unter Berücksichtigung einer kleinen Grace-Zeit).
   */
  static void tagPresenceTick(bool present);

  // --------------------------------------------------------------------------
  // Direkte Anzeige
  // --------------------------------------------------------------------------

  /**
   * @brief Einzelnen Pixel in gewünschter NeoPixel-Farbe setzen.
   *        Beendet aktive Error-Anzeigen (Blink/Solid) und löscht ggf. Restzustände.
   * @param index  Pixelindex [0..count-1]
   * @param color  0x00RRGGBB im NeoPixel-Format des Strips
   */
  static void setPixel(int index, uint32_t color);

  /**
   * @brief Alle Pixel ausschalten, internen Buffer löschen und auf Idle-Pulse gehen.
   */
  static void allOff();

  // --------------------------------------------------------------------------
  // Fehleranzeigen
  // --------------------------------------------------------------------------

  /**
   * @brief Sofort alle Pixel in LED_COLOR_ERROR setzen (Solid).
   *        Timeout läuft erst nach Tag-Entfernung.
   */
  static void errorAll();

  /**
   * @brief Erst LED_COLOR_ERROR blinken lassen, danach – sofern noch aktiv –
   *        in errorAll() (Solid) übergehen.
   */
  static void errorBlink();

  // --------------------------------------------------------------------------
  // Status
  // --------------------------------------------------------------------------

  /**
   * @brief True, wenn keine Error-Anzeige aktiv ist und der Buffer dunkel ist.
   *        (Dann läuft ggf. der Idle-Breath-Pulse.)
   */
  static bool isIdle();

  /**
   * @brief Optionaler Zugriff auf den internen Strip (nur weiterreichen/lesen).
   */
  static Adafruit_NeoPixel* rawStrip();

  // --------------------------------------------------------------------------
  // Netzlast-Hinweis (Idle kurz pausieren)
  // --------------------------------------------------------------------------
  /**
   * @brief Hinweis vom Webserver/WS: Netzwerk ist gerade beschäftigt.
   *        Pausiert IDLE-Frames für die nächsten @p ms Millisekunden.
   *        Transitions (Blink/Solid/Reassert) bleiben unbeeinflusst.
   */
  static void netBusyHint(uint16_t ms); // FIX: hinzugefügt

  // WebIF: virtuellen "Tag-Hold" starten, damit Timeout/Idle wieder greifen
  static void webifHoldFor(uint16_t ms);

  

   /**
    * @brief Alle Anzeigen ausschalten und in einen passiven Standby-Zustand wechseln.
    *        (Derzeit: Idle-Pulse deaktivieren, damit bei längerer Inaktivität nicht
    *        ständig die LEDs an- und ausgehen.)
    */
    static void standBy(bool state);
    
    static bool _standby;            // Standby-Zustand aktiv?
    static bool _idlePulseEnabled;   // Idle-Pulse aktiv?

private:
  // --------------------------------------------------------------------------
  // Hardware / Buffer
  // --------------------------------------------------------------------------
  static Adafruit_NeoPixel* _leds;      // eigener NeoPixel-Strip
  static uint32_t*          _buf;       // Shadow-Buffer (pro Pixel-Farbe)
  static int                _bufCount;  // Anzahl Pixel (Größe von _buf)

  // --------------------------------------------------------------------------
  // Error-Blink-State (phasenbasiert)
  // --------------------------------------------------------------------------
  static bool               _errBlinkActive;
  static unsigned long      _errBlinkStart;
  static uint16_t           _errBlinkMs;
  static uint8_t            _errBlinkCount;
  static uint8_t            _errBlinkStep;

  // --------------------------------------------------------------------------
  // Error-Solid-State
  // --------------------------------------------------------------------------
  static bool               _errSolidActive;

  // --------------------------------------------------------------------------
  // Präsenz / Timeout
  // --------------------------------------------------------------------------
  static bool               _tagHeld;            // Tag physisch vor Ort (inkl. Grace)
  static unsigned long      _lastTagSeen;        // Zeitpunkt der letzten Roh-Erkennung
  static unsigned long      _releaseTs;          // 0 = kein Timeout aktiv, sonst Startzeit
  static const uint16_t     TAG_HELD_GRACE_MS;   // „Sticky“ gegen kurze Lücken

  // Reassert (gegen Glitches / halbe Frames)
  static unsigned long      _lastHoldRefresh;
  static const uint16_t     HOLD_REFRESH_MS;

  // --------------------------------------------------------------------------
  // Idle-Breath-Pulse
  // --------------------------------------------------------------------------
  
  static float              _minBrightness;      // Minimaler Helligkeitsfaktor [0..1]
  static unsigned long      _lastPulseUpdate;    // letzter Renderzeitpunkt
  static const uint16_t     PULSE_INTERVAL_MS;   // ~Frame-Intervall (z. B. 16 ms ≈ 60 FPS)
  static const uint16_t     BREATHS_PER_MIN;     // Atemfrequenz
  static uint8_t            _ditherPhase;        // Ordered-Dithering-Phase

  // Blockt den Idle-Pulse ganz kurz nach Umschaltungen (Frame-Trennung)
  static unsigned long      _idleBlockUntil;

  // FIX: Während Netzlast (HTTP/WS) zusätzlich Idle-Frames pausieren
  static unsigned long      _netPauseUntil;      // bis wann Idle unterdrücken

  // --------------------------------------------------------------------------
  // Buffer-Helfer
  // --------------------------------------------------------------------------
  static void ensureBuf(int n);                       // Buffer (re)alloziieren
  static void renderAllFromBuf(Adafruit_NeoPixel* s); // Buffer → Strip übertragen
  static bool bufAnyLit();      
  
  
 
  
};
