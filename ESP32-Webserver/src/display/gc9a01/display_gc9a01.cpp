#include "display/display_config.h"

#if DISPLAY_TYPE == DISPLAY_TYPE_GC9A01

#include "display/display.h"

#include <LovyanGFX.hpp>
#include <SPI.h>

DisplayType display;

// LGFX Klasse für GC9A01 Display
class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_GC9A01 _panel;
    lgfx::Bus_SPI      _bus;

public:
    LGFX() {
        auto bus_cfg = _bus.config();
        bus_cfg.spi_host = SPI3_HOST;
        bus_cfg.freq_write = 80000000;
        bus_cfg.freq_read  = 16000000;
        bus_cfg.pin_sclk = SPI_SCK;
        bus_cfg.pin_mosi = SPI_MOSI;
        bus_cfg.pin_miso = -1;
        _bus.config(bus_cfg);

        auto panel_cfg = _panel.config();
        panel_cfg.pin_cs   = TFT_CS;
        panel_cfg.pin_rst  = TFT_RST;
        panel_cfg.pin_busy = -1;
        panel_cfg.panel_width  = 240;
        panel_cfg.panel_height = 240;
        panel_cfg.readable    = true;
        panel_cfg.invert      = false;
        panel_cfg.rgb_order   = true;
        _panel.config(panel_cfg);

        _panel.setBus(&_bus);
        setPanel(&_panel);
    }
};

// Globale Display-Instanz (keine static Konflikte)
LGFX display;

// -----------------------------
// Wrapper-Funktionen
// -----------------------------
void displayInit() {
    display.init();
    display.setBrightness(255);
    display.fillScreen(TFT_BLACK);
    MYDISPLAY::init(&display);
}

void displayClear() {
    display.fillScreen(0);
}

void displayFlush() {
    // TFT braucht kein flush
}

void displayLoop() {
}

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
    _display->fillScreen(0);
    _display->setCursor(0,0);
    _display->println(line1);
    _display->println(line2);
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
    _display->fillScreen(0);
    _display->println(msg);
}

void MYDISPLAY::showBootVersion(const char* version, const char* dateShort) {
    if (!_display) return;
    _display->fillScreen(0);
    _display->println(version);
    _display->println(dateShort);
}

#endif