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
// LEDCTRL_FILAMENT – Filament LED strip control
//  - Supports setting individual pixels, error blink/solid, and idle breathing
//  - Timeout handling is coupled to NFC presence events (tagPresenceTick)
// ============================================================================
class LEDCTRL_FILAMENT {
public:
  // --------------------------------------------------------------------------
  // Lifecycle / ticking
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
  // Direct display
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
  // Error displays
  // --------------------------------------------------------------------------

  /**
  * @brief Immediately set all pixels to LED_COLOR_SUCCESS (solid).
  *        Timeout starts only after tag removal.
   */
  static void successAll();

  /**
  * @brief First blink LED_COLOR_ERROR, then – if still active –
  *        transition to errorAll() (solid).
   */
  static void successBlink();

  /**
  * @brief Immediately set all pixels to LED_COLOR_ERROR (solid).
  *        Timeout starts only after tag removal.
   */
  static void errorAll();

  /**
  * @brief First blink LED_COLOR_ERROR, then – if still active –
  *        transition to errorAll() (solid).
   */
  static void errorBlink();

  // --------------------------------------------------------------------------
  // Status
  // --------------------------------------------------------------------------

  /**
   * @brief True when no error display is active and the buffer is dark.
  *        (The idle breathing pulse may then run.)
   */
  static bool isIdle();

  /**
   * @brief Optional access to the internal strip (forward/read only).
   */
  static Adafruit_NeoPixel* rawStrip();

  // --------------------------------------------------------------------------
  // Network load hint (pause idle briefly)
  // --------------------------------------------------------------------------
  /**
  * @brief Notification from the web server/WS that the network is busy.
  *        Pauses IDLE frames for the next @p ms milliseconds.
  *        Transitions (blink/solid/reassert) remain unaffected.
  */
  static void netBusyHint(uint16_t ms); // FIX: added

  // WebIF: start a virtual "tag hold" so timeout/idle logic can resume

  

   /**
    * @brief Switch off all displays and enter passive standby mode.
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
  // Error blink state (phase-based)
  // --------------------------------------------------------------------------
  static bool               _errBlinkActive;
  static unsigned long      _errBlinkStart;
  static uint16_t           _errBlinkMs;
  static uint8_t            _errBlinkCount;
  static uint8_t            _errBlinkStep;

  // --------------------------------------------------------------------------
  // Error solid state
  // --------------------------------------------------------------------------
  static bool               _errSolidActive;

  // --------------------------------------------------------------------------
  // Presence / timeout
  // --------------------------------------------------------------------------
  static bool               _tagHeld;            // Tag physically present (including grace)
  static unsigned long      _lastTagSeen;        // timestamp of the last raw detection
  static unsigned long      _releaseTs;          // 0 = timeout inactive, otherwise start time
  static const uint16_t     TAG_HELD_GRACE_MS;   // "Sticky" protection against short gaps

  // Reassert (against glitches / half frames)
  static unsigned long      _lastHoldRefresh;
  static const uint16_t     HOLD_REFRESH_MS;

  // --------------------------------------------------------------------------
  // Idle breathing pulse
  // --------------------------------------------------------------------------
  
  static float              _minBrightness;      // minimum brightness factor [0..1]
  static unsigned long      _lastPulseUpdate;    // last render timestamp
  static const uint16_t     PULSE_INTERVAL_MS;   // ~frame interval (e.g. 16 ms ≈ 60 FPS)
  static const uint16_t     BREATHS_PER_MIN;     // breathing frequency
  static uint8_t            _ditherPhase;        // ordered-dithering phase

  // Briefly blocks the idle pulse after switching states (frame separation)
  static unsigned long      _idleBlockUntil;

  // FIX: also pause idle frames while the network is busy (HTTP/WS)
  static unsigned long      _netPauseUntil;      // until when to suppress idle

  // --------------------------------------------------------------------------
  // Buffer-Helfer
  // --------------------------------------------------------------------------
  static void ensureBuf(int n);                       // Buffer (re)alloziieren
  static void renderAllFromBuf(Adafruit_NeoPixel* s); // Transfer buffer -> strip
  static bool bufAnyLit();      
  
  
 
  
};
