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
extern void notifyUID(const String &uid);
void handleUID(const String &uid, UidSource source);



void setLedBrightness(int index, int brightness);
void initWebServer(AsyncWebServer &server, AsyncWebSocket &ws);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len);

void sendHeartbeat(AsyncWebSocket &ws);

struct SysInfo {
    const char* chipName;
    uint8_t cores;
    uint8_t revision;
    uint32_t flashSize;
    const char* fwVersion;
    const char* buildDate;
};    





SysInfo getSysInfo();