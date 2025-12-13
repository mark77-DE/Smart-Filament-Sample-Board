#include "display.h"
#include "display_config.h"
#include <Arduino.h>

DisplayType* MYDISPLAY::_display = nullptr;

// feste Höhe der Adafruit-GFX Standardfont (5x7 Bitmap-Font)
static const int STD_FONT_HEIGHT = 7;

void MYDISPLAY::init(DisplayType* disp) {
    _display = disp;
}

// ------------------------------------------------------------
// Ersetzt Umlaute durch ASCII-Äquivalente
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// Anzeige der 3 Zeilen: vendor / type / color
// ------------------------------------------------------------
void MYDISPLAY::show(const FilamentEntry& entry) {
    if (!_display) return;

    _display->clearDisplay();
    _display->setTextWrap(false);
    _display->setTextColor(DISPLAY_COLOR);

    const bool smallDisplay = (SCREEN_HEIGHT <= 32);

    // ------------------------------------------------------------
    // Zeilenhöhe bestimmen
    // ------------------------------------------------------------
    int16_t x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;

    int16_t lineHeight = 0;

    if (smallDisplay) {
        // Standardfont: feste Höhe (plus kleiner Abstand)
        lineHeight = STD_FONT_HEIGHT + 2;
    } else {
        // Großer Font: messen (wie vorher)
        _display->setFont(DISPLAY_FONT);
        _display->setTextSize(1);
        _display->getTextBounds("Hg", 0, 0, &x1, &y1, &w, &h);
        lineHeight = (int16_t)h + 2;
    }

    // ------------------------------------------------------------
    // Start-Y:
    // - 32er Display: oben starten (damit wirklich "oben links")
    // - 64er Display: wie vorher eine Zeile Abstand nach oben
    // ------------------------------------------------------------
    int16_t y = smallDisplay ? 0 : lineHeight;

    // ------------------------------------------------------------
    // Hilfsfunktion: eine Zeile ausgeben, passend machen
    // ------------------------------------------------------------
    auto printLineAutoFit = [&](const String& rawText, int16_t yLine) {
        String t = fixUmlauts(rawText);

        // ---- 32er Display: IMMER Standardfont + sauber oben links ----
        if (smallDisplay) {
            _display->setFont(nullptr);
            _display->setTextSize(1);

            // Bei Standardfont ist y die Baseline – damit es optisch oben beginnt,
            // verschieben wir um STD_FONT_HEIGHT nach unten:
            int16_t yBase = yLine + STD_FONT_HEIGHT;

            _display->getTextBounds(t, 0, 0, &x1, &y1, &w, &h);
            if (w <= SCREEN_WIDTH) {
                _display->setCursor(0, yBase);
                _display->print(t);
                return;
            }

            // Kürzen + "..."
            String base = t;
            String out;
            while (base.length() > 0) {
                out = base + "...";
                _display->getTextBounds(out, 0, 0, &x1, &y1, &w, &h);
                if (w <= SCREEN_WIDTH) {
                    _display->setCursor(0, yBase);
                    _display->print(out);
                    return;
                }
                base.remove(base.length() - 1);
            }

            _display->setCursor(0, yBase);
            _display->print("...");
            return;
        }

        // ---- 64er Display: altes Verhalten beibehalten ----

        // Ab DISPLAY_AUTOFIT_THRESHOLD Zeichen direkt auf Standard-Font wechseln
        bool forceFallback =
            (DISPLAY_AUTOFIT_THRESHOLD > 0 && t.length() > DISPLAY_AUTOFIT_THRESHOLD);

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

        // 2) Zweiter Versuch: Standardfont (schmaler) → wie zuvor vertikal "einpassen"
        _display->setFont(nullptr);
        _display->setTextSize(1);

        // Alte Logik: Standardfont innerhalb der größeren Zeilenhöhe "zentrieren"
        int16_t yAdjusted = yLine - (lineHeight - STD_FONT_HEIGHT) / 2;

        _display->getTextBounds(t, 0, 0, &x1, &y1, &w, &h);

        if (w <= SCREEN_WIDTH) {
            _display->setCursor(0, yAdjusted);
            _display->print(t);
            return;
        }

        // 3) Immer noch zu breit → kürzen + "..."
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
            base.remove(base.length() - 1);
        }

        _display->setCursor(0, yAdjusted);
        _display->print("...");
    };

    // ------------------------------------------------------------
    // Ausgabe: vendor / type / color
    // ------------------------------------------------------------
    printLineAutoFit(entry.vendor, y);
    y += lineHeight;

    printLineAutoFit(entry.type, y);
    y += lineHeight;

    printLineAutoFit(entry.color, y);

    _display->display();

    // ------------------------------------------------------------
    // WICHTIG: Font-State wieder herstellen (für Idle/SCAN TAG)
    // ------------------------------------------------------------
    _display->setFont(DISPLAY_FONT);
    _display->setTextSize(1);

    // Debug
    Serial.print(F("DISPLAY: "));
    Serial.print(entry.vendor);
    Serial.print(' ');
    Serial.print(entry.type);
    Serial.print(' ');
    Serial.println(entry.color);
}

// ------------------------------------------------------------
// Zentrierte Anzeige (z.B. "UNBEKANNT", IP, etc.)
// ------------------------------------------------------------
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
