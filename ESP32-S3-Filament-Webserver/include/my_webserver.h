// my_webserver.h
#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "filament_db.h"

extern int targetLed;
extern int LED_COUNT;
extern int LED_BRIGHTNESS;
extern unsigned long lastTagTime;
extern String lastScannedUID;
extern void notifyUID(const String &uid);


void setLedBrightness(int index, int brightness);
void initWebServer(AsyncWebServer &server, AsyncWebSocket &ws);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len);