//display_oled.cpp

#include "display/display_config.h"

#ifndef DISPLAY_TYPE
#define DISPLAY_TYPE DISPLAY_TYPE_SH1106
#endif

#if DISPLAY_TYPE == DISPLAY_TYPE_SH1106 || DISPLAY_TYPE == DISPLAY_TYPE_SSD1306

#include "display/display.h"

#include <Arduino.h>
#include "globals.h"
#include "version_info.h"
#include "config.h"
#include "pins.h"

DisplayType* MYDISPLAY::_display = nullptr;

static const int STD_FONT_HEIGHT = 7;

DisplayType display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET_PIN);


void displayInit()
{
    Serial.printf("[DISPLAY] Wire.begin SDA=%d SCL=%d\n", SDA_PIN, SCL_PIN);

    bool wireOk = Wire.begin(SDA_PIN, SCL_PIN);

    Serial.printf("[DISPLAY] Wire.begin result: %s\n",
                  wireOk ? "OK" : "FAILED");

    if (!initDisplay(display)) {
        Serial.println("Display init failed");
        return;
    }

    Serial.println("[DISPLAY] Display init OK");

    display.clearDisplay();

    MYDISPLAY::init(&display);
}






void displayClear()
{
    display.clearDisplay();
}

void displayFlush()
{
    display.display();
}

// ------------------------------------------------------------
// Umlaute → ASCII
// ------------------------------------------------------------
static String fixUmlauts(String s) {
  s.replace("ä", "ae"); s.replace("ö", "oe"); s.replace("ü", "ue");
  s.replace("Ä", "Ae"); s.replace("Ö", "Oe"); s.replace("Ü", "Ue");
  s.replace("ß", "ss");
  return s;
}

// ------------------------------------------------------------
// Auto-Fit für eine einzelne Zeile, horizontal zentriert.
// Berücksichtigt 32px-Displays (Standardfont) und 64px mit DISPLAY_FONT.
// yBaselineOrTop: bei 32px = top (wir addieren STD_FONT_HEIGHT), bei 64px = baseline.
// lineHeight: verteilte Zeilenhöhe (für 64px: abgeleitet aus DISPLAY_FONT)
// ------------------------------------------------------------
static void printLineAutoFitCenteredGfx(DisplayType* d, const String& raw,
                                        int16_t yBaselineOrTop,
                                        bool smallDisplay, int16_t lineHeight, const GFXfont* font = DISPLAY_FONT)
{
  if (!d) return;

  String t = fixUmlauts(raw);
  int16_t x1, y1; uint16_t w, h;

  if (smallDisplay) {
    // Immer Standard-Font, yBaseline = yTop + STD_FONT_HEIGHT
    d->setFont(nullptr);
    d->setTextSize(1);
    int16_t yBase = yBaselineOrTop + STD_FONT_HEIGHT;

    d->getTextBounds(t, 0, 0, &x1, &y1, &w, &h);
    if (w <= SCREEN_WIDTH) {
      int16_t x = (SCREEN_WIDTH - (int16_t)w)/2 - x1;
      d->setCursor(x, yBase);
      d->print(t);
      return;
    }
    // kürzen + "..."
    String base = t, out;
    while (base.length() > 0) {
      out = base + "...";
      d->getTextBounds(out, 0, 0, &x1, &y1, &w, &h);
      if (w <= SCREEN_WIDTH) {
        int16_t x = (SCREEN_WIDTH - (int16_t)w)/2 - x1;
        d->setCursor(x, yBase);
        d->print(out);
        return;
      }
      base.remove(base.length()-1);
    }
    d->setCursor((SCREEN_WIDTH-9)/2, yBase);
    d->print("...");
    return;
  }

  // 64px: erst DISPLAY_FONT probieren
  bool useStd = false;
  if (font != nullptr) {
    d->setFont(font);
    d->setTextSize(1);
    d->getTextBounds(t, 0, 0, &x1, &y1, &w, &h);
    if (w > SCREEN_WIDTH) useStd = true;
  } else {
    useStd = true;
  }

  if (useStd) {
    d->setFont(nullptr);
    d->setTextSize(1);
    // baseline für Standardfont optisch in die Zeilenhöhe mittig setzen
    int16_t yBase = yBaselineOrTop - (lineHeight - STD_FONT_HEIGHT)/2;

    d->getTextBounds(t, 0, 0, &x1, &y1, &w, &h);
    if (w <= SCREEN_WIDTH) {
      int16_t x = (SCREEN_WIDTH - (int16_t)w)/2 - x1;
      d->setCursor(x, yBase);
      d->print(t);
      return;
    }
    String base = t, out;
    while (base.length() > 0) {
      out = base + "...";
      d->getTextBounds(out, 0, 0, &x1, &y1, &w, &h);
      if (w <= SCREEN_WIDTH) {
        int16_t x = (SCREEN_WIDTH - (int16_t)w)/2 - x1;
        d->setCursor(x, yBase);
        d->print(out);
        return;
      }
      base.remove(base.length()-1);
    }
    d->setCursor((SCREEN_WIDTH-9)/2, yBase);
    d->print("...");
    return;
  }

  // DISPLAY_FONT passt
  d->getTextBounds(t, 0, 0, &x1, &y1, &w, &h);
  {
    int16_t x = (SCREEN_WIDTH - (int16_t)w)/2 - x1;
    d->setCursor(x, yBaselineOrTop);
    d->print(t);
  }
}

