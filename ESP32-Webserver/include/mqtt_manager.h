// mqtt_manager.h
#pragma once
#include <PubSubClient.h>
#include <WiFi.h>
#include "filament_db.h"

void mqttInit();
void mqttLoop();
bool mqttIsConnected();
void publishAnimationStatus(bool on);



void publishFilamentState(const FilamentEntry& entry);

