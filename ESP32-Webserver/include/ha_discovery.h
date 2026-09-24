#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include "config.h"

/**
 * @brief Publishes Home Assistant MQTT discovery for this device
 *
 * Creates automatically:
 * - Switch for display animation (ON/OFF)
 * - Switch for LEDs (ALL OFF)
 *
 * Must be called once MQTT is connected.
 */
#pragma once
#include <PubSubClient.h>

/**
 * @brief Publishes Home Assistant MQTT discovery payloads for the device.
 * @param client MQTT client used for publishing discovery messages.
 * @param discoveryPrefix Home Assistant discovery prefix, e.g. "homeassistant".
 */
void publishHADiscovery(
    PubSubClient& client,
    const String& discoveryPrefix
);