// ------------------------------------------------------------
// Anzeige der 3 Zeilen: vendor / type / color (DEIN ORIGINAL, unverändert)
// ------------------------------------------------------------
void MYDISPLAY::show(const FilamentEntry& entry) {
  if (!_display) return;

  _display->clearDisplay();
  _display->setTextWrap(false);
  _display->setTextColor(DISPLAY_COLOR);

  const bool smallDisplay = (SCREEN_HEIGHT <= 32);

  // Zeilenhöhe bestimmen
  int16_t x1=0, y1=0; uint16_t w=0, h=0;
  int16_t lineHeight = 0;

  if (smallDisplay) {
    lineHeight = STD_FONT_HEIGHT + 2;
  } else {
    _display->setFont(DISPLAY_FONT);
    _display->setTextSize(1);
    _display->getTextBounds("Hg", 0, 0, &x1, &y1, &w, &h);
    lineHeight = (int16_t)h + 2;
  }

  // Start-Y
  int16_t y = smallDisplay ? 0 : lineHeight;

  // Hilfsfunktion: eine Zeile linksbündig (deine Original-Logik)
  auto printLineAutoFit = [&](const String& rawText, int16_t yLine) {
    String t = fixUmlauts(rawText);

    if (smallDisplay) {
      _display->setFont(nullptr);
      _display->setTextSize(1);
      int16_t yBase = yLine + STD_FONT_HEIGHT;

      _display->getTextBounds(t, 0, 0, &x1, &y1, &w, &h);
      if (w <= SCREEN_WIDTH) {
        _display->setCursor(0, yBase);
        _display->print(t);
        return;
      }
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
        base.remove(base.length()-1);
      }
      _display->setCursor(0, yBase);
      _display->print("...");
      return;
    }

    bool forceFallback = (DISPLAY_AUTOFIT_THRESHOLD > 0 && t.length() > DISPLAY_AUTOFIT_THRESHOLD);

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

    _display->setFont(nullptr);
    _display->setTextSize(1);
    int16_t yAdjusted = yLine - (lineHeight - STD_FONT_HEIGHT)/2;

    _display->getTextBounds(t, 0, 0, &x1, &y1, &w, &h);
    if (w <= SCREEN_WIDTH) {
      _display->setCursor(0, yAdjusted);
      _display->print(t);
      return;
    }
    String base = t, out;
    while (base.length() > 0) {
      out = base + "...";
      _display->getTextBounds(out, 0, 0, &x1, &y1, &w, &h);
      if (w <= SCREEN_WIDTH) {
        _display->setCursor(0, yAdjusted);
        _display->print(out);
        return;
      }
      base.remove(base.length()-1);
    }
    _display->setCursor(0, yAdjusted);
    _display->print("...");
  };

  // Ausgabe: vendor / type / color
  printLineAutoFit(entry.vendor, y); y += lineHeight;
  printLineAutoFit(entry.type,   y); y += lineHeight;
  printLineAutoFit(entry.color,  y);

  _display->display();

  // Font-State wiederherstellen
  _display->setFont(DISPLAY_FONT);
  _display->setTextSize(1);

  // Debug
  if (CONFIGV2.system.debugMode) {
    Serial.println();
    Serial.println(F("[DISPLAY] SHOW:"));
    Serial.print(" Vendor: "); Serial.println(entry.vendor);
    Serial.print(" Type:   "); Serial.println(entry.type);
    Serial.print(" Color:  "); Serial.println(entry.color);
  }
  
}

