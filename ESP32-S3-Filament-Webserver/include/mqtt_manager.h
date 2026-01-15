// mqtt_manager.h
#pragma once
#include <PubSubClient.h>
#include <WiFi.h>

void mqttInit();
void mqttLoop();
bool mqttIsConnected();
