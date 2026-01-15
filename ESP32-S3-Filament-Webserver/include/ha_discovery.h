#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include "config.h"

/**
 * @brief Veröffentlicht Home-Assistant MQTT Discovery für dieses Gerät
 *
 * Erstellt automatisch:
 * - Schalter für Display-Animation (ON/OFF)
 * - Schalter für LEDs (ALL OFF)
 *
 * Muss aufgerufen werden, sobald MQTT verbunden ist.
 */
void publishHADiscovery(PubSubClient& client);
