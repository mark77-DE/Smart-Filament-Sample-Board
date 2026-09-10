#include "update_manager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "version_info.h"
#include "config.h"
#include "globals.h"
#include <Update.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

static bool changed = false;

static uint32_t updateIntervalMs = 3 * 60 * 1000UL;
static uint32_t initialDelayMs = 2 * 60 * 1000UL;

static bool updateTaskRunning = false;
static bool updateResultReady = false;
static String latestVersionBuffer = "";

static UpdateInfo g_updateInfo;

// ----------------------------------------
void updateInit()
{
    g_updateInfo.currentVersion = FIRMWARE_VERSION;
    g_updateInfo.latestVersion = "";
    g_updateInfo.updateAvailable = false;

    // Apply configuration
    if (CONFIGV2.system.updateCheckInterval > 0)
    {
        updateIntervalMs = CONFIGV2.system.updateCheckInterval * 60 * 1000UL;
    }

    // erster Check nach initialDelay
    g_updateInfo.lastCheck = millis() - (updateIntervalMs - initialDelayMs);

    changed = true;

    if (CONFIGV2.system.debugMode)
    {
        Serial.println("[UPDATE-CHECK] Init done. Current version: " + g_updateInfo.currentVersion);
        Serial.println("[UPDATE-CHECK] Interval (s): " + String(updateIntervalMs / 1000));
        Serial.println("[UPDATE-CHECK] URL: " + String(FW_VERSION_URL));
        Serial.println("[UPDATE-CHECK] Binary URL: " + String(FW_BINARY_URL));
    }
}

// ----------------------------------------
bool checkForUpdate(String &latestVersion)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        if (CONFIGV2.system.debugMode)
        {
            Serial.println("[UPDATE-CHECK] WiFi not connected, skipping check.");
        }
        return false;
    }

    if (CONFIGV2.system.debugMode)
    {
        Serial.println("[UPDATE-CHECK] Checking for update...");
        Serial.println("[UPDATE-CHECK] Uptime: " + String(millis() / 1000) + " seconds");
        Serial.println("[UPDATE-CHECK] URL: " + String(FW_VERSION_URL));
        Serial.println("[UPDATE-CHECK] Binary URL: " + String(FW_BINARY_URL));
    }

    HTTPClient http;
    http.setTimeout(2000);

    const char *url = FW_VERSION_URL;
    bool isHttps = String(url).startsWith("https://");

    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    bool beginOk;
    if (isHttps)
    {
        secureClient.setInsecure();
        beginOk = http.begin(secureClient, url);
    }
    else
    {
        beginOk = http.begin(plainClient, url);
    }

    if (!beginOk)
    {
        if (CONFIGV2.system.debugMode)
        {
            Serial.println("[UPDATE-CHECK] http.begin() failed");
        }
        return false;
    }

    if (CONFIGV2.system.debugMode) Serial.println("[HEAP] Checkpoint A (before GET)");
    heap_caps_check_integrity_all(true);

    int httpCode = http.GET();

    if (CONFIGV2.system.debugMode) Serial.println("[HEAP] Checkpoint B (after GET)");
    heap_caps_check_integrity_all(true);

    if (CONFIGV2.system.debugMode)
    {
        Serial.println("[UPDATE-CHECK] HTTP Code: " + String(httpCode));
    }

    if (httpCode != 200)
    {
        if (CONFIGV2.system.debugMode)
        {
            Serial.println("[UPDATE-CHECK] HTTP request failed");
        }
        http.end();

        if (CONFIGV2.system.debugMode) Serial.println("[HEAP] Checkpoint C-fail (after http.end())");
        heap_caps_check_integrity_all(true);

        return false;
    }

    latestVersion = http.getString();
    latestVersion.trim();

    if (CONFIGV2.system.debugMode) Serial.println("[HEAP] Checkpoint C (after getString)");
    heap_caps_check_integrity_all(true);

    LATEST_FIRMWARE_VERSION = latestVersion;

    if (CONFIGV2.system.debugMode)
    {
        Serial.println("[UPDATE-CHECK] Latest version fetched: " + latestVersion);
    }

    http.end();

    if (CONFIGV2.system.debugMode) Serial.println("[HEAP] Checkpoint D (after http.end())");
    heap_caps_check_integrity_all(true);

    return latestVersion.length() > 0;
}