// ------------------------------------------------------------
// Eine zentrierte Zeile (deine Original-Funktion, unverändert)
// ------------------------------------------------------------
void MYDISPLAY::showCentered(const String& msg, const int FOREGROUND_COLOR, const int BACKGROUND_COLOR) {
  if (!_display) return;

  _display->clearDisplay();

  _display->setFont(DISPLAY_FONT);
  _display->setTextSize(1);
  _display->setTextWrap(false);
  _display->setTextColor(DISPLAY_COLOR);

  int16_t x1, y1; uint16_t w, h;
  String fixed = fixUmlauts(msg);
  _display->getTextBounds(fixed, 0, 0, &x1, &y1, &w, &h);

  int16_t x = (SCREEN_WIDTH  - (int16_t)w) / 2 - x1;
  int16_t y = (SCREEN_HEIGHT - (int16_t)h) / 2 - y1;

  _display->setCursor(x, y);
  _display->print(fixed);
  _display->display();
}

// ------------------------------------------------------------
// Zwei zentrierte Zeilen (bestehende API wieder explizit verfügbar)
// ------------------------------------------------------------
void MYDISPLAY::showCenteredTwoLines(const String& line1, const String& line2) {
  if (!_display) return;

  _display->clearDisplay();
  _display->setTextWrap(false);
  _display->setTextColor(DISPLAY_COLOR);

  const bool smallDisplay = (SCREEN_HEIGHT <= 32);

  // Zeilenhöhe bestimmen
  int16_t x1=0, y1=0; uint16_t w=0, h=0;
  int16_t lineHeight = 0;
  if (smallDisplay) {
    lineHeight = STD_FONT_HEIGHT + 2;
  } else {
    _display->setFont(DISPLAY_FONT);
    _display->setTextSize(1);
    _display->getTextBounds("Hg", 0, 0, &x1, &y1, &w, &h);
    lineHeight = (int16_t)h + 2;
  }

  // Ziel: vertikal in 2 Zeilen verteilen → Start oben (wie bei show)
  int16_t y = smallDisplay ? 0 : lineHeight;

  printLineAutoFitCenteredGfx(_display, line1, y, smallDisplay, lineHeight); y += lineHeight;
  printLineAutoFitCenteredGfx(_display, line2, y, smallDisplay, lineHeight);

  _display->display();

  _display->setFont(DISPLAY_FONT);
  _display->setTextSize(1);
}

