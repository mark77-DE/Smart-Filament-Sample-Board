// time_manager.h
#pragma once
#include <Arduino.h>

namespace TimeManager {
    void init();
    void loop();          // NEU - regelmäßig aufrufen
    bool isSynced();
    String getTimestampISO();
    time_t getEpoch();
}