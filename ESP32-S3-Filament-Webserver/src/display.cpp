#include "display.h"
#include "display_config.h"
#include <Arduino.h>

DisplayType* MYDISPLAY::_display = nullptr;

// feste Höhe der Standard-Font (5x7 Bitmap-Font)
static const int STD_FONT_HEIGHT = 7;

void MYDISPLAY::init(DisplayType* disp) {
    _display = disp;
}

// Ersetzt Umlaute durch ASCII-Äquivalente
static String fixUmlauts(String s) {
    s.replace("ä", "ae");
    s.replace("ö", "oe");
    s.replace("ü", "ue");
    s.replace("Ä", "Ae");
    s.replace("Ö", "Oe");
    s.replace("Ü", "Ue");
    s.replace("ß", "ss");
    return s;
}

void MYDISPLAY::show(const FilamentEntry& entry) {
    if (!_display) return;

    _display->clearDisplay();
    _display->setTextWrap(false);
    _display->setTextColor(DISPLAY_COLOR);

    // Basis-Font für Layout-Berechnung
    _display->setFont(DISPLAY_FONT);
    _display->setTextSize(1);

    int16_t x1, y1;
    uint16_t w, h;
    _display->getTextBounds("Hg", 0, 0, &x1, &y1, &w, &h);
    int16_t lineHeight = h + 2;   // etwas Abstand

    int16_t y = lineHeight;       // erste Zeile

    // Hilfsfunktion für eine Zeile: passt Text an und kürzt bei Bedarf
    auto printLineAutoFit = [&](const String& text, int16_t yLine) {
        String t = text;

        // Konfigurierbarer Schwellwert:
        // Ab DISPLAY_AUTOFIT_THRESHOLD Zeichen direkt auf Standard-Font wechseln
        bool forceFallback = (DISPLAY_AUTOFIT_THRESHOLD > 0 &&
                              t.length() > DISPLAY_AUTOFIT_THRESHOLD);

        // 1) Versuch: DISPLAY_FONT (falls gesetzt und Text nicht zu lang)
        if (DISPLAY_FONT != nullptr && !forceFallback) {
            _display->setFont(DISPLAY_FONT);
            _display->setTextSize(1);
            _display->getTextBounds(t, 0, 0, &x1, &y1, &w, &h);

            if (w <= SCREEN_WIDTH) {
                _display->setCursor(0, yLine);
                _display->print(t);
                return;
            }
        }

        // 2) Zweiter Versuch: Standard-Font (schmaler) → vertikal zentrieren
        _display->setFont(nullptr);
        _display->setTextSize(1);

        // Vertikale Zentrierung der Standard-Font innerhalb der GFX-Zeilenhöhe
        int16_t yAdjusted = yLine - (lineHeight - STD_FONT_HEIGHT) / 2;

        _display->getTextBounds(t, 0, 0, &x1, &y1, &w, &h);

        if (w <= SCREEN_WIDTH) {
            _display->setCursor(0, yAdjusted);
            _display->print(t);
            return;
        }

        // 3) Immer noch zu breit → von hinten kürzen und "..." anhängen
        String base = t;
        String out;

        while (base.length() > 0) {
            out = base + "...";
            _display->getTextBounds(out, 0, 0, &x1, &y1, &w, &h);
            if (w <= SCREEN_WIDTH) {
                _display->setCursor(0, yAdjusted);
                _display->print(out);
                return;
            }
            base.remove(base.length() - 1);  // ein Zeichen weg
        }

        // Falls alles schief geht: nur "..."
        _display->setCursor(0, yAdjusted);
        _display->print("...");
    };

    // Zeilen ausgeben (mit Umlaut-Ersetzung)
    printLineAutoFit(fixUmlauts(entry.vendor), y);

    y += lineHeight;
    printLineAutoFit(fixUmlauts(entry.type), y);

    y += lineHeight;
    printLineAutoFit(fixUmlauts(entry.color), y);

    // Optional noch Slot:
    // y += lineHeight;
    // printLineAutoFit(fixUmlauts(String("Slot ") + entry.ledIndex), y);

    _display->display();

    // Debug
    Serial.print(F("DISPLAY: "));
    Serial.print(entry.vendor);
    Serial.print(' ');
    Serial.print(entry.type);
    Serial.print(' ');
    Serial.println(entry.color);
}

void MYDISPLAY::showCentered(const String& msg) {
    if (!_display) return;

    _display->clearDisplay();

    _display->setFont(DISPLAY_FONT);
    _display->setTextSize(1);
    _display->setTextWrap(false);
    _display->setTextColor(DISPLAY_COLOR);

    int16_t x1, y1;
    uint16_t w, h;
    String fixed = fixUmlauts(msg);
    _display->getTextBounds(fixed, 0, 0, &x1, &y1, &w, &h);

    int16_t x = (SCREEN_WIDTH  - w) / 2 - x1;
    int16_t y = (SCREEN_HEIGHT - h) / 2 - y1;

    _display->setCursor(x, y);
    _display->print(fixed);
    _display->display();
}
