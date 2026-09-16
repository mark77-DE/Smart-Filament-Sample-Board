#pragma once

#include <Adafruit_GFX.h>
#include "filament_db.h"
#include "display_config.h"
#include "globals.h"
#include "version_info.h"
#include "update_manager.h"

void displayInit();
void displayClear();
void displayFlush();

/**
 * @brief Display helper class for filament information and status displays.
 *
 *
 */

#ifndef TFT_BLACK
#define TFT_BLACK 0x0000
#endif

#ifndef TFT_WHITE
#define TFT_WHITE 0xFFFF
#endif

#ifndef TFT_RED
#define TFT_RED 0xF800
#endif

#ifndef TFT_GREEN
#define TFT_GREEN 0x07E0
#endif

#ifndef TFT_ORANGE
#define TFT_ORANGE 0xFD20
#endif

class MYDISPLAY
{
public:
    /**
     * @brief Display-Backend setzen (einmalig im Setup).
     * @param disp Zeiger auf das Display-Objekt
     */
    static void init(DisplayType *disp) { _display = disp; }

    /**
     * @brief Shows filament data (vendor / type / color) in three lines.
     */
    static void show(const FilamentEntry &entry);

    /**
     * @brief Shows a single centered line.
     * @param msg Text
     */
    static void showCentered(const String &msg, const int FOREGROUND_COLOR = TFT_WHITE, const int BACKGROUND_COLOR = TFT_BLACK);

    /**
     * @brief Shows two centered lines (keeps the existing API).
     * @param line1 First line
     * @param line2 Second line
     */
    static void showTwoLinesCentered(const String &line1, const String &line2);

    /**
     * @brief Shows three centered lines (new, for reboot countdown).
     *        Truncates with "..." when needed and adjusts the font to the display height.
     * @param line1 First line (top)
     * @param line2 Second line (middle)
     * @param line3 Third line (bottom)
     */
    static void showThreeLinesCentered(const String &line1, const String &line2, const String &line3, int foregroundColor = TFT_WHITE, int backgroundColor = TFT_BLACK);

    /**
     * @brief Shows four centered lines.
     * @param line1 First line (top)
     * @param line2 Second line
     * @param line3 Third line
     * @param line4 Fourth line (bottom)
     */
    static void showFourLinesCentered(const String &line1, const String &line2, const String &line3, const String &line4);

    /**
     * @brief Boot screen with firmware version/date
     * @param version Version text
     * @param dateShort Date
     */
    static void showBootVersion(const char *version, const char *dateShort);

    /**
     * @brief Clears the screen (e.g. before the idle animation).
     */
    static void clear();

    /**
     * @brief Shows an error message centered (e.g. for an unknown tag).
     * @param msg Error message text
     * @param FOREGROUND_COLOR Text color
     * @param BACKGROUND_COLOR Background color
     */
    static void showErrorCentered(const String &msg, const int FOREGROUND_COLOR = TFT_RED, const int BACKGROUND_COLOR = TFT_BLACK);

    static int getMaxTextSize(const String &text, int maxWidth, int maxSize);

    static String getDisplayVariant()
    {
#if DISPLAY_TYPE == DISPLAY_TYPE_SH1106
        return "sh1106";
#elif DISPLAY_TYPE == DISPLAY_TYPE_ST7789
        return "st7789";
#else
        return "unknown";
#endif
    }


    static void renderSelfUpdateStatus(const SelfUpdateStatus& status);



private:
    static DisplayType *_display;
};