#pragma once
#include <Arduino.h>

// Hinweis: Für den Zeiger-Typ reicht eine Vorwärtsdeklaration.
// (Die eigentliche Header-Datei von Adafruit_NeoPixel wird im .cpp inkludiert.)
class Adafruit_NeoPixel;

// ============================================================================
// Öffentliche, von der Config beeinflusste Parameter
// Diese Variablen werden in ledctrl_nfc.cpp definiert und über loadNfcLedConfig()
// aus /config.json eingelesen. Fallbacks sind dort ebenfalls hinterlegt.
// ============================================================================

extern int           NFC_LED_COUNT;          // Anzahl LEDs am NFC-Stripe
extern int           NFC_LED_PIN;            // Daten-Pin des NFC-Stripe
extern int           NFC_LED_BRIGHTNESS;     // 0..255 (wird intern geklemmt)
extern unsigned long NFC_LED_TIMEOUT;        // ms, Timeout nach Tag-Entfernung

extern uint32_t NFC_LED_COLOR_SUCCESS;       // 0xRRGGBB – Farbe für „Success“
extern uint32_t NFC_LED_COLOR_ERROR;         // 0xRRGGBB – Farbe für „Error“
extern uint32_t NFC_LED_COLOR_PULSE;         // 0xRRGGBB – Idle-Breath-Farbe

extern bool     NFC_LED_SUCCESS_BLINK_ENABLED; // true = Success blinkt zunächst
extern uint8_t  NFC_LED_SUCCESS_BLINK_COUNT;   // Anzahl An/Aus-Wechsel (0 = kein Blink)
extern uint16_t NFC_LED_SUCCESS_BLINK_MS;      // Blink-Intervall in ms (min. 25 ms)


// ============================================================================
// LEDCTRL_NFC – Controller für den NFC-LED-Streifen
// - Zustandsautomat mit Idle-Breath, Success (Blink → Solid), Error (Solid)
// - Timeout läuft erst, wenn das NFC-Tag entfernt wurde (Presence-Tracking)
// - Reassert/Refresh gegen RMT/Glitches
// - Thread-sicheres show() via neopixel_guard
// Optionales Debug (Build-Flag -DLED_NFC_DEBUG) mit kompakten Logs.
// ============================================================================
class LEDCTRL_NFC {
public:
  // --------------------------------------------------------------------------
  /**
   * @brief Strip initialisieren und internen Zustand zurücksetzen.
   * @param count         Anzahl Pixel
   * @param pin           GPIO-Pin
   * @param timeout_ms    Timeout in Millisekunden (wirkt erst ab Tag-Entfernung)
   * @param brightness    Helligkeit [0..255]
   * @param colorSuccess  Standardfarbe 0xRRGGBB
   * @param colorError    Fehlerfarbe 0xRRGGBB
   * @param colorPulse    Idle-Pulse-Farbe 0xRRGGBB
   * @param successBlinkEnabled  True = Success blinkt zunächst
   * @param successBlinkCount    Anzahl Blink-Zyklen
   * @param successBlinkMs       Blink-Intervall in ms
   */
  // --------------------------------------------------------------------------
  static void init(int count, int pin, int timeout_ms, int brightness, uint32_t colorSuccess, uint32_t colorError, uint32_t colorPulse,
                   bool successBlinkEnabled, int successBlinkCount, int successBlinkMs);

  // --------------------------------------------------------------------------
  // Muss zyklisch aus loop() aufgerufen werden.
  // Wartet/blinkt/refresh’t je nach aktuellem State.
  // --------------------------------------------------------------------------
  static void update();

  // --------------------------------------------------------------------------
  // Presence-Tracking (von der NFC-Schicht aufzurufen).
  // present=true : Tag gesehen → Timeout wird zurückgesetzt
  // present=false: Tag momentan nicht gesehen; Grace-Logik im .cpp
  // --------------------------------------------------------------------------
  static void tagPresenceTick(bool present);

  // --------------------------------------------------------------------------
  // Ergebnis-Trigger nach erfolgreicher/fehlgeschlagener UID-Verarbeitung.
  // confirmSuccess(): optionales Blink → Solid Success (mit Reassert)
  // confirmError()  : sofort Solid Error (mit Reassert)
  // --------------------------------------------------------------------------
  static void confirmSuccess();
  static void confirmError();

  // Rückwärtskompatible Wrapper-Namen
  static void showSuccess();
  static void showError();

  // --------------------------------------------------------------------------
  // Schaltet alle LEDs aus und geht in den Idle-Breath.
  // (Setzt State/Timer entsprechend zurück.)
  // --------------------------------------------------------------------------
  static void allOff();

  // --------------------------------------------------------------------------
  // Setzt einen einzelnen Pixel (Color(r,g,b) erwartet).
  // Wird selten benötigt; der Controller arbeitet normalerweise state-gesteuert.
  // --------------------------------------------------------------------------
  static void setPixel(int index, uint32_t color);

  // --------------------------------------------------------------------------
  // True, wenn der Controller im Idle-State (Breath) ist.
  // Praktisch um z. B. Display-Idle mit den LEDs zu synchronisieren.
  // --------------------------------------------------------------------------
  static bool isIdle();

  // --------------------------------------------------------------------------
  // Zugriff auf den internen NeoPixel-Strip (read-only/Weitergabe).
  // Achtung: Nur für spezielle Fälle (z. B. Composition mit zweitem Strip).
  // Das eigentliche Rendering steuert der Controller.
  // --------------------------------------------------------------------------
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

private:
  // Interner Pointer auf den NeoPixel-Strip (lebenszyklisch von init()/allOff() verwaltet)
  static Adafruit_NeoPixel* _leds;

  // FIX: Während Netzlast (HTTP/WS) zusätzlich Idle-Frames pausieren
  static unsigned long      s_netPauseUntil; // bis wann Idle unterdrücken
};
