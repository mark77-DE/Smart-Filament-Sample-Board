#pragma once
#include <Adafruit_NeoPixel.h>

extern int LED_COUNT;
extern int LED_PIN;
extern int LED_TIMEOUT;
extern int LED_BRIGHTNESS;
extern uint32_t LED_COLOR; // global

class LEDCTRL {
public:
    static void init(int count, int pin, int timeout, int brightness);       // LED Strip initialisieren
    static void setPixel(int index, uint32_t color); // Pixel setzen
    static void allOff();                        // alle LEDs aus
    static Adafruit_NeoPixel* _leds;            // Pointer auf den Strip
};

// --- neue Deklaration ---
void loadLedConfig();
extern int LED_BRIGHTNESS;