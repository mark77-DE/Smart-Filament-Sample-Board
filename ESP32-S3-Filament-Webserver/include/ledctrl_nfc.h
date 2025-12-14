#pragma once
#include <Adafruit_NeoPixel.h>

extern int NFC_LED_COUNT;
extern int NFC_LED_PIN;
extern int NFC_LED_BRIGHTNESS;
extern unsigned long NFC_LED_TIMEOUT;
extern uint32_t NFC_LED_COLOR_SUCCESS;
extern uint32_t NFC_LED_COLOR_ERROR;

class LEDCTRL_NFC {
public:
    static void init(int count, int pin, int timeout, int brightness);
    static void setPixel(int index, uint32_t color);
    static void allOff();

    // Non-blocking LED-Funktionen
    static void showSuccess();
    static void showError();
    static void update();  // muss regelmäßig im loop() aufgerufen werden

    static Adafruit_NeoPixel* _leds;
};

// Config laden
void loadNfcLedConfig();
