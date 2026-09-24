// mqtt_manager.h
#pragma once
#include <PubSubClient.h>
#include <WiFi.h>
#include "filament_db.h"

/**
 * @brief Initializes the MQTT client and schedules the first connection attempt.
 */
void mqttInit();

/**
 * @brief Processes MQTT housekeeping and reconnect logic.
 * @note Call once per loop iteration.
 */
void mqttLoop();

/**
 * @brief Reports whether the MQTT connection is currently active.
 * @return true if connected, otherwise false.
 */
bool mqttIsConnected();

/**
 * @brief Publishes the current animation state.
 * @param on true when animation is enabled, false when disabled.
 */
void publishAnimationStatus(bool on);

/**
 * @brief Publishes the current filament state for a known filament.
 * @param entry Filament entry to publish.
 */
void publishFilamentState(const FilamentEntry& entry);



/**
 * @brief Publishes the fw update available true or false.
 */
void publishUpdateStatus();