#pragma once

#include <Arduino.h>
#include "display/display.h"

// Small state machine for the idle animation (spinner + "SCAN TAG")
namespace DisplayAnim {

    // Idle-Animation starten (Standard: erst Animation, dann Text)
    void startIdle(unsigned long now);

    // Idle mit Text beginnen (z.B. direkt nach Boot: erst "SCAN TAG", dann Spinner)
    void startIdleTextFirst(unsigned long now);

    // Idle-Animation komplett stoppen (z.B. wenn ein Tag gescannt wurde)
    void stop();

    // Must be called regularly from loop() when the system is idle
    void tickIdle(DisplayType &display, unsigned long now);


    // --- Splash: 3-zeilige Typewriter-Animation (blocking, aber mit yield) ---
    void playThreeLineTypewriter(
        DisplayType& display,
        const String& line1,
        const String& line2,
        const String& line3,
        uint32_t charDelayMs     = 40,   // Delay pro Buchstabe (Tippen)
        uint32_t linePauseMs     = 250,  // Pause zwischen Zeilen (nach Tippen)
        uint32_t endHoldMs       = 800,  // Haltezeit nach kompletter Anzeige
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

