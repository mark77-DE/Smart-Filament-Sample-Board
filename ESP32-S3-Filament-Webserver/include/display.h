#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_SSD1306.h>
#include "filament_db.h"
#include "display_config.h"

// Typ-Alias für dein Display, z.B.
// using DisplayType = Adafruit_SH1106G;  // steht ja in display.h / display_config.h

class MYDISPLAY {
public:
    static void init(DisplayType* disp);
    static void show(const FilamentEntry& entry);
    static void showCentered(const String& msg);

private:
    static DisplayType* _display;
};
