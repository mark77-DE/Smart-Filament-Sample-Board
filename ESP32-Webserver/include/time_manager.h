// time_manager.h
#pragma once
#include <Arduino.h>

namespace TimeManager {
    void init();
    void loop();          // NEW - call regularly
    bool isSynced();
    String getTimestampISO();
    time_t getEpoch();
}