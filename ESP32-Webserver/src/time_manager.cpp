// time_manager.cpp
#include "time_manager.h"
#include "debug_utils.h"
#include <time.h>
#include <sys/time.h>
#include "config.h"

namespace {
    bool s_synced = false;
    bool s_syncLogged = false;   // prevents log spam

    uint32_t s_resyncIntervalMs = 8UL * 60UL * 60UL * 1000UL;   // Fair use, min interval should be 17 minutes
    unsigned long s_lastSyncAttempt = 0;
}

namespace TimeManager {

void doSync() {
    String tz = CONFIGV2.system.timezone;
    if (tz.length() == 0) {
        tz = TIMEZONE;  // fallback if migration/config is empty
    }

    configTzTime(tz.c_str(), TIMESERVER_1, TIMESERVER_2, TIMESERVER_3);
    s_lastSyncAttempt = millis();

    DEBUG_LOGF("TIME", "NTP sync triggered. TZ=%s", CONFIGV2.system.timezone.c_str());
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

    // Re-sync timer
    if (now - s_lastSyncAttempt >= s_resyncIntervalMs) {
        DEBUG_LOG("TIME", "Periodic re-sync triggered.");
        s_syncLogged = false;   // log again once synced after re-sync
        doSync();
    }

    // Actively check for sync success and log once
    if (!s_syncLogged && isSynced()) {
        s_syncLogged = true;
        DEBUG_LOGF("TIME", "NTP sync successful. Current time: %s", getTimestampISO().c_str());
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

String getDebugTimestamp() {
    if (isSynced()) {
        struct timeval tv;
        gettimeofday(&tv, nullptr);

        struct tm timeinfo;
        localtime_r(&tv.tv_sec, &timeinfo);

        char buf[16];
        strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);

        char result[24];
        snprintf(result, sizeof(result), "%s.%03ld", buf, tv.tv_usec / 1000);
        return String(result);
    }
    return String(millis()) + "ms";
}

} // namespace TimeManager