// ----------------------------------------
int compareVersion(const String &v1, const String &v2)
{
    int a1 = 0, b1 = 0, c1 = 0;
    int a2 = 0, b2 = 0, c2 = 0;

    String v1clean = v1;
    String v2clean = v2;

    if (v1clean.startsWith("v") || v1clean.startsWith("V"))
        v1clean = v1clean.substring(1);
    if (v2clean.startsWith("v") || v2clean.startsWith("V"))
        v2clean = v2clean.substring(1);

    sscanf(v1clean.c_str(), "%d.%d.%d", &a1, &b1, &c1);
    sscanf(v2clean.c_str(), "%d.%d.%d", &a2, &b2, &c2);

    if (a2 != a1)
        return a2 - a1;
    if (b2 != b1)
        return b2 - b1;
    return c2 - c1;
}

// ----------------------------------------
bool isUpdateAvailable(const String &current, const String &latest)
{
    bool available = compareVersion(current, latest) > 0;

    if (CONFIGV2.system.debugMode)
    {
        Serial.println("[UPDATE-CHECK] Compare versions: Current=" + current +
                       " Latest=" + latest +
                       " -> UpdateAvailable=" + String(available));
    }

    return available;
}

// ----------------------------------------
// 🔹 Task (JETZT die EINZIGE Stelle mit HTTP)
void updateTask(void *parameter)
{
    String latest;

    if (checkForUpdate(latest))
    {
        latestVersionBuffer = latest;
        updateResultReady = true;
    }

    updateTaskRunning = false;
    vTaskDelete(NULL);
}

// ----------------------------------------
void startUpdateTask()
{
    if (updateTaskRunning)
    {
        if (CONFIGV2.system.debugMode)
        {
            Serial.println("[UPDATE-CHECK] Task already running, skip.");
        }
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
        tskNO_AFFINITY);
}

// ----------------------------------------
void updateLoop()
{
    uint32_t now = millis();

    // Configuration change
    if (CONFIGV2.system.updateCheckInterval != updateIntervalMs / (60 * 1000UL))
    {
        updateIntervalMs = CONFIGV2.system.updateCheckInterval * 60 * 1000UL;
        Serial.println("[UPDATE-CHECK] Update interval changed to " + String(updateIntervalMs) + " ms");
    }

    // 🔹 Zeit noch nicht erreicht
    if (now - g_updateInfo.lastCheck >= updateIntervalMs)
    {
        g_updateInfo.lastCheck = now;

        if (CONFIGV2.system.debugMode)
        {
            Serial.println("[UPDATE-CHECK] Trigger async update check...");
        }
        startUpdateTask();
    }

    // 🔹 Ergebnis verarbeiten (NON-BLOCKING)
    if (updateResultReady)
    {
        updateResultReady = false;

        if (latestVersionBuffer != g_updateInfo.latestVersion)
        {
            if (CONFIGV2.system.debugMode)
            {
                Serial.println("[UPDATE-CHECK] New version detected!");
            }
            changed = true;
        }

        g_updateInfo.latestVersion = latestVersionBuffer;
        g_updateInfo.updateAvailable =
            isUpdateAvailable(g_updateInfo.currentVersion, latestVersionBuffer);
    }
}

// ----------------------------------------
UpdateInfo &getUpdateInfo()
{
    return g_updateInfo;
}

bool updateHasChanged()
{
    return changed;
}

void clearUpdateChanged()
{
    changed = false;
}

// ----------------------------------------
// Self-update status
// ----------------------------------------

static SelfUpdateStatus g_selfUpdateStatus = {
    false,
    false,
    false,
    0,
    ""};

static bool selfUpdateTaskRunning = false;

// ----------------------------------------
// Get self-update status
// ----------------------------------------

SelfUpdateStatus &getSelfUpdateStatus()
{
    return g_selfUpdateStatus;
}

// ----------------------------------------
// Get firmware asset name
// ----------------------------------------

String getFirmwareAssetName()
{
#if defined(BOARD_VARIANT)

    String board = BOARD_VARIANT;

#if DISPLAY_TYPE == DISPLAY_TYPE_SH1106

    if (board == "dev-kit-v1")
        return "esp32-sh1106-firmware.bin";

    if (board == "esp32-s3-zero")
        return "esp32-s3-sh1106-firmware.bin";

#elif DISPLAY_TYPE == DISPLAY_TYPE_ST7789

    if (board == "dev-kit-v1")
        return "esp32-st7789-firmware.bin";

    if (board == "esp32-s3-zero")
        return "esp32-s3-st7789-firmware.bin";

#endif

#endif

    return "";
}

// ----------------------------------------
// Get firmware download URL
// ----------------------------------------

