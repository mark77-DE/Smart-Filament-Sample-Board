#pragma once

#include <Arduino.h>
#include "display/display.h"

// Small state machine for the idle animation (spinner + "SCAN TAG")
namespace DisplayAnim {

    // Start idle animation (default: animation first, then text)
    void startIdle(unsigned long now);

    // Start idle with text (e.g. right after boot: first "SCAN TAG", then spinner)
    void startIdleTextFirst(unsigned long now);

    // Stop the idle animation completely (e.g. when a tag is scanned)
    void stop();

    // Must be called regularly from loop() when the system is idle
    void tickIdle(DisplayType &display, unsigned long now);


    // --- Splash: 3-line typewriter animation (blocking, but with yield) ---
    void playThreeLineTypewriter(
        DisplayType& display,
        const String& line1,
        const String& line2,
        const String& line3,
        uint32_t charDelayMs     = 40,   // Delay per character (typing)
        uint32_t linePauseMs     = 250,  // Pause between lines (after typing)
        uint32_t endHoldMs       = 800,  // Hold time after full display
        bool     eraseBackwards  = true, // Enable backward erasing?
        uint32_t eraseCharDelayMs= 10,   // Delay per character (erasing)
        uint32_t eraseLinePauseMs= 180   // Pause between lines (before erasing)
    );

    // Convenience overload for PROGMEM strings (F("..."))
    void playThreeLineTypewriter(
        DisplayType& display,
        const __FlashStringHelper* line1,
        const __FlashStringHelper* line2,
        const __FlashStringHelper* line3,
        uint32_t charDelayMs     ,
        uint32_t linePauseMs     ,
        uint32_t endHoldMs       ,
        bool     eraseBackwards  ,
        uint32_t eraseCharDelayMs,
        uint32_t eraseLinePauseMs
    );

    


}

