#pragma once
#include <Adafruit_NeoPixel.h>

extern int targetLed;
extern unsigned long lastTagTime;


void setLedBrightness(int index, int brightness);
void ledLoop();

class LEDCTRL {
public:
    static void init(Adafruit_NeoPixel* strip);
    static void highlight(int index);
    static void allOff();
private:
    static Adafruit_NeoPixel* _leds;
};