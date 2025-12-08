#pragma once
#include <Adafruit_SSD1306.h>
#include "filament_db.h"

class MYDISPLAY {
public:
    static void init(Adafruit_SSD1306* disp);
    static void show(const FilamentEntry& entry);
private:
    static Adafruit_SSD1306* _display;
};