#pragma once
#include "time_manager.h"
#include "config.h"   // wherever CONFIGV2 is declared

// Converts a tag to upper case for consistent log formatting
inline String upperTag(const char* tag) {
    String s(tag);
    s.toUpperCase();
    return s;
}

#define DEBUG_LOG(tag, msg) \
    do { \
        if (CONFIGV2.system.debugMode) { \
            Serial.println("[" + TimeManager::getDebugTimestamp() + "] [" + upperTag(tag) + "] " + (msg)); \
        } \
    } while (0)

#define DEBUG_LOGF(tag, fmt, ...) \
    do { \
        if (CONFIGV2.system.debugMode) { \
            Serial.printf("[%s] [%s] " fmt "\n", TimeManager::getDebugTimestamp().c_str(), upperTag(tag).c_str(), ##__VA_ARGS__); \
        } \
    } while (0)

#define DEBUG_BLOCK(code) \
    do { if (CONFIGV2.system.debugMode) { code; } } while (0)


// debug_utils.h — always-on logging, still timestamped, no debugMode gate
#define LOG_LN(tag, msg) \
    Serial.println("[" + TimeManager::getDebugTimestamp() + "] [" + upperTag(tag) + "] " + (msg))

#define LOG_LNF(tag, fmt, ...) \
    Serial.printf("[%s] [%s] " fmt "\n", TimeManager::getDebugTimestamp().c_str(), upperTag(tag).c_str(), ##__VA_ARGS__)

    