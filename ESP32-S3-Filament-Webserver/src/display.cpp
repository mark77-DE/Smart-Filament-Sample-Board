#include "display.h"

Adafruit_SSD1306* MYDISPLAY::_display = nullptr;

void MYDISPLAY::init(Adafruit_SSD1306* disp) {
    _display = disp;
}

void MYDISPLAY::show(const FilamentEntry& entry) {
    if (!_display) return;
    _display->clearDisplay();
    _display->setCursor(0,0);
    _display->println(entry.vendor);
    _display->println(entry.type);
    _display->println(entry.color);
    _display->display();
}