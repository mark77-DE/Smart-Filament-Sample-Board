#include "display/display_config.h"

#if DISPLAY_TYPE == DISPLAY_TYPE_ST7789

#include "display/st7789/display_st7789.h"  // LGFX bekannt
#include "display/display.h"
#include <LovyanGFX.hpp>
#include <SPI.h>

DisplayType display;

void displayInit() {
    display.init();
    display.setBrightness(255);
    display.setRotation(1);

    display.fillScreen(TFT_BLACK);
    
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextSize(1);

    MYDISPLAY::init(&display);

    display.fillScreen(TFT_BLACK);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);

    const int cx = display.width() / 2;
    const int cy = display.height() / 2;

    display.drawString("DISPLAY OK", cx, cy);
}

void displayClear() { display.fillScreen(0); }
void displayFlush() { /* TFT braucht kein flush */ }
void displayLoop() {}

void displayShowIP(const String &ip) {
    display.fillScreen(TFT_BLACK);
    display.setTextDatum(MC_DATUM);
    display.setTextSize(2);
    display.setTextColor(TFT_GREEN);

    const int cx = display.width() / 2;
    const int cy = display.height() / 2;

    display.drawString(ip.c_str(), cx, cy);
}


void MYDISPLAY::clear() {
    if (_display) _display->fillScreen(TFT_BLACK);
}

void MYDISPLAY::showThreeLinesCentered(
    const String& line1,
    const String& line2,
    const String& line3,
    int foregroundColor,
    int backgroundColor
) {
    if (!_display) return;

    _display->fillScreen(TFT_BLACK);
    _display->setTextDatum(MC_DATUM);
    _display->setTextColor(foregroundColor, backgroundColor);
    _display->setTextSize(2);

    const int cx = display.width() / 2;
    const int cy = display.height() / 2;

    const int lineSpacing = display.height() / 6;

    _display->drawString(line1, cx, cy - lineSpacing);
    _display->drawString(line2, cx, cy);
    _display->drawString(line3, cx, cy + lineSpacing);
}

void MYDISPLAY::showFourLinesCentered(
    const String& line1,
    const String& line2,
    const String& line3,
    const String& line4
) {
    if (!_display) return;

    _display->fillScreen(TFT_BLACK);
    _display->setTextDatum(MC_DATUM);
    _display->setTextSize(2);
    _display->setTextColor(TFT_WHITE, TFT_BLACK);

    const int cx = display.width() / 2;
    const int cy = display.height() / 2;

    const int spacing = display.height() / 8;

    _display->drawString(line1, cx, cy - (spacing * 1.5));
    _display->drawString(line2, cx, cy - (spacing * 0.5));
    _display->drawString(line3, cx, cy + (spacing * 0.5));
    _display->drawString(line4, cx, cy + (spacing * 1.5));
}

void MYDISPLAY::showCentered(
    const String& msg,
    const int FOREGROUND_COLOR,
    const int BACKGROUND_COLOR
) {
    if (!_display) return;

    _display->fillScreen(TFT_BLACK);
    _display->setTextDatum(MC_DATUM);
    _display->setTextColor(FOREGROUND_COLOR, BACKGROUND_COLOR);
    _display->setTextSize(3);

    const int cx = display.width() / 2;
    const int cy = display.height() / 2;

    _display->drawString(msg.c_str(), cx, cy);
}

void MYDISPLAY::showBootVersion(const char* version, const char* dateShort) {
    if (!_display) return;

    _display->fillScreen(TFT_BLACK);
    _display->setTextDatum(MC_DATUM);
    _display->setTextColor(TFT_WHITE, TFT_BLACK);
    _display->setTextSize(2);

    const int cx = display.width() / 2;
    const int cy = display.height() / 2;

    const int offset = display.height() / 8;

    _display->drawString(version, cx, cy - offset);
    _display->drawString(dateShort, cx, cy + offset);
}



void MYDISPLAY::showErrorCentered(
    const String& msg,
    const int FOREGROUND_COLOR,
    const int BACKGROUND_COLOR
) {
    if (!_display) return;

    _display->fillScreen(TFT_BLACK);
    _display->setTextDatum(MC_DATUM);
    _display->setTextColor(FOREGROUND_COLOR, BACKGROUND_COLOR);
    _display->setTextSize(3);

    const int cx = display.width() / 2;
    const int cy = display.height() / 2;

    _display->drawString(msg.c_str(), cx, cy);

    

    // ----------------------
    // 2px breiter roter Rahmen
    // ----------------------
    const int borderWidth = 2;
    _display->drawRect(
        borderWidth / 2,                        // x = 1 (2px Rand innen)
        borderWidth / 2,                        // y = 1
        _display->width() - borderWidth,        // Breite
        _display->height() - borderWidth,       // Höhe
        FOREGROUND_COLOR                        // Farbe
    );
}


// -----------------------------
// WICHTIG: Definition des statischen Members
// -----------------------------
DisplayType* MYDISPLAY::_display = nullptr;

#endif