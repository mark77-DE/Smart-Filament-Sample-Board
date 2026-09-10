#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#include "led_config.h"


// ============================================================================
// Configuration structures
// ============================================================================

/**
 * @brief Globale LED-Hardware-Konfiguration
 *
 * Applies to all addressable LEDs in the device.
 * Defaults come from platformio.ini.
 */
struct LedHardwareConfigV2 {
  LedType  type  = static_cast<LedType>(LED_TYPE);
  LedOrder order = static_cast<LedOrder>(LED_ORDER);
};


/**
 * @brief LED-Konfiguration (Filament-Stripe)
 */
struct LedConfigV2 {
  int      count;       ///< Number of LEDs
  int      pin;         ///< GPIO pin
  int      brightness;  ///< Brightness [0..255]
  int      timeout;     ///< Timeout in ms
  uint32_t color;       ///< Default color 0xRRGGBB
  uint32_t colorError;  ///< Error color 0xRRGGBB
  uint32_t colorPulse;  ///< Idle pulse color 0xRRGGBB
};

/**
 * @brief NFC-LED-Konfiguration
 */
struct NfcLedConfigV2 {
  int      count;                 ///< Number of LEDs
  int      pin;                   ///< GPIO pin
  int      brightness;            ///< Brightness [0..255]
  int      timeout;               ///< Timeout in ms
  uint32_t colorSuccess;          ///< Success color 0xRRGGBB
  uint32_t colorError;            ///< Error color 0xRRGGBB
  uint32_t colorPulse;            ///< Idle pulse color 0xRRGGBB
  bool     successBlinkEnabled;   ///< True = success blinking enabled
  int      successBlinkCount;     ///< Number of blink cycles
  int      successBlinkMs;        ///< Blink interval in ms
};

/**
 * @brief Button-Konfiguration
 */
struct ButtonConfigV2 {
  bool enabled       = true; ///< Button enabled?
  int  pin           = -1;   ///< Button GPIO (-1 = off)
  bool pullup        = true; ///< Internal pull-up -> active-low
  int  debounceMs    = 30;   ///< Debounce time
  int  longMs        = 800;  ///< Long-press threshold
  int  doubleGapMs   = 400;  ///< Double-press window
  int  holdRepeatMs  = 250;  ///< Hold repeat rate
};

/**
 * @brief Buzzer-Konfiguration
 */
struct BuzzerConfigV2 {
  bool enabled       = true; ///< Buzzer enabled?
  int  pin           = -1;    ///< Buzzer GPIO (-1 = off)
  bool activeHigh    = true;  ///< Active level HIGH?
  bool passive       = false; ///< false = active buzzer, true = passive (PWM)
  int  freqHz        = 4000;  ///< Frequency for tone()
  int  singleMs      = 80;    ///< Dauer Single-Beep
  int  doubleOnMs    = 60;    ///< Ein-Zeit Double-Beep
  int  doubleGapMs   = 80;    ///< Pause Double-Beep
  int  errorOnMs     = 50;    ///< Ein-Zeit Error-Sequenz
  int  errorGapMs    = 60;    ///< Pause Error-Sequenz
  int  errorCount    = 3;     ///< Wiederholungen Error-Sequenz
};

/**
 * @brief MQTT-Konfiguration
 */

struct MqttConfigV2 {
  bool enabled;
  String server;
  uint16_t port;
  String user;
  String password;
  String baseTopic;
  String clientId;
  bool haDiscovery;          ///< Home Assistant Discovery aktiv
  String haDiscoveryPrefix;  ///< Prefix for HA Discovery (e.g. "homeassistant")
};

// OPTIONAL: Marker so gpio_hardware.cpp knows that
// Button/buzzer are present in CONFIG:
#define CONFIG_HAS_GPIO

