#pragma once

#include <Adafruit_GFX.h>
#include <Fonts/FreeMono7pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

#ifndef DISPLAY_TYPE_SH1106
#define DISPLAY_TYPE_SH1106 1
#endif


#ifndef DISPLAY_TYPE_ST7789
#define DISPLAY_TYPE_ST7789 3
#endif

// -----------------------------
// Standby Text / Idle Animation
// -----------------------------
#define IDLE_TEXT_STRING "SCAN TAG"
#define IDLE_ANIM_FRAME_DELAY 30    
#define IDLE_TEXT_DURATION 5000
#define IDLE_TEXT_CHAR_DELAY 65 
#define IDLE_TEXT_CURSOR_BLINK_INTERVAL 500
#define FRAME_CROP_MODE_CENTER 1

// -----------------------------
// Auto Fit & Fonts
// -----------------------------
#define DISPLAY_AUTOFIT_THRESHOLD 0
#define DISPLAY_FONT &FreeSans9pt7b
//#define DISPLAY_FONT &FreeMono7pt7b
//#define DISPLAY_FONT nullptr

// -----------------------------
// Display-Typ
// -----------------------------
#ifndef DISPLAY_TYPE
  #define DISPLAY_TYPE DISPLAY_TYPE_SH1106
#endif

#if DISPLAY_TYPE == DISPLAY_TYPE_SH1106
  #include <Adafruit_SH110X.h>
  using DisplayType = Adafruit_SH1106G;
  static constexpr int SCREEN_WIDTH   = 128;
  static constexpr int SCREEN_HEIGHT  = 64;
  static constexpr int OLED_RESET_PIN = -1;
  static constexpr uint8_t OLED_ADDR  = 0x3C;
  static constexpr uint16_t DISPLAY_COLOR = SH110X_WHITE;



#elif DISPLAY_TYPE == DISPLAY_TYPE_ST7789
  #include "display/st7789/display_st7789.h"  // LGFX bekannt
  #include <LovyanGFX.hpp>

  using DisplayType = LGFX;  
  

  static constexpr int SCREEN_WIDTH   = 170;
  static constexpr int SCREEN_HEIGHT  = 320;

#else
  #error "Ungültiger DISPLAY_TYPE! Bitte DISPLAY_TYPE_SH1106 oder ST7789 verwenden."
#endif


extern DisplayType display;              // LGFX-Objekt, global

// -----------------------------
// Helper für Initialisierung
// -----------------------------
inline bool initDisplay(DisplayType &disp) {
#if DISPLAY_TYPE == DISPLAY_TYPE_SH1106
    return disp.begin(OLED_ADDR, true);
#elif DISPLAY_TYPE == DISPLAY_TYPE_ST7789
    disp.init(); // LovyanGFX init
    return true;
#endif
}