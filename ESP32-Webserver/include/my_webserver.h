// my_webserver.h
#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "filament_db.h"
#include "globals.h"

extern int targetLed;
extern int LED_COUNT;
extern int LED_BRIGHTNESS;
extern bool DEBUG_MODE;
extern unsigned long lastTagTime;
extern String lastScannedUID;

struct SysInfo
{
    const char *chipName;
    uint8_t cores;
    uint8_t revision;
    uint32_t flashSize;
    const char *fwVersion;
    const char *buildDate;
};

/**
 * @brief Initializes the web server and WebSocket handlers.
 * @param server Async web server instance to configure.
 * @param ws WebSocket instance bound to the /ws endpoint.
 */
void initWebServer(AsyncWebServer &server, AsyncWebSocket &ws);


/**
 * @brief Handles an NFC or WebIF UID event.
 * @param uid UID string to process.
 * @param source Origin of the UID event.
 */
void handleUID(const String &uid, UidSource source);

/**
 * @brief Sets the brightness of a specific LED index.
 * @param index LED index to update.
 * @param brightness New brightness value in the device’s range.
 */
void setLedBrightness(int index, int brightness);


void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len);


/**
 * @brief send frequent heartbeat to connected websocket clients
 * @param ws WebSocket instance bound to the /ws endpoint.
 */
void sendHeartbeat(AsyncWebSocket &ws);



/**
 * @brief Sends a storage-location update to all WebSocket clients.
 * @param ws WebSocket instance to broadcast to.
 * @param location Human-readable storage location string.
 */
void sendStorageLocation(AsyncWebSocket &ws, String location);

SysInfo getSysInfo();