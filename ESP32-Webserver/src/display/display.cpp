#include "display/display.h"

DisplayType* MYDISPLAY::_display = nullptr;

void MYDISPLAY::init(DisplayType* disp)
{
    _display = disp;
}