#pragma once

#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

extern AsyncWebServer server;
extern AsyncWebSocket ws;

// ----------------- LED & Tag -----------------
extern int targetLed;                // aktuell aktive LED
extern unsigned long ledStartTime;   // Startzeit der LED
extern const unsigned long LED_TIMEOUT; // 3s Timeout
extern bool displayIdleShown;
