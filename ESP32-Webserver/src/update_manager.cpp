#include "update_manager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "version_info.h"
#include "config.h"

static UpdateInfo g_updateInfo;
static bool changed = false;

static uint32_t updateIntervalMs = 3*60*1000UL;
static uint32_t initialDelayMs  = 2*60*1000UL;

static bool updateTaskRunning = false;
static bool updateResultReady = false;
static String latestVersionBuffer = "";

// ----------------------------------------
void updateInit() {
    g_updateInfo.currentVersion = FIRMWARE_VERSION;
    g_updateInfo.latestVersion = "";
    g_updateInfo.updateAvailable = false;

    // 🔹 Config übernehmen
    if (CONFIGV2.system.updateCheckInterval > 0) {
        updateIntervalMs = CONFIGV2.system.updateCheckInterval * 60 * 1000UL;
    }

    // erster Check nach initialDelay
    g_updateInfo.lastCheck = millis() - (updateIntervalMs - initialDelayMs);

    changed = true;

    Serial.println("[UPDATE] Init done. Current version: " + g_updateInfo.currentVersion);
    Serial.println("[UPDATE] Interval (ms): " + String(updateIntervalMs));
}

// ----------------------------------------
bool checkForUpdate(String& latestVersion) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[UPDATE] WiFi not connected, skipping check.");
        return false;
    }

    Serial.println("[UPDATE] Checking for update...");
    Serial.println("[UPDATE] Uptime: " + String(millis() / 1000) + " seconds");

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(2000); // 🔹 wichtig gegen lange Hänger

    const char* url = "https://raw.githubusercontent.com/mark77-DE/Smart-Filament-Sample-Board/refs/heads/main/ESP32-Webserver/version.txt";
    http.begin(client, url);

    int httpCode = http.GET();
    Serial.println("[UPDATE] HTTP Code: " + String(httpCode));

    if (httpCode != 200) {
        Serial.println("[UPDATE] HTTP request failed");
        http.end();
        return false;
    }

    latestVersion = http.getString();
    latestVersion.trim();

    Serial.println("[UPDATE] Latest version fetched: " + latestVersion);

    http.end();
    return latestVersion.length() > 0;
}

// ----------------------------------------
int compareVersion(const String& v1, const String& v2) {
    int a1=0,b1=0,c1=0;
    int a2=0,b2=0,c2=0;

    String v1clean = v1;
    String v2clean = v2;

    if (v1clean.startsWith("v") || v1clean.startsWith("V")) v1clean = v1clean.substring(1);
    if (v2clean.startsWith("v") || v2clean.startsWith("V")) v2clean = v2clean.substring(1);

    sscanf(v1clean.c_str(), "%d.%d.%d", &a1,&b1,&c1);
    sscanf(v2clean.c_str(), "%d.%d.%d", &a2,&b2,&c2);

    if (a2!=a1) return a2-a1;
    if (b2!=b1) return b2-b1;
    return c2-c1;
}

// ----------------------------------------
bool isUpdateAvailable(const String& current, const String& latest) {
    bool available = compareVersion(current, latest) > 0;

    Serial.println("[UPDATE] Compare versions: Current=" + current +
                   " Latest=" + latest +
                   " -> UpdateAvailable=" + String(available));

    return available;
}

// ----------------------------------------
// 🔹 Task (JETZT die EINZIGE Stelle mit HTTP)
void updateTask(void * parameter) {
    String latest;

    if (checkForUpdate(latest)) {
        latestVersionBuffer = latest;
        updateResultReady = true;
    }

    updateTaskRunning = false;
    vTaskDelete(NULL);
}

// ----------------------------------------
void startUpdateTask() {
    if (updateTaskRunning) {
        Serial.println("[UPDATE] Task already running, skip.");
        return;
    }

    updateTaskRunning = true;

    xTaskCreatePinnedToCore(
        updateTask,
        "updateTask",
        8192,
        NULL,
        1,
        NULL,
        1   // Core 1
    );
}

// ----------------------------------------
void updateLoop() {
    uint32_t now = millis();

    // 🔹 Config-Änderung
    if (CONFIGV2.system.updateCheckInterval != updateIntervalMs / (60 * 1000UL)) {
        updateIntervalMs = CONFIGV2.system.updateCheckInterval * 60 * 1000UL;
        Serial.println("[UPDATE] Update interval changed to " + String(updateIntervalMs) + " ms");
    }

    // 🔹 Zeit noch nicht erreicht
    if (now - g_updateInfo.lastCheck >= updateIntervalMs) {
        g_updateInfo.lastCheck = now;

        Serial.println("[UPDATE] Trigger async update check...");
        startUpdateTask();
    }

    // 🔹 Ergebnis verarbeiten (NON-BLOCKING)
    if (updateResultReady) {
        updateResultReady = false;

        if (latestVersionBuffer != g_updateInfo.latestVersion) {
            Serial.println("[UPDATE] New version detected!");
            changed = true;
        }

        g_updateInfo.latestVersion = latestVersionBuffer;
        g_updateInfo.updateAvailable =
            isUpdateAvailable(g_updateInfo.currentVersion, latestVersionBuffer);
    }
}

// ----------------------------------------
UpdateInfo& getUpdateInfo() {
    return g_updateInfo;
}

bool updateHasChanged() {
    return changed;
}

void clearUpdateChanged() {
    changed = false;
}