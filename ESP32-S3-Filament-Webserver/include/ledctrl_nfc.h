#pragma once
#include <Adafruit_NeoPixel.h>

extern int NFC_LED_COUNT;
extern int NFC_LED_PIN;
extern int NFC_LED_BRIGHTNESS;
extern uint32_t NFC_LED_COLOR;

class LEDCTRL_NFC {
public:
    static void init(int count, int pin);
    static void setPixel(int index, uint32_t color);
    static void allOff();
    static Adafruit_NeoPixel* _leds;
};

void loadNfcLedConfig();
