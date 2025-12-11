#pragma once
#include "display_config.h"
#include "filament_db.h"

class MYDISPLAY {
public:
    static void init(DisplayType* disp);
    static void show(const FilamentEntry& entry);
private:
    static DisplayType* _display;
};
