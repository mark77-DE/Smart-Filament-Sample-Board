#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeMono7pt7b.h>
#include <Fonts/FreeSans9pt7b.h>


//Standby Text
#define IDLE_TEXT_STRING "SCAN TAG"

// Idle-Animation-Konfiguration Zeit in ms zwischen Frames (Animation)
#define IDLE_ANIM_FRAME_DELAY 42    

//Anzeigedauer Standy-Text
#define IDLE_TEXT_DURATION 5000

// Delay zwischen den Buchstaben in ms (SCAN TAG Schriftzug)
#define IDLE_TEXT_CHAR_DELAY 75 

//Blink-Intervall für den Cursor "_"
#define IDLE_TEXT_CURSOR_BLINK_INTERVAL 400

// Cropping-Modus bei 32er Displays 1= Zentriert
#define FRAME_CROP_MODE_CENTER 1

// -----------------------------
// Auto Fit Konfiguration
// -----------------------------
// Ab wie vielen Zeichen auf die Standard-Font (nullptr) gewechselt wird.
// 0 = Autofit nur per Pixelbreite, kein Wechsel nur wegen Länge.
#define DISPLAY_AUTOFIT_THRESHOLD 12


// -----------------------------
// FONT Einstellungen
// -----------------------------
// Zentrale Font-Auswahl:
// - für GFX-Font:    &FreeSans9pt7b
// - für Standardfont: nullptr
#define DISPLAY_FONT &FreeSans9pt7b
//#define DISPLAY_FONT      &FreeMono7pt7b
//#define DISPLAY_FONT nullptr   // <- wenn du die Standard-Schrift nutzen willst


// -----------------------------
// Display-Typ wählen
// -----------------------------
// Entweder hier hart einstellen, oder über die platformio.ini per build_flag
#define DISPLAY_TYPE_SH1106 1
#define DISPLAY_TYPE_SSD1306 2

#ifndef DISPLAY_TYPE
  // Default, falls nichts per build_flag gesetzt wird:
  #define DISPLAY_TYPE DISPLAY_TYPE_SH1106
#endif


// -----------------------------
// Display-Konfiguration
// -----------------------------
#if DISPLAY_TYPE == DISPLAY_TYPE_SH1106

  using DisplayType = Adafruit_SH1106G;
  static constexpr int SCREEN_WIDTH   = 128;
  static constexpr int SCREEN_HEIGHT  = 64;
  static constexpr int OLED_RESET_PIN = -1;
  static constexpr uint8_t OLED_ADDR  = 0x3C;
  static constexpr uint16_t DISPLAY_COLOR = SH110X_WHITE;

#elif DISPLAY_TYPE == DISPLAY_TYPE_SSD1306

  using DisplayType = Adafruit_SSD1306;
  static constexpr int SCREEN_WIDTH   = 128;
  static constexpr int SCREEN_HEIGHT  = 32;
  static constexpr int OLED_RESET_PIN = -1;
  static constexpr uint8_t OLED_ADDR  = 0x3C;
  static constexpr uint16_t DISPLAY_COLOR = SSD1306_WHITE;

#else
  #error "Ungültiger DISPLAY_TYPE! Bitte DISPLAY_TYPE_SH1106 oder DISPLAY_TYPE_SSD1306 verwenden."
#endif

// -----------------------------
// Helper für die Initialisierung
// -----------------------------
inline bool initDisplay(DisplayType &disp) {
#if DISPLAY_TYPE == DISPLAY_TYPE_SH1106
    // SH1106: begin(addr, reset)
    return disp.begin(OLED_ADDR, true);
#elif DISPLAY_TYPE == DISPLAY_TYPE_SSD1306
    // SSD1306: begin(vccstate, addr)
    return disp.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
#endif
}
