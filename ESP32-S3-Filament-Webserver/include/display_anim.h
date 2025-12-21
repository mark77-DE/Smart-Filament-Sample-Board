#pragma once

#include <Arduino.h>
#include "display.h"

// Kleine State-Machine für die Idle-Animation (Spinner + "SCAN TAG")
namespace DisplayAnim {

    // Idle-Animation starten (Standard: erst Animation, dann Text)
    void startIdle(unsigned long now);

    // Idle mit Text beginnen (z.B. direkt nach Boot: erst "SCAN TAG", dann Spinner)
    void startIdleTextFirst(unsigned long now);

    // Idle-Animation komplett stoppen (z.B. wenn ein Tag gescannt wurde)
    void stop();

    // Muss regelmäßig aus loop() aufgerufen werden, wenn das System im Idle ist
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
        bool     eraseBackwards  = true, // Rückwärts löschen aktiv?
        uint32_t eraseCharDelayMs= 40,   // Delay pro Buchstabe (Löschen)
        uint32_t eraseLinePauseMs= 180   // Pause zwischen Zeilen (vor Löschen)
    );

    // Komfort-Overload für PROGMEM-Strings (F("..."))
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

