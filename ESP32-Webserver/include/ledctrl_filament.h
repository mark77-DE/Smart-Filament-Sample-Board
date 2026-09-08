#pragma once
#include <Arduino.h>
#include "led_config.h"

// A forward declaration is sufficient here (the actual header is included in the .cpp)
class Adafruit_NeoPixel;

// ============================================================================
// Public configuration variables
//  -> loaded by loadLedConfigV2() from /config_v2.json (or /filament_default.json)
//     and then applied by LEDCTRL_FILAMENT::init(...)
// ============================================================================
extern int      LED_COUNT;        // Number of pixels in the filament strip
extern int      LED_TIMEOUT;      // Timeout in ms (starts after tag removal)
extern int      LED_BRIGHTNESS;   // Brightness [0..255]

// Default color for "normal" pixels (e.g. setPixel)
extern uint32_t LED_COLOR;        // 0xRRGGBB

// Dedicated error color (for errorBlink/errorAll)
extern uint32_t LED_COLOR_ERROR;  // 0xRRGGBB

// Color for the idle breathing pulse
extern uint32_t LED_COLOR_PULSE;  // 0xRRGGBB

// ============================================================================
// LEDCTRL_FILAMENT – Steuerung für den Filament-LED-Strip
//  - Supports setting individual pixels, error blink/solid, and idle breathing
//  - Timeout handling is coupled to NFC presence events (tagPresenceTick)
// ============================================================================
class LEDCTRL_FILAMENT {
public:
  // --------------------------------------------------------------------------
  // Lebenszyklus / Ticking
  // --------------------------------------------------------------------------

  /**
  * @brief Initializes the strip and resets internal state.
  * @param count       Number of pixels
  * @param timeout_ms  Timeout in milliseconds (starts after tag removal)
  * @param brightness  Brightness [0..255]
  * @param color       Default color 0xRRGGBB
  * @param colorError  Error color 0xRRGGBB
  * @param colorPulse  Idle pulse color 0xRRGGBB
  * @param type        NeoPixel type (e.g. NEO_GRBW + NEO_KHZ800)
   */
  static void init(int count, int timeout_ms, int brightness, uint32_t color, uint32_t colorError, uint32_t colorPulse, neoPixelType pixelType);

  /**
  * @brief Call periodically from the main loop.
  *        Updates blink/solid/idle animations and timeout logic.
   */
  static void update();

  // --------------------------------------------------------------------------
  // Presence tracking (coupled to NFC tag hold/removal)
  // --------------------------------------------------------------------------

  /**
  * @brief Reports NFC tag presence (true = tag present, false = removed).
  *        The timeout starts only after the tag has been removed
  *        (allowing for a short grace period).
   */
  static void tagPresenceTick(bool present);

  // --------------------------------------------------------------------------
  // Direkte Anzeige
  // --------------------------------------------------------------------------

  /**
  * @brief Sets an individual pixel to the requested NeoPixel color.
  *        Ends active error displays (blink/solid) and clears residual state if needed.
   * @param index  Pixelindex [0..count-1]
   * @param color  0x00RRGGBB im NeoPixel-Format des Strips
   */
  static void setPixel(int index, uint32_t color);

  /**
  * @brief Turns off all pixels, clears the internal buffer, and switches to the idle pulse.
   */
  static void allOff();

  // --------------------------------------------------------------------------
  // Fehleranzeigen
  // --------------------------------------------------------------------------

  /**
   * @brief Sofort alle Pixel in LED_COLOR_SUCCESS setzen (Solid).
  *        Timeout starts only after tag removal.
   */
  static void successAll();

  /**
   * @brief Erst LED_COLOR_ERROR blinken lassen, danach – sofern noch aktiv –
  *        transitions to errorAll() (solid).
   */
  static void successBlink();

  /**
   * @brief Sofort alle Pixel in LED_COLOR_ERROR setzen (Solid).
  *        Timeout starts only after tag removal.
   */
  static void errorAll();

  /**
   * @brief Erst LED_COLOR_ERROR blinken lassen, danach – sofern noch aktiv –
  *        transitions to errorAll() (solid).
   */
  static void errorBlink();

  // --------------------------------------------------------------------------
  // Status
  // --------------------------------------------------------------------------

  /**
   * @brief True, wenn keine Error-Anzeige aktiv ist und der Buffer dunkel ist.
  *        (The idle breathing pulse may then run.)
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
  * @brief Notification from the web server/WS that the network is busy.
  *        Pauses IDLE frames for the next @p ms milliseconds.
   *        Transitions (Blink/Solid/Reassert) bleiben unbeeinflusst.
   */
  static void netBusyHint(uint16_t ms); // FIX: added

  // WebIF: virtuellen "Tag-Hold" starten, damit Timeout/Idle wieder greifen
  static void webifHoldFor(uint16_t ms);

  

   /**
    * @brief Alle Anzeigen ausschalten und in einen passiven Standby-Zustand wechseln.
    *        (Currently: disable idle pulses so the LEDs do not continuously
    *        turn on and off during extended inactivity.)
    */
    static void standBy(bool state);
    
    static bool _standby;            // Standby state active?
    static bool _idlePulseEnabled;   // Idle pulse active?

private:
  // --------------------------------------------------------------------------
  // Hardware / Buffer
  // --------------------------------------------------------------------------
  static Adafruit_NeoPixel* _leds;      // eigener NeoPixel-Strip
  static uint32_t*          _buf;       // Shadow-Buffer (pro Pixel-Farbe)
  static int                _bufCount;  // Number of pixels (size of _buf)

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
  static const uint16_t     TAG_HELD_GRACE_MS;   // "Sticky" protection against short gaps

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
  static unsigned long      _netPauseUntil;      // until when to suppress idle

  // --------------------------------------------------------------------------
  // Buffer-Helfer
  // --------------------------------------------------------------------------
  static void ensureBuf(int n);                       // Buffer (re)alloziieren
  static void renderAllFromBuf(Adafruit_NeoPixel* s); // Transfer buffer -> strip
  static bool bufAnyLit();      
  
  
 
  
};
