#include "display.h"
#include <Arduino.h>  // für Serial, falls nicht eh schon drin

DisplayType* MYDISPLAY::_display = nullptr;

void MYDISPLAY::init(DisplayType* disp) {
    _display = disp;
}

void MYDISPLAY::show(const FilamentEntry& entry) {
    if (!_display) return;

    _display->clearDisplay();
    _display->setCursor(0, 0);
    _display->println(entry.vendor);
    _display->println(entry.type);
    _display->println(entry.color);
    _display->display();

    Serial.println("DISPLAY: " + entry.vendor + " " + entry.type + " " + entry.color);
}
