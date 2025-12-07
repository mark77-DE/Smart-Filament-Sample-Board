#include "ledctrl.h"

Adafruit_NeoPixel* LEDCTRL::_leds = nullptr;

void LEDCTRL::init(Adafruit_NeoPixel* strip) {
    _leds = strip;
    _leds->begin();
    _leds->show();
}

void LEDCTRL::highlight(int index) {
    if (!_leds || index < 0 || index >= _leds->numPixels()) return;
    _leds->clear();
    _leds->setPixelColor(index, _leds->Color(55,0,0));
    _leds->show();
}

void LEDCTRL::allOff() {
    if (!_leds) return;
    _leds->clear();
    _leds->show();
}