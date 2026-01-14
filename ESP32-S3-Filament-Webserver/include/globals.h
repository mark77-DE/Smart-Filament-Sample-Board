#pragma once

#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>



extern AsyncWebServer server;
extern AsyncWebSocket ws;


extern volatile bool rebootPending;
extern unsigned long rebootAt;

extern uint32_t REBOOT_DELAY_MS;        // Button-Delay (ms)
extern uint32_t REBOOT_DELAY_WEBIF_MS;  // WebIF-Delay (ms)

// ----------------- LED & Tag -----------------
extern int targetLed;                // aktuell aktive LED
extern unsigned long ledStartTime;   // Startzeit der LED
//extern const unsigned long LED_TIMEOUT; // 3s Timeout
extern bool displayIdleShown;

extern bool DEBUG_MODE;

enum class UidSource {
    NFC,
    WEBIF
};

// NEU: WebIF Idle Timer (OLED Rücksprung)
void webifArmIdleTimeout(uint32_t ms);
void webifCancelIdleTimeout();
bool webifIdleDue(uint32_t now);

extern volatile bool g_applyConfigPending;
extern volatile bool g_reloadFilamentsPending;


