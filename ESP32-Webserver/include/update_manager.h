#pragma once
#include <Arduino.h>

struct UpdateInfo
{
    String currentVersion;
    String latestVersion;
    bool updateAvailable;
    uint32_t lastCheck;
};

void updateInit();
void updateLoop();
UpdateInfo &getUpdateInfo();
bool updateHasChanged();
void clearUpdateChanged();

static String getFirmwareAssetName()
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