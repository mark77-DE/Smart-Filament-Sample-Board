#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#include "led_config.h"

// OPTIONAL: Marker so gpio_hardware.cpp knows that
// Button/buzzer are present in CONFIG:
#define CONFIG_HAS_GPIO


// ============================================================================
// Configuration structures
// ============================================================================
/**
 * @brief System
 * Configuration
 */

struct systemConfig {
  String        version               = "error";      
  bool          darkmode              = false;                      ///< Dark mode enabled
  bool          debugMode             = true;                       ///< Debug mode enabled
  uint32_t      webLEDTimeout         = 3000;                       ///< Dashboard default (ms)
  String        hostname              = "filament-sample-board";    ///< Wi-Fi hostname
  bool          animationAfterBoot    = true;                       ///< Startup animation after boot enabled
  String        defaultLanguage       = "en";                       ///< Default language (e.g. "en" or "de")
  uint32_t      updateCheckInterval   = 240;                        ///< Seconds until reboot after long press
  String        timezone              = "UTC";                      // POSIX-TZ-String, example see TIMEZONE in platformio.ini
};



/**
 * @brief Global LED hardware configuration
 *
 * Applies to all addressable LEDs in the device.
 * Defaults come from platformio.ini.
 */
struct LedHardwareConfigV2 {
  LedType  type  = static_cast<LedType>(LED_TYPE);                ///< defaults to platformio.ini
  LedOrder order = static_cast<LedOrder>(LED_ORDER);              ///< defaults to platformio.ini
};


/**
 * @brief LED configuration (filament strip)
 */
struct LedConfigV2 {
  int      count        = 8;          ///< Number of LEDs
  int      pin          = -1;         ///< GPIO pin
  int      brightness   = 10;         ///< Brightness [0..255]
  int      timeout      = 2000;       ///< Timeout in ms
  uint32_t color        = 0x00FF00;   ///< Default color 0xRRGGBB
  uint32_t colorError   = 0xFF00FF;   ///< Error color 0xRRGGBB
  uint32_t colorPulse   = 0x0040A0;   ///< Idle pulse color 0xRRGGBB
};

/**
 * @brief NFC LED configuration
 */
struct NfcLedConfigV2 {
  int      count                  = 8;          ///< Number of LEDs
  int      pin                    = -1;         ///< GPIO pin
  int      brightness             = 10;         ///< Brightness [0..255]
  int      timeout                = 4000;       ///< Timeout in ms
  uint32_t colorSuccess           = 0x00FF00;   ///< Success color 0xRRGGBB
  uint32_t colorError             = 0xFF0000;   ///< Error color 0xRRGGBB
  uint32_t colorPulse             = 0x0000FF;   ///< Idle pulse color 0xRRGGBB
  bool     successBlinkEnabled    = true;       ///< True = success blinking enabled
  int      successBlinkCount      = 3;          ///< Number of blink cycles
  int      successBlinkMs         = 200;        ///< Blink interval in ms
};

/**
 * @brief Button configuration
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
 * @brief Buzzer configuration
 */
struct BuzzerConfigV2 {
  bool enabled       = true;  ///< Buzzer enabled?
  int  pin           = -1;    ///< Buzzer GPIO (-1 = off)
  bool activeHigh    = true;  ///< Active level HIGH?
  bool passive       = false; ///< false = active buzzer, true = passive (PWM)
  int  freqHz        = 4000;  ///< Frequency for tone()
  int  singleMs      = 80;    ///< Duration of single beep
  int  doubleOnMs    = 60;    ///< Duration of one double-beep pulse
  int  doubleGapMs   = 80;    ///< Pause between double-beep pulses
  int  errorOnMs     = 50;    ///< Duration of one error-sequence pulse
  int  errorGapMs    = 60;    ///< Pause between error-sequence pulses
  int  errorCount    = 3;     ///< Number of error-sequence repetitions
};

/**
 * @brief MQTT configuration
 */

struct MqttConfigV2 {
  bool enabled                = false;
  String server               = "192.168.168.150";
  uint16_t port               = 1883;
  String user                 = "";    
  String password             = "";
  String baseTopic            = "spotmyfilament";
  String clientId             = "filament-board";
  bool haDiscovery            = false   ;          ///< Home Assistant discovery active
  String haDiscoveryPrefix    = "homeassistant";  ///< Prefix for HA Discovery (e.g. "homeassistant")
};


/**
 * @brief Filaman configuration
 */

struct FilamanConfig 
{
  /* data */
  bool      enabled         = false;
  String    server          = "192.168.169.151";    //IP-address of filaman server
  uint32_t  port            = 8083;                 //port of filman api/server
  String    user            = "admin@exapmle.com";  //Filaman login user (usually email address)
  String    password        = "admin123";           //Filaman password
};





/**
 * @brief Main application configuration structure
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
  FilamanConfig  filamanConfig;     ///< Filaman configuration
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

/**
 * @brief reset config to default
 * @return true on success, otherwise false
 */
void resetConfigToDefaults();



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




/**
 * @brief Migrates a JSON config object from an older schema version to the current format.
 * @tparam TCfg JSON object or document type that supports bracket access.
 * @param cfg Configuration document to normalize in place.
 * @return true if the config was changed during migration, otherwise false.
 */
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
            cfg["system"]["timezone"] = TIMEZONE;
        }
        Serial.println(F("[MIGRATION] 2.1 -> 2.2"));
        ver = "2.2";
        changed = true;
    }

    if (ver == "2.2") {
        // Next step later
        //CONFIGV2.system.timezone = sys["timezone"] | CONFIGV2.system.timezone;
        if (cfg["filamanConfig"]["enabled"].isNull()) {
            cfg["filamanConfig"]["enabled"] = false;
        }
        if (cfg["filamanConfig"]["server"].isNull()) {
            cfg["filamanConfig"]["server"] = "";
        }
        if (cfg["filamanConfig"]["port"].isNull()) {
            cfg["filamanConfig"]["port"] = 8083;
        }
        if (cfg["filamanConfig"]["user"].isNull()) {
            cfg["filamanConfig"]["user"] = "";
        }
        if (cfg["filamanConfig"]["password"].isNull()) {
            cfg["filamanConfig"]["password"] = "";
        }
        Serial.println(F("[MIGRATION] 2.2 -> 2.3"));
        ver = "2.3";
        changed = true;
    }

    if (changed) {
        cfg["version"] = ver;
        Serial.printf("Config Version  = '%s'\n", ver.c_str());
    }
    return changed;
}