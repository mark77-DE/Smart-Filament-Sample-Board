#pragma once
#include <Arduino.h>

// Vorwärtsdeklaration reicht für Zeiger-Typ
class Adafruit_NeoPixel;

// Öffentliche Konfig
extern int           NFC_LED_COUNT;
extern int           NFC_LED_PIN;
extern int           NFC_LED_BRIGHTNESS;
extern unsigned long NFC_LED_TIMEOUT;

extern uint32_t NFC_LED_COLOR_SUCCESS; // 0xRRGGBB
extern uint32_t NFC_LED_COLOR_ERROR;   // 0xRRGGBB
extern uint32_t NFC_LED_COLOR_PULSE;   // 0xRRGGBB

extern bool     NFC_LED_SUCCESS_BLINK_ENABLED;
extern uint8_t  NFC_LED_SUCCESS_BLINK_COUNT;
extern uint16_t NFC_LED_SUCCESS_BLINK_MS;

// Config laden + init
void loadNfcLedConfig();

class LEDCTRL_NFC {
public:
  static void init(int count, int pin, int timeout_ms, int brightness);
  static void update();

  // Präsenz-Tracking (keine LED-Aktion!)
  static void tagPresenceTick(bool present);

  // Ergebnis-Trigger
  static void confirmSuccess();
  static void confirmError();

  // Backwards-Compat Wrapper
  static void showSuccess();
  static void showError();

  // Alles aus (wechselt auf Idle-Pulse)
  static void allOff();

  static void setPixel(int index, uint32_t color); // erwartet Color(r,g,b)

  // Query für OFF/Idle
  static bool isIdle();

  // Nur für die eigene Implementierung (Helper im .cpp):
  static Adafruit_NeoPixel* rawStrip(); // Zugriff auf den internen Strip

private:
  static Adafruit_NeoPixel* _leds;
};
