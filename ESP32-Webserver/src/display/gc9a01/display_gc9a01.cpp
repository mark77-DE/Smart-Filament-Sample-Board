#include "display/display_config.h"

#if DISPLAY_TYPE == DISPLAY_TYPE_GC9A01

#include "display/gc9a01/display_gc9a01.h"  // LGFX bekannt
#include "display/display.h"
#include <LovyanGFX.hpp>
#include <SPI.h>

DisplayType display;

void displayInit() {
    display.init();
    display.setBrightness(255);
    display.setRotation(0);

    display.fillScreen(TFT_BLACK);
    
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextSize(1);

    MYDISPLAY::init(&display);

    display.fillScreen(TFT_BLACK);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.drawString("DISPLAY OK", display.width()/2, display.height()/2);
}

void displayClear() { display.fillScreen(0); }
void displayFlush() { /* TFT braucht kein flush */ }
void displayLoop() {}

void displayShowIP(const String &ip) {
    display.fillScreen(TFT_BLACK);
    display.setTextDatum(MC_DATUM);
    display.setTextSize(2);
    display.setTextColor(TFT_WHITE);
    display.drawString(ip.c_str(), display.width()/2, display.height()/2);
}

void displayMessage(const String &msg) {
    display.fillScreen(TFT_BLACK);
    display.setTextDatum(MC_DATUM);
    display.setTextSize(2);
    display.setTextColor(TFT_WHITE);
    display.drawString(msg.c_str(), display.width()/2, display.height()/2);
}

void MYDISPLAY::clear() {
    if (_display) _display->fillScreen(TFT_BLACK);
}

void MYDISPLAY::showThreeLinesCentered(const String& line1, const String& line2, const String& line3) {
    if (!_display) return;

    _display->fillScreen(TFT_BLACK);

    _display->setTextColor(TFT_WHITE, TFT_BLACK);
    _display->setTextSize(2);

    int y = 60;

    _display->setCursor(10, y);
    _display->println(line1);

    _display->setCursor(10, y + 40);
    _display->println(line2);

    _display->setCursor(10, y + 80);
    _display->println(line3);
}

void MYDISPLAY::showFourLinesCentered(const String& line1, const String& line2, const String& line3, const String& line4) {
    if (!_display) return;
    _display->fillScreen(0);
    _display->println(line1);
    _display->println(line2);
    _display->println(line3);
    _display->println(line4);
}

void MYDISPLAY::showCentered(const String& msg) {
    if (!_display) return;

    _display->fillScreen(TFT_BLACK);

    _display->setTextColor(TFT_WHITE, TFT_BLACK);
    _display->setTextSize(2);

    _display->drawCenterString(msg.c_str(), 120, 120);
}

void MYDISPLAY::showBootVersion(const char* version, const char* dateShort) {
    if (!_display) return;

    _display->fillScreen(TFT_BLACK);

    _display->setTextColor(TFT_WHITE, TFT_BLACK);
    _display->setTextSize(2);

    _display->drawCenterString(version, 120, 90);
    _display->drawCenterString(dateShort, 120, 130);
}

// -----------------------------
// WICHTIG: Definition des statischen Members
// -----------------------------
DisplayType* MYDISPLAY::_display = nullptr;

#endif