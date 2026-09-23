// time_manager.h
#pragma once
#include <Arduino.h>

namespace TimeManager {

/**
 * @brief Initializes the time manager and synchronizes the local clock state.
 */
void init();

/**
 * @brief Processes periodic time synchronization and clock maintenance.
 * @note Call once per loop iteration.
 */
void loop();

/**
 * @brief Checks whether the device clock is synchronized.
 * @return true if the time source is valid, otherwise false.
 */
bool isSynced();

/**
 * @brief Formats the current time as an ISO-8601 string.
 * @return Current timestamp as ISO string.
 */
String getTimestampISO();

/**
 * @brief Returns the current Unix epoch timestamp.
 * @return Epoch time in seconds.
 */
time_t getEpoch();

}