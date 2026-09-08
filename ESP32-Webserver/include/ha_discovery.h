#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include "config.h"

/**
 * @brief Publishes Home Assistant MQTT discovery for this device
 *
 * Erstellt automatisch:
 * - Switch for display animation (ON/OFF)
 * - Switch for LEDs (ALL OFF)
 *
 * Muss aufgerufen werden, sobald MQTT verbunden ist.
 */
#pragma once
#include <PubSubClient.h>

void publishHADiscovery(
    PubSubClient& client,
    const String& discoveryPrefix
);

