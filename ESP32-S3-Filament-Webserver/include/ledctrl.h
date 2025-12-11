#pragma once
#include <Adafruit_NeoPixel.h>

extern int targetLed;
extern unsigned long lastTagTime;

extern int LED_COUNT;
extern int LED_PIN;

void loadConfig();
void setLedBrightness(int index, int brightness);
void ledLoop();


class LEDCTRL {
public:
    // init nimmt jetzt LED-Anzahl und Pin
    static void init(int count, int pin);
    static void highlight(int index);
    static void allOff();
private:
    static Adafruit_NeoPixel* _leds;
};