static String getFirmwareDownloadUrl(const String &version)
{
    String asset = getFirmwareAssetName();

    if (asset.length() == 0)
        return "";

    return FW_BINARY_URL +
           version + "/" + asset;
}

// ----------------------------------------
// Self-update task
// ----------------------------------------

static void selfUpdateTask(void *parameter)
{
    selfUpdateTaskRunning = true;

    String latestVersion = g_updateInfo.latestVersion;
    String firmwareAsset = getFirmwareAssetName();

    if (CONFIGV2.system.debugMode)
    {
        Serial.println("[SELF-UPDATE] Task started.");
        Serial.println("[SELF-UPDATE] Current version: " +
                       g_updateInfo.currentVersion);
        Serial.println("[SELF-UPDATE] Latest version: " +
                       latestVersion);
        Serial.println("[SELF-UPDATE] Firmware asset: " +
                       firmwareAsset);
    }

    // ----------------------------------------
    // Validate latest version
    // ----------------------------------------

    if (latestVersion.length() == 0)
    {
        Serial.println("[SELF-UPDATE] Latest version is unknown.");

        g_selfUpdateStatus.running = false;
        g_selfUpdateStatus.finished = true;
        g_selfUpdateStatus.success = false;
        g_selfUpdateStatus.message = "Latest version unknown";

        selfUpdateTaskRunning = false;
        vTaskDelete(NULL);
        return;
    }

    // ----------------------------------------
    // Validate firmware asset
    // ----------------------------------------

    if (firmwareAsset.length() == 0)
    {
        Serial.println(
            "[SELF-UPDATE] No firmware asset available for this target.");

        g_selfUpdateStatus.running = false;
        g_selfUpdateStatus.finished = true;
        g_selfUpdateStatus.success = false;
        g_selfUpdateStatus.message = "Unsupported firmware target";

        selfUpdateTaskRunning = false;
        vTaskDelete(NULL);
        return;
    }

    // ----------------------------------------
    // Check WiFi connection
    // ----------------------------------------

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[SELF-UPDATE] WiFi not connected.");

        g_selfUpdateStatus.running = false;
        g_selfUpdateStatus.finished = true;
        g_selfUpdateStatus.success = false;
        g_selfUpdateStatus.message = "WiFi not connected";

        selfUpdateTaskRunning = false;
        vTaskDelete(NULL);
        return;
    }

    // ----------------------------------------
    // Build firmware URL
    // ----------------------------------------

    String firmwareUrl = getFirmwareDownloadUrl(latestVersion);

    if (firmwareUrl.length() == 0)
    {
        Serial.println("[SELF-UPDATE] Failed to build firmware URL.");

        g_selfUpdateStatus.running = false;
        g_selfUpdateStatus.finished = true;
        g_selfUpdateStatus.success = false;
        g_selfUpdateStatus.message = "Invalid firmware URL";

        selfUpdateTaskRunning = false;
        vTaskDelete(NULL);
        return;
    }

    if (CONFIGV2.system.debugMode)
    {
        Serial.println("[SELF-UPDATE] Firmware URL:");
        Serial.println(firmwareUrl);
    }

    g_selfUpdateStatus.message = "Connecting to firmware server";

    // ----------------------------------------
    // Start HTTPS or lokal http connection
    // ----------------------------------------

    bool isHttps = String(firmwareUrl).startsWith("https://");

    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    HTTPClient http;
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

    bool beginOk;
    if (isHttps)
    {
        secureClient.setInsecure();
        beginOk = http.begin(secureClient, firmwareUrl);
    }
    else
    {
        beginOk = http.begin(plainClient, firmwareUrl);
    }

    if (!beginOk)
    {
        Serial.println("[SELF-UPDATE] HTTP begin failed.");

        g_selfUpdateStatus.running = false;
        g_selfUpdateStatus.finished = true;
        g_selfUpdateStatus.success = false;
        g_selfUpdateStatus.message = "HTTP connection failed";

        selfUpdateTaskRunning = false;
        vTaskDelete(NULL);
        return;
    }

    if (CONFIGV2.system.debugMode)
        Serial.println("[SELF-UPDATE] Starting firmware download...");

    // in checkForUpdate(), vor und nach dem HTTP-Request
    heap_caps_check_integrity_all(true); // vorher: sollte noch OK sein

    int httpCode = http.GET();

    heap_caps_check_integrity_all(true); // nachher: hier crasht es jetzt ggf. schon,
                                         // statt erst Minuten später im tcpip_thread

    if (CONFIGV2.system.debugMode)
    {
        Serial.println("[SELF-UPDATE] HTTP response code: " +
                       String(httpCode));
    }

    if (httpCode != HTTP_CODE_OK)
    {
        Serial.println(
            "[SELF-UPDATE] Firmware download failed. HTTP code: " +
            String(httpCode));

        g_selfUpdateStatus.running = false;
        g_selfUpdateStatus.finished = true;
        g_selfUpdateStatus.success = false;
        g_selfUpdateStatus.message = "Firmware download failed";

        http.end();

        selfUpdateTaskRunning = false;
        vTaskDelete(NULL);
        return;
    }

    // ----------------------------------------
    // Get firmware size
    // ----------------------------------------

    int totalSize = http.getSize();

    if (totalSize <= 0)
    {
        Serial.println(
            "[SELF-UPDATE] Invalid firmware size: " +
            String(totalSize));

        g_selfUpdateStatus.running = false;
        g_selfUpdateStatus.finished = true;
        g_selfUpdateStatus.success = false;
        g_selfUpdateStatus.message = "Invalid firmware size";

        http.end();

        selfUpdateTaskRunning = false;
        vTaskDelete(NULL);
        return;
    }

    if (CONFIGV2.system.debugMode)
    {
        Serial.println("[SELF-UPDATE] Firmware size: " +
                       String(totalSize) +
                       " bytes");
    }

    // ----------------------------------------
    // Get download stream
    // ----------------------------------------

    WiFiClient *stream = http.getStreamPtr();

    if (stream == nullptr)
    {
        Serial.println(
            "[SELF-UPDATE] Firmware download stream unavailable.");

        g_selfUpdateStatus.running = false;
        g_selfUpdateStatus.finished = true;
        g_selfUpdateStatus.success = false;
        g_selfUpdateStatus.message = "Download stream unavailable";

        http.end();

        selfUpdateTaskRunning = false;
        vTaskDelete(NULL);
        return;
    }

    // ----------------------------------------
    // Start OTA update
    // ----------------------------------------

    if (!Update.begin(totalSize))
    {
        Serial.println(
            "[SELF-UPDATE] Update.begin() failed. Error: " +
            String(Update.getError()));

        g_selfUpdateStatus.running = false;
        g_selfUpdateStatus.finished = true;
        g_selfUpdateStatus.success = false;
        g_selfUpdateStatus.message = "OTA begin failed";

        http.end();

        selfUpdateTaskRunning = false;
        vTaskDelete(NULL);
        return;
    }

    g_selfUpdateStatus.message = "Downloading firmware";

    // ----------------------------------------
    // Download and write firmware
    // ----------------------------------------

    uint8_t buffer[4096];

    size_t totalWritten = 0;

    uint8_t lastProgress = 0;

    uint32_t lastDataTime = millis();

    while (totalWritten < (size_t)totalSize)
    {
        size_t available = stream->available();

        if (available == 0)
        {
            if (!http.connected())
            {
                Serial.println("[SELF-UPDATE] Connection lost.");

                Update.abort();

                g_selfUpdateStatus.running = false;
                g_selfUpdateStatus.finished = true;
                g_selfUpdateStatus.success = false;
                g_selfUpdateStatus.message = "Download connection lost";

                http.end();

                selfUpdateTaskRunning = false;
                vTaskDelete(NULL);
                return;
            }

            if (millis() - lastDataTime > 15000)
            {
                Serial.println("[SELF-UPDATE] Download timeout.");

                Update.abort();

                g_selfUpdateStatus.running = false;
                g_selfUpdateStatus.finished = true;
                g_selfUpdateStatus.success = false;
                g_selfUpdateStatus.message = "Download timeout";

                http.end();

                selfUpdateTaskRunning = false;
                vTaskDelete(NULL);
                return;
            }

            delay(1);
            continue;
        }

        // Determine amount of data to read
        size_t remaining = (size_t)totalSize - totalWritten;

        size_t toRead = available;

        if (toRead > sizeof(buffer))
            toRead = sizeof(buffer);

        if (toRead > remaining)
            toRead = remaining;

        // Read from HTTP stream
        int len = stream->readBytes(buffer, toRead);

        if (len <= 0)
            continue;

        lastDataTime = millis();

        // Write to OTA partition
        size_t written = Update.write(buffer, len);

        if (written != (size_t)len)
        {
            Serial.println(
                "[SELF-UPDATE] Firmware write failed. Error: " +
                String(Update.getError()));

            Update.abort();

            g_selfUpdateStatus.running = false;
            g_selfUpdateStatus.finished = true;
            g_selfUpdateStatus.success = false;
            g_selfUpdateStatus.message = "Firmware write failed";

            http.end();

            selfUpdateTaskRunning = false;
            vTaskDelete(NULL);
            return;
        }

        totalWritten += written;

        // ----------------------------------------
        // Update progress in 5% steps
        // ----------------------------------------

        uint8_t progress =
            (uint8_t)((totalWritten * 100UL) / totalSize);

        uint8_t progressStep = (progress / 5) * 5;

        if (progressStep > lastProgress)
        {
            lastProgress = progressStep;

            g_selfUpdateStatus.progress = progressStep;
            g_selfUpdateStatus.message =
                "Downloading firmware " +
                String(progressStep) +
                "%";

            if (CONFIGV2.system.debugMode)
            {
                Serial.println(
                    "[SELF-UPDATE] Progress: " +
                    String(progressStep) +
                    "%");
            }
        }

        yield();
    }

    // ----------------------------------------
    // Finalize OTA update
    // ----------------------------------------

    g_selfUpdateStatus.progress = 100;
    g_selfUpdateStatus.message = "Finalizing firmware update";

    if (CONFIGV2.system.debugMode)
        Serial.println("[SELF-UPDATE] Finalizing OTA update...");

    if (!Update.end(false))
    {
        Serial.println(
            "[SELF-UPDATE] Update.end() failed. Error: " +
            String(Update.getError()));

        g_selfUpdateStatus.running = false;
        g_selfUpdateStatus.finished = true;
        g_selfUpdateStatus.success = false;
        g_selfUpdateStatus.message = "OTA finalize failed";

        http.end();

        selfUpdateTaskRunning = false;
        vTaskDelete(NULL);
        return;
    }

    // ----------------------------------------
    // Check OTA result
    // ----------------------------------------

    if (!Update.isFinished())
    {
        Serial.println("[SELF-UPDATE] OTA update is not finished.");

        g_selfUpdateStatus.running = false;
        g_selfUpdateStatus.finished = true;
        g_selfUpdateStatus.success = false;
        g_selfUpdateStatus.message = "OTA not finished";

        http.end();

        selfUpdateTaskRunning = false;
        vTaskDelete(NULL);
        return;
    }

    http.end();

    // ----------------------------------------
    // Update successful
    // ----------------------------------------

    g_selfUpdateStatus.running = false;
    g_selfUpdateStatus.finished = true;
    g_selfUpdateStatus.success = true;
    g_selfUpdateStatus.progress = 100;
    g_selfUpdateStatus.message = "Firmware update successful";

    Serial.println("[SELF-UPDATE] Firmware update successful.");

    // ----------------------------------------
    // Reboot after successful update
    // ----------------------------------------

    g_selfUpdateStatus.message = "Firmware update successful, rebooting";

    delay(1000);

    ESP.restart();

    // Should never be reached
    selfUpdateTaskRunning = false;
    vTaskDelete(NULL);
}

