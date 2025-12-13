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
}

