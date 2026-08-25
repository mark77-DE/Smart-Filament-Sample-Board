#pragma once
#include <Arduino.h>

struct UpdateInfo {
    String currentVersion;
    String latestVersion;
    bool updateAvailable;
    uint32_t lastCheck;
};

void updateInit();
void updateLoop();
UpdateInfo& getUpdateInfo();
bool updateHasChanged();
void clearUpdateChanged();