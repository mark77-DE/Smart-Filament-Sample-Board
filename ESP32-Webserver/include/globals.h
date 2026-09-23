#pragma once

#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>



extern AsyncWebServer server;
extern AsyncWebSocket ws;


extern volatile bool rebootPending;
extern unsigned long rebootAt;
extern bool rebootReason;

extern uint32_t REBOOT_DELAY_MS;        // Button-Delay (ms)
extern uint32_t REBOOT_DELAY_WEBIF_MS;  // WebIF-Delay (ms)

// ----------------- LED & Tag -----------------
extern int targetLed;                // currently active LED
extern unsigned long ledStartTime;   // LED start time
//extern const unsigned long LED_TIMEOUT; // 3s Timeout
extern bool displayIdleShown;

extern bool DEBUG_MODE;

extern String LATEST_FIRMWARE_VERSION;

/**
 * @brief Origin of the scanned UID.
 */
enum class UidSource {
    NFC,
    WEBIF
};

/**
 * @brief Arms a web-interface idle timer.
 * @param ms Timeout in milliseconds before the UI is considered idle.
 */
void webifArmIdleTimeout(uint32_t ms);

/**
 * @brief Cancels the currently armed web-interface idle timeout.
 */
void webifCancelIdleTimeout();

/**
 * @brief Checks whether the web interface has been idle longer than the timeout.
 * @param now Current millisecond timestamp.
 * @return true if the idle timeout has elapsed, otherwise false.
 */
bool webifIdleDue(uint32_t now);

extern volatile bool g_applyConfigPending;
extern volatile bool g_reloadFilamentsPending;


void activateLed(int index);

extern volatile bool factoryResetRequested;