#pragma once
#include <Arduino.h>

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

void updateInit();
void updateLoop();
UpdateInfo &getUpdateInfo();
bool updateHasChanged();
void clearUpdateChanged();

// start self update (OTA) process)
bool startSelfUpdate();

// get self update status
SelfUpdateStatus& getSelfUpdateStatus();

String getFirmwareAssetName();



#ifdef OTA_DEBUG_LOCAL_SERVER
  #define FW_VERSION_URL  "http://192.168.0.197:8000/version.txt"
  #define FW_BINARY_URL   "http://192.168.0.197:8000/"
#else
  #define FW_VERSION_URL  "https://raw.githubusercontent.com/mark77-DE/Smart-Filament-Sample-Board/refs/heads/main/ESP32-Webserver/version.txt"
  #define FW_BINARY_URL   "https://github.com/mark77-DE/Smart-Filament-Sample-Board/releases/download/"
#endif