// ----------------------------------------
// Start self-update (OTA) process
// ----------------------------------------

bool startSelfUpdate()
{
    // Prevent starting another update
    if (selfUpdateTaskRunning ||
        g_selfUpdateStatus.running)
    {
        if (CONFIGV2.system.debugMode)
            Serial.println(
                "[SELF-UPDATE] Update already running.");

        return false;
    }

    // Reset status
    g_selfUpdateStatus.running = true;
    g_selfUpdateStatus.finished = false;
    g_selfUpdateStatus.success = false;
    g_selfUpdateStatus.progress = 0;
    g_selfUpdateStatus.message = "Starting firmware update";

    if (CONFIGV2.system.debugMode)
    {
        Serial.println(
            "[SELF-UPDATE] Starting self-update process...");
    }

    // Check WiFi before starting the task
    if (WiFi.status() != WL_CONNECTED)
    {
        g_selfUpdateStatus.running = false;
        g_selfUpdateStatus.finished = true;
        g_selfUpdateStatus.success = false;
        g_selfUpdateStatus.message = "WiFi not connected";

        Serial.println("[SELF-UPDATE] WiFi not connected.");

        return false;
    }

    // Start self-update task
    BaseType_t result = xTaskCreatePinnedToCore(
        selfUpdateTask,
        "selfUpdateTask",
        12288,
        nullptr,
        1,
        nullptr,
        1);

    if (result != pdPASS)
    {
        Serial.println(
            "[SELF-UPDATE] Failed to create update task.");

        g_selfUpdateStatus.running = false;
        g_selfUpdateStatus.finished = true;
        g_selfUpdateStatus.success = false;
        g_selfUpdateStatus.message = "Failed to start update task";

        return false;
    }

    return true;
}
