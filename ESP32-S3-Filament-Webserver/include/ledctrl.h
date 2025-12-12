#pragma once
#include <Adafruit_NeoPixel.h>

extern int LED_COUNT;
extern int LED_PIN;
extern uint32_t LED_COLOR; // global

class LEDCTRL {
public:
    static void init(int count, int pin);       // LED Strip initialisieren
    static void setPixel(int index, uint32_t color); // Pixel setzen
    static void allOff();                        // alle LEDs aus
    static Adafruit_NeoPixel* _leds;            // Pointer auf den Strip
};

// --- neue Deklaration ---
void loadConfig();
extern int LED_BRIGHTNESS;