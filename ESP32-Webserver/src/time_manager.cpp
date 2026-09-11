// time_manager.cpp
#include "time_manager.h"
#include <time.h>
#include "config.h"

namespace {
    bool s_synced = false;
    bool s_syncLogged = false;   // NEU: verhindert Spam im Log

    uint32_t s_resyncIntervalMs = 8UL * 60UL * 60UL * 1000UL;   //Fair use, min interval should be 17 minutes
    unsigned long s_lastSyncAttempt = 0;
}

namespace TimeManager {

void doSync() {
    String tz = CONFIGV2.system.timezone;
    if (tz.length() == 0) {
        tz = TIMEZONE;  // Fallback, falls Migration/Config leer ist
    }

    configTzTime(tz.c_str(), TIMESERVER_1, TIMESERVER_2, TIMESERVER_3);

    s_lastSyncAttempt = millis();

    if (CONFIGV2.system.debugMode) {
        Serial.println("[TIME] NTP sync triggered. TZ=" + CONFIGV2.system.timezone);
    }
}

void init() {
    doSync();
}

bool isSynced() {
    if (s_synced) return true;

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) {
        if (timeinfo.tm_year + 1900 > 2020) {
            s_synced = true;
        }
    }
    return s_synced;
}

void loop() {
    unsigned long now = millis();

    // Re-Sync-Timer
    if (now - s_lastSyncAttempt >= s_resyncIntervalMs) {
        if (CONFIGV2.system.debugMode) {
            Serial.println("[TIME] Periodic re-sync triggered.");
        }
        s_syncLogged = false;   // log again once synced after re-sync
        doSync();
    }

    // Actively check for sync success and log once
    if (!s_syncLogged && isSynced()) {
        s_syncLogged = true;
        if (CONFIGV2.system.debugMode) {
            Serial.println("[TIME] NTP sync successful. Current time: " + getTimestampISO());
        }
    }
}

String getTimestampISO() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 10)) {
        return "unsynced";
    }
    char buf[25];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    return String(buf);
}

time_t getEpoch() {
    time_t now;
    time(&now);
    return now;
}

} // namespace TimeManager