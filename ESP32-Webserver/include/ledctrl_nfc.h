#pragma once
#include <Arduino.h>
#include "led_config.h"

// Note: a forward declaration is sufficient for the pointer type.
// (The actual Adafruit_NeoPixel header is included in the .cpp.)
class Adafruit_NeoPixel;

// ============================================================================
// Public parameters affected by the configuration
// These variables are defined in ledctrl_nfc.cpp and loaded through loadNfcLedConfig()
// from /config_v2.json. Fallbacks are also defined there.
// ============================================================================

extern int           NFC_LED_COUNT;          // Number of LEDs on the NFC strip
extern int           NFC_LED_BRIGHTNESS;     // 0..255 (clamped internally)
extern unsigned long NFC_LED_TIMEOUT;        // ms, timeout after tag removal

extern uint32_t NFC_LED_COLOR_SUCCESS;       // 0xRRGGBB - color for "Success"
extern uint32_t NFC_LED_COLOR_ERROR;         // 0xRRGGBB - color for "Error"
extern uint32_t NFC_LED_COLOR_PULSE;         // 0xRRGGBB - idle breathing color

extern bool     NFC_LED_SUCCESS_BLINK_ENABLED; // true = Success blinks initially
extern uint8_t  NFC_LED_SUCCESS_BLINK_COUNT;   // Number of on/off changes (0 = no blinking)
extern uint16_t NFC_LED_SUCCESS_BLINK_MS;      // Blink interval in ms (min. 25 ms)


// ============================================================================
// LEDCTRL_NFC - controller for the NFC LED strip
// - Zustandsautomat mit Idle-Breath, Success (Blink → Solid), Error (Solid)
// - Timeout starts only after the NFC tag is removed (presence tracking)
// - Reassert/Refresh gegen RMT/Glitches
// - Thread-sicheres show() via neopixel_guard
// Optionales Debug (Build-Flag -DLED_NFC_DEBUG) mit kompakten Logs.
// ============================================================================
class LEDCTRL_NFC {
public:
  // --------------------------------------------------------------------------
  /**
  * @brief Initializes the strip and resets internal state.
  * @param count         Number of pixels
  * @param timeout_ms    Timeout in milliseconds (starts after tag removal)
  * @param brightness    Brightness [0..255]
  * @param colorSuccess  Default color 0xRRGGBB
  * @param colorError    Error color 0xRRGGBB
  * @param colorPulse    Idle pulse color 0xRRGGBB
  * @param successBlinkEnabled  True = Success blinks initially
  * @param successBlinkCount    Number of blink cycles
  * @param successBlinkMs       Blink interval in ms
  * @param ledType              NeoPixel type
   */
  // --------------------------------------------------------------------------
  static void init(int count, int timeout_ms, int brightness, uint32_t colorSuccess, uint32_t colorError, uint32_t colorPulse,
                   bool successBlinkEnabled, int successBlinkCount, int successBlinkMs, neoPixelType pixelType);

  // --------------------------------------------------------------------------
  // Must be called periodically from loop().
  // Waits/blinks/refreshes depending on the current state.
  // --------------------------------------------------------------------------
  static void update();

  // --------------------------------------------------------------------------
  // Presence tracking (called by the NFC layer).
  // present=true: tag seen -> timeout is reset
  // present=false: tag not currently seen; grace logic is in the .cpp
  // --------------------------------------------------------------------------
  static void tagPresenceTick(bool present);

  // --------------------------------------------------------------------------
  // Ergebnis-Trigger nach erfolgreicher/fehlgeschlagener UID-Verarbeitung.
  // confirmSuccess(): optionales Blink → Solid Success (mit Reassert)
  // confirmError()  : sofort Solid Error (mit Reassert)
  // --------------------------------------------------------------------------
  static void confirmSuccess();
  static void confirmError();

  // Backward-compatible wrapper names
  static void showSuccess();
  static void showError();

  // --------------------------------------------------------------------------
  // Turns all LEDs off and enters idle breathing.
  // (Resets state/timer accordingly.)
  // --------------------------------------------------------------------------
  static void allOff();

  // --------------------------------------------------------------------------
  // Sets an individual pixel (Color(r,g,b) expected).
  // Rarely needed; the controller normally operates state-driven.
  // --------------------------------------------------------------------------
  static void setPixel(int index, uint32_t color);

  // --------------------------------------------------------------------------
  // True, wenn der Controller im Idle-State (Breath) ist.
  // Praktisch um z. B. Display-Idle mit den LEDs zu synchronisieren.
  // --------------------------------------------------------------------------
  static bool isIdle();

  // --------------------------------------------------------------------------
  // Access to the internal NeoPixel strip (read-only/forwarding).
  // Caution: only for special cases (e.g. composition with a second strip).
  // The controller handles the actual rendering.
  // --------------------------------------------------------------------------
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


  static void standBy(bool state);
  static bool isStandby();

private:
  // Internal pointer to the NeoPixel strip (lifetime managed by init()/allOff())
  static Adafruit_NeoPixel* _leds;

  // FIX: Also pause idle frames during network load (HTTP/WS)
  static unsigned long      s_netPauseUntil; // until when to suppress idle


  static bool _standby;
};
