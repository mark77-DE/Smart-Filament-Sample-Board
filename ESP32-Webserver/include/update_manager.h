#pragma once
#include <Arduino.h>

/**
 * @brief Runtime state for the current firmware update check.
 */
struct UpdateInfo
{
    String currentVersion;
    String latestVersion;
    bool updateAvailable;
    uint32_t lastCheck;
};

struct SelfUpdateStatus {
    bool running;       // true if self-update is in progress
    bool finished;      // true if self-update has finished (success or failure)    
    bool success;       // true if self-update was successful
    uint8_t progress;   // progress percentage (0-100)
    String message;     //  status message (e.g., "Starting update", "Downloading...", "Update successful", "Update failed", etc.)
};

/**
 * @brief Initializes the update-check subsystem and configures the interval state.
 */
void updateInit();

/**
 * @brief Runs the periodic update check logic.
 * @note Call regularly from the main loop.
 */
void updateLoop();

/**
 * @brief Returns the current update metadata.
 * @return Reference to the live update state.
 */
UpdateInfo &getUpdateInfo();

/**
 * @brief Reports whether the update state has changed since the last reset.
 * @return true if a new update status was detected, otherwise false.
 */
bool updateHasChanged();

/**
 * @brief Clears the update-changed flag after processing.
 */
void clearUpdateChanged();

/**
 * @brief Starts the OTA self-update process.
 * @return true if the update process was started, otherwise false.
 */
bool startSelfUpdate();

/**
 * @brief Returns the current self-update status.
 * @return Reference to the live self-update state.
 */
SelfUpdateStatus& getSelfUpdateStatus();

/**
 * @brief Gets the firmware asset name used for OTA downloads.
 * @return Asset filename for the current platform.
 */
String getFirmwareAssetName();



#ifdef OTA_DEBUG_LOCAL_SERVER
  #include "ota_local_server_generated.h"   // wird von dev_release.py geschrieben
  #define FW_VERSION_URL  OTA_LOCAL_SERVER_URL_BASE "version.txt"
  #define FW_BINARY_URL   OTA_LOCAL_SERVER_URL_BASE
#else
  #define FW_VERSION_URL  "https://raw.githubusercontent.com/mark77-DE/Smart-Filament-Sample-Board/refs/heads/main/ESP32-Webserver/version.txt"
  #define FW_BINARY_URL   "https://github.com/mark77-DE/Smart-Filament-Sample-Board/releases/download/"
#endif