struct systemConfig {
  String        version = "error";
  bool          darkmode;    ///< Dark mode enabled
  bool          debugMode;   ///< Debug mode enabled
  uint32_t      webLEDTimeout;   // Dashboard default (ms)
  String        hostname;    ///< Wi-Fi hostname
  bool          animationAfterBoot; ///< Startup animation after boot enabled
  String        defaultLanguage; ///< Default language (e.g. "en" or "de")
  uint32_t      updateCheckInterval; ///< Seconds until reboot after long press
  String        timezone;   // POSIX-TZ-String, example "CET-1CEST,M3.5.0,M10.5.0/3"
};

/**
 * @brief Haupt-Konfigurationsstruktur der App
 */
struct AppConfigV2 {
  systemConfig system;     ///< System configuration
  uint32_t     webLEDTimeout;   // Dashboard default (ms)
  String       hostname;    ///< Wi-Fi hostname

  LedHardwareConfigV2 ledHardware;

  LedConfigV2    led;         ///< LED configuration (filament)
  NfcLedConfigV2 nfc;         ///< NFC LED configuration
  ButtonConfigV2 button;      ///< Pushbutton configuration
  BuzzerConfigV2 buzzer;      ///< Buzzer configuration
  MqttConfigV2   mqttConfig;  ///< MQTT configuration
};

// Global, currently loaded configuration
extern AppConfigV2 CONFIGV2;

// ============================================================================
// High-level API
// ============================================================================

/**
 * @brief Loads the configuration from config_v2.json into CONFIGV2
 * @return true on success, otherwise false
 */
bool loadConfigV2();

/**
 * @brief Applies the current CONFIG to the hardware/modules
 */
void applyConfigV2();

/**
 * @brief Saves CONFIGV2 to /config_v2.json
 * @return true on success, otherwise false
 */
bool saveConfigV2();



// ============================================================================
// JSON helper API
// ============================================================================

/**
 * @brief Updates CONFIG from a JSON document (doc["options"]...)
 * @param doc JSON document with configuration values
 * @return true on success, otherwise false
 */
bool updateConfigFromJsonV2(JsonDocument& doc);

/**
 * @brief Loads the configuration as a JSON object into an existing document
 * @param target Target JsonObject (will be populated)
 * @return true on success, otherwise false
 */
bool loadConfigAsJsonV2(JsonObject target);




/**
 * @brief Imports a configuration from a JSON object (writes the file)
 * @param src Source JsonObject
 * @return true on success, otherwise false
 */
bool importConfigJsonV2(JsonObject src);


// ============================================================================
// Miscellaneous
// ============================================================================



/**
 * @brief Writes a 0xRRGGBB color as an [r,g,b] array into a JsonObject
 * @param opt Target JsonObject (e.g. "options")
 * @param key Key under which the array is created
 * @param color 0xRRGGBB
 */

uint32_t colorFromArrayV2(JsonArrayConst arr);

// void setColorArrayV2(JsonObject& opt, const char* key, uint32_t color);




// Migration routine
// in config.h (IMPORTANT: in the header, not in the .cpp!)
template<typename TCfg>
bool migrateConfigV2(TCfg& cfg)
{
    String ver = cfg["version"] | "1.0";
    bool changed = false;

    if (ver == "1.0") {
        if (cfg["system"]["updateCheckInterval"].isNull()) {
            cfg["system"]["updateCheckInterval"] = 3;
        }
        Serial.println(F("[MIGRATION] 1.0 -> 2.0"));
        ver = "2.0";
        changed = true;
    }
    if (ver == "2.1") {
        // Next step later
        //CONFIGV2.system.timezone = sys["timezone"] | CONFIGV2.system.timezone;
        if (cfg["system"]["timezone"].isNull()) {
            cfg["system"]["timezone"] = "CET-1CEST,M3.5.0,M10.5.0/3";
        }
        Serial.println(F("[MIGRATION] 2.1 -> 2.2"));
        ver = "2.2";
        changed = true;
    }

    if (changed) {
        cfg["version"] = ver;
        Serial.printf("Config Version  = '%s'\n", ver.c_str());
    }
    return changed;
}