// ------------------------------------------------------------
// Drei zentrierte Zeilen (neu – für Reboot/Countdown/Prompts)
// ------------------------------------------------------------
void MYDISPLAY::showThreeLinesCentered(const String& line1, const String& line2, const String& line3, int foregroundColor, int backgroundColor) {
  if (!_display) return;

  _display->clearDisplay();
  _display->setTextWrap(false);
  _display->setTextColor(DISPLAY_COLOR);

  const bool smallDisplay = (SCREEN_HEIGHT <= 32);

  // Zeilenhöhe bestimmen
  int16_t x1=0, y1=0; uint16_t w=0, h=0;
  int16_t lineHeight = 0;
  if (smallDisplay) {
    lineHeight = STD_FONT_HEIGHT + 2;
  } else {
    _display->setFont(DISPLAY_FONT);
    _display->setTextSize(1);
    _display->getTextBounds("Hg", 0, 0, &x1, &y1, &w, &h);
    lineHeight = (int16_t)h + 2;
  }

  // Start oben (32px) oder mit Abstand (64px) – konsistent zu show()
  int16_t y = smallDisplay ? 0 : lineHeight;

  printLineAutoFitCenteredGfx(_display, line1, y, smallDisplay, lineHeight); y += lineHeight;
  printLineAutoFitCenteredGfx(_display, line2, y, smallDisplay, lineHeight); y += lineHeight;
  printLineAutoFitCenteredGfx(_display, line3, y, smallDisplay, lineHeight);

  _display->display();

  _display->setFont(DISPLAY_FONT);
  _display->setTextSize(1);
}

// ------------------------------------------------------------
// Drei zentrierte Zeilen (neu – für Reboot/Countdown/Prompts)
// ------------------------------------------------------------
void MYDISPLAY::showFourLinesCentered(const String& line1, const String& line2, const String& line3, const String& line4) {
  if (!_display) return;

  _display->clearDisplay();
  _display->setTextWrap(false);
  _display->setTextColor(DISPLAY_COLOR);

  const bool smallDisplay = (SCREEN_HEIGHT <= 32);

  // Zeilenhöhe bestimmen
  int16_t x1=0, y1=0; uint16_t w=0, h=0;
  int16_t lineHeight = 0;
  if (smallDisplay) {
    lineHeight = STD_FONT_HEIGHT + 2;
  } else {
    _display->setFont(&FreeMono7pt7b);
    _display->setTextSize(1);
    _display->getTextBounds("Hg", 0, 0, &x1, &y1, &w, &h);
    lineHeight = (int16_t)h + 3;
  }

  // Start oben (32px) oder mit Abstand (64px) – konsistent zu show()
  int16_t y = smallDisplay ? 0 : lineHeight;

  printLineAutoFitCenteredGfx(_display, line1, y, smallDisplay, lineHeight, &FreeMono7pt7b); y += lineHeight;
  printLineAutoFitCenteredGfx(_display, line2, y, smallDisplay, lineHeight, &FreeMono7pt7b); y += lineHeight;
  printLineAutoFitCenteredGfx(_display, line3, y, smallDisplay, lineHeight, &FreeMono7pt7b); y += lineHeight;
  printLineAutoFitCenteredGfx(_display, line4, y, smallDisplay, lineHeight, &FreeMono7pt7b);

  _display->display();

  _display->setFont(DISPLAY_FONT);
  _display->setTextSize(1);
}



// --- Neu: Bootscreen 
void MYDISPLAY::showBootVersion(const char* version, const char* dateShort) {
    if (!_display) return;

    String l1 = F("Firmware");
    String l2 = String(version);   // z.B. "FW v0.1.0"
    String l3 = String(dateShort);            // z.B. "21.12:25"

    showThreeLinesCentered(l1, l2, l3);            
}


void MYDISPLAY::showErrorCentered(const String& msg, const int FOREGROUND_COLOR, const int BACKGROUND_COLOR) {


  showCentered(msg, FOREGROUND_COLOR, BACKGROUND_COLOR);


}


void MYDISPLAY::clear() {
  if (_display) {
    _display->clearDisplay();
    _display->display();
  }
}







#endif