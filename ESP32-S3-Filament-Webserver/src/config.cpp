#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ledctrl_filament.h"
#include "ledctrl_nfc.h"
#include "gpio_hardware.h"   // für gpiohw_init()
#include "globals.h"

AppConfigV2 CONFIGV2;

// ============================================================================
// Laden / Anwenden der Konfiguration
// ============================================================================

bool loadConfigV2()
{
    // LittleFS mounten (mit Format-on-fail = true, wie bisher genutzt)
    Serial.println("Mounting LittleFS for V2...");
    if (!LittleFS.begin(true))
    {

        Serial.println(F("LittleFS mount failed V2!"));

        // unverändertes Verhalten: blockieren
        while (1)
        {
            delay(10);
        }
    }

    if (!LittleFS.exists("/config_v2.json")) {
        Serial.println(F("Config file not found!"));
        return false;
    }

    File f = LittleFS.open("/config_v2.json", "r");
    if (!f) {
        Serial.println(F("Failed to open config file!"));
        return false;
    }
    

    JsonDocument doc;
    auto err = deserializeJson(doc, f);
    f.close();
    if (err)
        return false;

    if (!doc.is<JsonObject>())
    return false;
    JsonObject cfg = doc.as<JsonObject>();

    JsonObject sys  = cfg["system"];
    JsonObject led  = cfg["led"];
    JsonObject nfc  = cfg["nfc"];
    JsonObject btn  = cfg["button"];
    JsonObject buz  = cfg["buzzer"];
    JsonObject mqtt = cfg["mqttConfig"];






    // --- Basis-Flags ---
    CONFIGV2.system.darkmode = sys["darkmode"] | false;
    
    CONFIGV2.system.debugMode = sys["debugMode"] | false;
    CONFIGV2.system.hostname = sys["hostname"] | "FiSaBo";

    // --- Filament-LED ---
    CONFIGV2.led.count = led["count"] | 8;
    CONFIGV2.led.pin = led["pin"] | 4;
    CONFIGV2.led.brightness = led["brightness"] | 50;
    CONFIGV2.led.timeout = led["timeout"] | 3000;
    // --- Dashboard (Virtuelle LED)---
    CONFIGV2.system.webLEDTimeout = sys["webLEDTimeout"] | (uint32_t)CONFIGV2.led.timeout; // Fallback auf ledTimeout

    if (led["ledColor"].is<JsonArray>())
    {
        JsonArray c = led["ledColor"];
        CONFIGV2.led.color = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
    }
    else
    {
        CONFIGV2.led.color = 0xFFFFFF;
    }

    if (led["ledColorError"].is<JsonArray>())
    {
        JsonArray e = led["ledColorError"];
        CONFIGV2.led.colorError = ((uint32_t)e[0] << 16) | ((uint32_t)e[1] << 8) | (uint32_t)e[2];
    }
    else
    {
        CONFIGV2.led.colorError = 0xFF0000;
    }

    if (led["ledColorPulse"].is<JsonArray>())
    {
        JsonArray p = led["ledColorPulse"];
        CONFIGV2.led.colorPulse = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
    }
    else
    {
        CONFIGV2.led.colorPulse = 0x0000FF;
    }

    // --- NFC-LED ---
    CONFIGV2.nfc.count                      = nfc["count"] | 8;
    CONFIGV2.nfc.pin                        = nfc["pin"] | 15;
    CONFIGV2.nfc.brightness                 = nfc["brightness"] | 100;
    CONFIGV2.nfc.timeout                    = nfc["timeout"] | 4000;

    if (nfc["colorSuccess"].is<JsonArray>())
    {
        JsonArray c = nfc["colorSuccess"];
        CONFIGV2.nfc.colorSuccess = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
    }
    else
    {
        CONFIGV2.nfc.colorSuccess = 0xFFFF00;
    }

    if (nfc["colorError"].is<JsonArray>())
    {
        JsonArray c = nfc["colorError"];
        CONFIGV2.nfc.colorError = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
    }
    else
    {
        CONFIGV2.nfc.colorError = 0x00FFFF;
    }

    if (nfc["colorPulse"].is<JsonArray>())
    {
        JsonArray c = nfc["colorPulse"];
        CONFIGV2.nfc.colorPulse = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
    }
    else
    {
        CONFIGV2.nfc.colorPulse = 0xFF00FF;
    }

    CONFIGV2.nfc.successBlinkEnabled        = nfc["successBlinkEnabled"] | true;
    CONFIGV2.nfc.successBlinkCount          = nfc["successBlinkCount"] | 3;
    CONFIGV2.nfc.successBlinkMs             = nfc["successBlinkMs"] | 150;

    // --- Button ---
    CONFIGV2.button.pin                     = btn["pin"] | CONFIGV2.button.pin;
    CONFIGV2.button.pullup                  = btn["pullup"] | CONFIGV2.button.pullup;
    CONFIGV2.button.debounceMs              = btn["debounceMs"] | CONFIGV2.button.debounceMs;
    CONFIGV2.button.longMs                  = btn["longMs"] | CONFIGV2.button.longMs;
    CONFIGV2.button.doubleGapMs             = btn["doubleMs"] | CONFIGV2.button.doubleGapMs;
    CONFIGV2.button.holdRepeatMs            = btn["holdMs"] | CONFIGV2.button.holdRepeatMs;

    // --- Buzzer ---
    CONFIGV2.buzzer.pin                     = buz["pin"] | CONFIGV2.buzzer.pin;
    CONFIGV2.buzzer.activeHigh              = buz["activeHigh"] | CONFIGV2.buzzer.activeHigh;
    CONFIGV2.buzzer.freqHz                  = buz["freq"] | CONFIGV2.buzzer.freqHz;
    CONFIGV2.buzzer.singleMs                = buz["singleMs"] | CONFIGV2.buzzer.singleMs;
    CONFIGV2.buzzer.doubleOnMs              = buz["doubleOnMs"] | CONFIGV2.buzzer.doubleOnMs;
    CONFIGV2.buzzer.doubleGapMs             = buz["doubleGapMs"] | CONFIGV2.buzzer.doubleGapMs;
    CONFIGV2.buzzer.errorOnMs               = buz["errorOnMs"] | CONFIGV2.buzzer.errorOnMs;
    CONFIGV2.buzzer.errorGapMs              = buz["errorGapMs"] | CONFIGV2.buzzer.errorGapMs;
    CONFIGV2.buzzer.errorCount              = buz["errorCount"] | CONFIGV2.buzzer.errorCount;

    // --- MQTT ---
    CONFIGV2.mqttConfig.enabled             = mqtt["enabled"] | false;
    CONFIGV2.mqttConfig.server              = mqtt["server"] | "192.168.178.10";
    CONFIGV2.mqttConfig.port                = mqtt["port"] | 1883;
    CONFIGV2.mqttConfig.user                = mqtt["user"] | "";
    CONFIGV2.mqttConfig.password            = mqtt["password"] | "";
    CONFIGV2.mqttConfig.clientId            = mqtt["clientId"] | "ESP32-SMFS";
    CONFIGV2.mqttConfig.baseTopic           = mqtt["baseTopic"] | "spotmyfilament";

    // Filament-DB laden & Konfiguration anwenden
    loadFilaments();
    return true;
}

//----------------------------------------------------------------------------
// Anwenden der geladenen Konfiguration (vor allem Hardware-bezogen)
//----------------------------------------------------------------------------

void applyConfigV2() {

  // Filament-LEDs
  LEDCTRL_FILAMENT::init(
    CONFIGV2.led.count,
    CONFIGV2.led.pin,
    CONFIGV2.led.timeout,
    CONFIGV2.led.brightness,
    CONFIGV2.led.color,
    CONFIGV2.led.colorError,
    CONFIGV2.led.colorPulse
  );

  // NFC-LEDs
  LEDCTRL_NFC::init(
    CONFIGV2.nfc.count,
    CONFIGV2.nfc.pin,
    CONFIGV2.nfc.timeout,
    CONFIGV2.nfc.brightness,
    CONFIGV2.nfc.colorSuccess,
    CONFIGV2.nfc.colorError,
    CONFIGV2.nfc.colorPulse,
    CONFIGV2.nfc.successBlinkEnabled,
    CONFIGV2.nfc.successBlinkCount,
    CONFIGV2.nfc.successBlinkMs
  );

  // GPIO-Hardware (Button/Buzzer)
  gpiohw_init();  // liest CONFIG.button / CONFIG.buzzer, richtet Pins & ISR/Timer ein

  if (CONFIGV2.system.debugMode) {
    Serial.println(F("--------------------"));
    Serial.println(F("Config V2 applied:"));

    Serial.print(F(" HOSTNAME = "));       Serial.println(CONFIGV2.system.hostname);

    Serial.print(F(" LED_COUNT = "));         Serial.println(CONFIGV2.led.count);
    Serial.print(F(" LED_PIN = "));           Serial.println(CONFIGV2.led.pin);
    Serial.print(F(" LED_TIMEOUT = "));       Serial.println(CONFIGV2.led.timeout);
    Serial.print(F(" LED_BRIGHTNESS = "));    Serial.println(CONFIGV2.led.brightness);
    Serial.print(F(" LED_COLOR = 0x"));       Serial.println(CONFIGV2.led.color, HEX);
    Serial.print(F(" LED_COLOR_ERROR = 0x")); Serial.println(CONFIGV2.led.colorError, HEX);
    Serial.print(F(" LED_COLOR_PULSE = 0x")); Serial.println(CONFIGV2.led.colorPulse, HEX);

    Serial.print(F(" WEB_LED_TIMEOUT = ")); Serial.println(CONFIGV2.webLEDTimeout);


    Serial.print(F(" NFC_LED_COUNT = "));     Serial.println(CONFIGV2.nfc.count);
    Serial.print(F(" NFC_LED_PIN = "));       Serial.println(CONFIGV2.nfc.pin);
    Serial.print(F(" NFC_LED_TIMEOUT = "));   Serial.println(CONFIGV2.nfc.timeout);
    Serial.print(F(" NFC_LED_BRIGHTNESS = "));Serial.println(CONFIGV2.nfc.brightness);
    Serial.print(F(" NFC_LED_COLOR_SUCCESS = 0x")); Serial.println(CONFIGV2.nfc.colorSuccess, HEX);
    Serial.print(F(" NFC_LED_COLOR_ERROR = 0x"));   Serial.println(CONFIGV2.nfc.colorError, HEX);
    Serial.print(F(" NFC_LED_COLOR_PULSE = 0x"));   Serial.println(CONFIGV2.nfc.colorPulse, HEX);
    Serial.print(F(" DEBUG_MODE = "));        Serial.println(CONFIGV2.system.debugMode ? F("true") : F("false"));

    Serial.print(F(" BUTTON pin="));          Serial.print(CONFIGV2.button.pin);
    Serial.print(F(" pullup="));              Serial.print(CONFIGV2.button.pullup);
    Serial.print(F(" debounce="));            Serial.print(CONFIGV2.button.debounceMs);
    Serial.print(F("ms long="));              Serial.print(CONFIGV2.button.longMs);
    Serial.print(F("ms double="));            Serial.print(CONFIGV2.button.doubleGapMs);
    Serial.print(F("ms holdRep="));           Serial.print(CONFIGV2.button.holdRepeatMs);
    Serial.println(F("ms"));

    Serial.print(F(" BUZZER pin="));          Serial.print(CONFIGV2.buzzer.pin);
    Serial.print(F(" activeHigh="));          Serial.print(CONFIGV2.buzzer.activeHigh);
    Serial.print(F(" freq="));                Serial.print(CONFIGV2.buzzer.freqHz);
    Serial.print(F("Hz single="));            Serial.print(CONFIGV2.buzzer.singleMs);
    Serial.print(F("ms dblOn="));             Serial.print(CONFIGV2.buzzer.doubleOnMs);
    Serial.print(F("ms dblGap="));            Serial.print(CONFIGV2.buzzer.doubleGapMs);
    Serial.print(F("ms errOn="));             Serial.print(CONFIGV2.buzzer.errorOnMs);
    Serial.print(F("ms errGap="));            Serial.print(CONFIGV2.buzzer.errorGapMs);
    Serial.print(F("ms errCount="));          Serial.println(CONFIG.buzzer.errorCount);

    Serial.println(F("--------------------"));
  }
}




bool updateConfigFromJsonV2(JsonDocument& doc) {
    if (!doc.is<JsonObject>()) return false;

    JsonObject cfg = doc.as<JsonObject>();

    // --- System ---
    if (cfg["system"].is<JsonObject>()) {
        JsonObject sys = cfg["system"];
        CONFIGV2.system.darkmode   = sys["darkmode"]   | CONFIGV2.system.darkmode;
        CONFIGV2.system.debugMode  = sys["debugMode"]  | CONFIGV2.system.debugMode;
        CONFIGV2.system.hostname   = sys["hostname"]   | CONFIGV2.system.hostname;
        CONFIGV2.system.webLEDTimeout = sys["webLEDTimeout"] | CONFIGV2.system.webLEDTimeout;
        Serial.println(F("System configuration updated:"));
        Serial.print(F("Hostname set to: ")); Serial.println(CONFIGV2.system.hostname);
        Serial.print(F("Web LED Timeout set to: ")); Serial.println(CONFIGV2.system.webLEDTimeout);
        Serial.print(F("Darkmode set to: ")); Serial.println(CONFIGV2.system.darkmode ? F("true") : F("false"));
        Serial.print(F("Debug Mode set to: ")); Serial.println(CONFIGV2.system.debugMode ? F("true") : F("false"));
    }

    // --- LED ---
    if (cfg["led"].is<JsonObject>()) {
        JsonObject led = cfg["led"];
        CONFIGV2.led.count      = led["count"]      | CONFIGV2.led.count;
        CONFIGV2.led.pin        = led["pin"]        | CONFIGV2.led.pin;
        CONFIGV2.led.brightness = led["brightness"] | CONFIGV2.led.brightness;
        CONFIGV2.led.timeout    = led["timeout"]    | CONFIGV2.led.timeout;

        if (led["color"].is<JsonArray>()) {
            JsonArray arr = led["color"];
            CONFIGV2.led.color = ((uint32_t)arr[0] << 16) |
                                 ((uint32_t)arr[1] << 8)  |
                                  (uint32_t)arr[2];
        }
        if (led["colorError"].is<JsonArray>()) {
            JsonArray arr = led["colorError"];
            CONFIGV2.led.colorError = ((uint32_t)arr[0] << 16) |
                                      ((uint32_t)arr[1] << 8)  |
                                       (uint32_t)arr[2];
        }
        if (led["colorPulse"].is<JsonArray>()) {
            JsonArray arr = led["colorPulse"];
            CONFIGV2.led.colorPulse = ((uint32_t)arr[0] << 16) |
                                      ((uint32_t)arr[1] << 8)  |
                                       (uint32_t)arr[2];
        }

        Serial.println(F("LED configuration updated:"));
        Serial.print(F("LED count set to: "));       Serial.println(CONFIGV2.led.count);
        Serial.print(F("LED pin set to: "));         Serial.println(CONFIGV2.led.pin);
        Serial.print(F("LED brightness set to: "));  Serial.println(CONFIGV2.led.brightness);
        Serial.print(F("LED timeout set to: "));     Serial.println(CONFIGV2.led.timeout);
        Serial.print(F("LED color set to: 0x"));   Serial.println(CONFIGV2.led.color, HEX);
        Serial.print(F("LED error color set to: 0x"));   Serial.println(CONFIGV2.led.colorError, HEX);
        Serial.print(F("LED pulse color set to: 0x"));   Serial.println(CONFIGV2.led.colorPulse, HEX);
    }

    // --- NFC ---
    if (cfg["nfc"].is<JsonObject>()) {
        JsonObject nfc = cfg["nfc"];
        CONFIGV2.nfc.count  = nfc["count"]  | CONFIGV2.nfc.count;
        CONFIGV2.nfc.pin    = nfc["pin"]    | CONFIGV2.nfc.pin;
        CONFIGV2.nfc.brightness = nfc["brightness"] | CONFIGV2.nfc.brightness;
        CONFIGV2.nfc.timeout    = nfc["timeout"]    | CONFIGV2.nfc.timeout;

        if (nfc["colorSuccess"].is<JsonArray>()) {
            JsonArray arr = nfc["colorSuccess"];
            CONFIGV2.nfc.colorSuccess = ((uint32_t)arr[0] << 16) |
                                        ((uint32_t)arr[1] << 8)  |
                                         (uint32_t)arr[2];
        }
        if (nfc["colorError"].is<JsonArray>()) {
            JsonArray arr = nfc["colorError"];
            CONFIGV2.nfc.colorError = ((uint32_t)arr[0] << 16) |
                                      ((uint32_t)arr[1] << 8)  |
                                       (uint32_t)arr[2];
        }
        if (nfc["colorPulse"].is<JsonArray>()) {
            JsonArray arr = nfc["colorPulse"];
            CONFIGV2.nfc.colorPulse = ((uint32_t)arr[0] << 16) |
                                      ((uint32_t)arr[1] << 8)  |
                                       (uint32_t)arr[2];
        }

        CONFIGV2.nfc.successBlinkEnabled = nfc["successBlinkEnabled"] | CONFIGV2.nfc.successBlinkEnabled;
        CONFIGV2.nfc.successBlinkCount   = nfc["successBlinkCount"]   | CONFIGV2.nfc.successBlinkCount;
        CONFIGV2.nfc.successBlinkMs      = nfc["successBlinkMs"]      | CONFIGV2.nfc.successBlinkMs;

        Serial.println(F("NFC configuration updated:"));
        Serial.print(F("NFC count set to: "));       Serial.println(CONFIGV2.nfc.count);
        Serial.print(F("NFC pin set to: "));         Serial.println(CONFIGV2.nfc.pin);
        Serial.print(F("NFC brightness set to: "));  Serial.println(CONFIGV2.nfc.brightness);
        Serial.print(F("NFC timeout set to: "));     Serial.println(CONFIGV2.nfc.timeout);
        Serial.print(F("NFC success color set to: 0x"));   Serial.println(CONFIGV2.nfc.colorSuccess, HEX);
        Serial.print(F("NFC error color set to: 0x"));     Serial.println(CONFIGV2.nfc.colorError, HEX);
        Serial.print(F("NFC pulse color set to: 0x"));     Serial.println(CONFIGV2.nfc.colorPulse, HEX);
        Serial.print(F("NFC success blink enabled set to: ")); Serial.println(CONFIGV2.nfc.successBlinkEnabled ? F("true") : F("false"));
        Serial.print(F("NFC success blink count set to: "));   Serial.println(CONFIGV2.nfc.successBlinkCount);
    }

    // --- Button ---
    if (cfg["button"].is<JsonObject>()) {
        JsonObject btn = cfg["button"];
        CONFIGV2.button.pin          = btn["pin"]         | CONFIGV2.button.pin;
        CONFIGV2.button.pullup       = btn["pullup"]      | CONFIGV2.button.pullup;
        CONFIGV2.button.debounceMs   = btn["debounceMs"]  | CONFIGV2.button.debounceMs;
        CONFIGV2.button.longMs       = btn["longMs"]      | CONFIGV2.button.longMs;
        CONFIGV2.button.doubleGapMs  = btn["doubleMs"]    | CONFIGV2.button.doubleGapMs;
        CONFIGV2.button.holdRepeatMs = btn["holdMs"]      | CONFIGV2.button.holdRepeatMs;

        Serial.println(F("Button configuration updated:"));
        Serial.print(F("Button pin set to: "));        Serial.println(CONFIGV2.button.pin);
        Serial.print(F("Button pullup set to: "));     Serial.println(CONFIGV2.button.pullup ? F("true") : F("false"));
        Serial.print(F("Button debounceMs set to: ")); Serial.println(CONFIGV2.button.debounceMs);
        Serial.print(F("Button longMs set to: "));     Serial.println(CONFIGV2.button.longMs);
        Serial.print(F("Button doubleGapMs set to: "));Serial.println(CONFIGV2.button.doubleGapMs);
        Serial.print(F("Button holdRepeatMs set to: "));Serial.println(CONFIGV2.button.holdRepeatMs);
    }

    // --- Buzzer ---
    if (cfg["buzzer"].is<JsonObject>()) {
        JsonObject buz = cfg["buzzer"];
        CONFIGV2.buzzer.pin          = buz["pin"]         | CONFIGV2.buzzer.pin;
        CONFIGV2.buzzer.activeHigh   = buz["activeHigh"]  | CONFIGV2.buzzer.activeHigh;
        CONFIGV2.buzzer.freqHz       = buz["freq"]        | CONFIGV2.buzzer.freqHz;
        CONFIGV2.buzzer.singleMs     = buz["singleMs"]    | CONFIGV2.buzzer.singleMs;
        CONFIGV2.buzzer.doubleOnMs   = buz["doubleOnMs"]  | CONFIGV2.buzzer.doubleOnMs;
        CONFIGV2.buzzer.doubleGapMs  = buz["doubleGapMs"] | CONFIGV2.buzzer.doubleGapMs;
        CONFIGV2.buzzer.errorOnMs    = buz["errorOnMs"]   | CONFIGV2.buzzer.errorOnMs;
        CONFIGV2.buzzer.errorGapMs   = buz["errorGapMs"]  | CONFIGV2.buzzer.errorGapMs;
        CONFIGV2.buzzer.errorCount   = buz["errorCount"]  | CONFIGV2.buzzer.errorCount;

        Serial.println(F("Buzzer configuration updated:"));
        Serial.print(F("Buzzer pin set to: "));         Serial.println(CONFIGV2.buzzer.pin);
        Serial.print(F("Buzzer activeHigh set to: "));  Serial.println(CONFIGV2.buzzer.activeHigh ? F("true") : F("false"));
        Serial.print(F("Buzzer freqHz set to: "));      Serial.println(CONFIGV2.buzzer.freqHz);
        Serial.print(F("Buzzer singleMs set to: "));    Serial.println(CONFIGV2.buzzer.singleMs);
        Serial.print(F("Buzzer doubleOnMs set to: "));  Serial.println(CONFIGV2.buzzer.doubleOnMs);
        Serial.print(F("Buzzer doubleGapMs set to: ")); Serial.println(CONFIGV2.buzzer.doubleGapMs);
        Serial.print(F("Buzzer errorOnMs set to: "));    Serial.println(CONFIGV2.buzzer.errorOnMs);
        Serial.print(F("Buzzer errorGapMs set to: "));   Serial.println(CONFIGV2.buzzer.errorGapMs);
        Serial.print(F("Buzzer errorCount set to: "));   Serial.println(CONFIGV2.buzzer.errorCount);
    }

    // --- MQTT ---
    if (cfg["mqttConfig"].is<JsonObject>()) {
        JsonObject mqtt = cfg["mqttConfig"];
        CONFIGV2.mqttConfig.enabled   = mqtt["enabled"]   | CONFIGV2.mqttConfig.enabled;
        CONFIGV2.mqttConfig.server    = mqtt["server"]    | CONFIGV2.mqttConfig.server;
        CONFIGV2.mqttConfig.port      = mqtt["port"]      | CONFIGV2.mqttConfig.port;
        CONFIGV2.mqttConfig.user      = mqtt["user"]      | CONFIGV2.mqttConfig.user;
        CONFIGV2.mqttConfig.password  = mqtt["password"]  | CONFIGV2.mqttConfig.password;
        CONFIGV2.mqttConfig.baseTopic = mqtt["baseTopic"] | CONFIGV2.mqttConfig.baseTopic;
        CONFIGV2.mqttConfig.clientId  = mqtt["clientId"]  | CONFIGV2.mqttConfig.clientId;

        Serial.println(F("MQTT configuration updated:"));
        Serial.print(F("MQTT enabled set to: "));      Serial.println(CONFIGV2.mqttConfig.enabled ? F("true") : F("false"));
        Serial.print(F("MQTT server set to: "));       Serial.println(CONFIGV2.mqttConfig.server);
        Serial.print(F("MQTT port set to: "));         Serial.println(CONFIGV2.mqttConfig.port);
        Serial.print(F("MQTT user set to: "));         Serial.println(CONFIGV2.mqttConfig.user);
        Serial.print(F("MQTT baseTopic set to: "));    Serial.println(CONFIGV2.mqttConfig.baseTopic);
        Serial.print(F("MQTT clientId set to: "));     Serial.println(CONFIGV2.mqttConfig.clientId);
    }

    saveConfigV2();
    g_applyConfigPending = true;
    return true;
}


bool saveConfigV2() {

    if(CONFIGV2.system.debugMode) {
        Serial.println(F("Saving configuration V2..."));
    }
    if (!LittleFS.begin()){
        Serial.println(F("LittleFS mount failed during saveConfigV2!"));
        return false;
    } 

    JsonDocument doc;

    // =========================
    // Version
    // =========================
    doc["version"] = 2;

    // =========================
    // System
    // =========================
    JsonObject system = doc["system"].to<JsonObject>();
    system["darkmode"]       = CONFIGV2.system.darkmode;
    system["debugMode"]      = CONFIGV2.system.debugMode;
    system["webLEDTimeout"]  = CONFIGV2.system.webLEDTimeout;
    system["hostname"]       = CONFIGV2.system.hostname;

    // =========================
    // LED
    // =========================
    JsonObject led = doc["led"].to<JsonObject>();
    led["count"]      = CONFIGV2.led.count;
    led["pin"]        = CONFIGV2.led.pin;
    led["brightness"] = CONFIGV2.led.brightness;
    led["timeout"]    = CONFIGV2.led.timeout;
    setColorArray(led, "color",      CONFIGV2.led.color);
    setColorArray(led, "colorError", CONFIGV2.led.colorError);
    setColorArray(led, "colorPulse", CONFIGV2.led.colorPulse);

    // =========================
    // NFC
    // =========================
    JsonObject nfc = doc["nfc"].to<JsonObject>();
    nfc["count"]      = CONFIGV2.nfc.count;
    nfc["pin"]        = CONFIGV2.nfc.pin;
    nfc["brightness"] = CONFIGV2.nfc.brightness;
    nfc["timeout"]    = CONFIGV2.nfc.timeout;
    setColorArray(nfc, "colorSuccess", CONFIGV2.nfc.colorSuccess);
    setColorArray(nfc, "colorError",   CONFIGV2.nfc.colorError);
    setColorArray(nfc, "colorPulse",   CONFIGV2.nfc.colorPulse);
    nfc["successBlinkEnabled"] = CONFIGV2.nfc.successBlinkEnabled;
    nfc["successBlinkCount"]   = CONFIGV2.nfc.successBlinkCount;
    nfc["successBlinkMs"]      = CONFIGV2.nfc.successBlinkMs;

    // =========================
    // Button
    // =========================
    JsonObject button = doc["button"].to<JsonObject>();
    button["pin"]        = CONFIGV2.button.pin;
    button["pullup"]     = CONFIGV2.button.pullup;
    button["debounceMs"] = CONFIGV2.button.debounceMs;
    button["longMs"]     = CONFIGV2.button.longMs;
    button["doubleMs"]   = CONFIGV2.button.doubleGapMs;
    button["holdMs"]     = CONFIGV2.button.holdRepeatMs;

    // =========================
    // Buzzer
    // =========================
    JsonObject buzzer = doc["buzzer"].to<JsonObject>();
    buzzer["pin"]         = CONFIGV2.buzzer.pin;
    buzzer["activeHigh"]  = CONFIGV2.buzzer.activeHigh;
    buzzer["freq"]        = CONFIGV2.buzzer.freqHz;
    buzzer["singleMs"]    = CONFIGV2.buzzer.singleMs;
    buzzer["doubleOnMs"]  = CONFIGV2.buzzer.doubleOnMs;
    buzzer["doubleGapMs"] = CONFIGV2.buzzer.doubleGapMs;
    buzzer["errorOnMs"]   = CONFIGV2.buzzer.errorOnMs;
    buzzer["errorGapMs"]  = CONFIGV2.buzzer.errorGapMs;
    buzzer["errorCount"]  = CONFIGV2.buzzer.errorCount;

    // =========================
    // MQTT
    // =========================
    JsonObject mqtt = doc["mqttConfig"].to<JsonObject>();
    mqtt["enabled"]   = CONFIGV2.mqttConfig.enabled;
    mqtt["server"]    = CONFIGV2.mqttConfig.server;
    mqtt["port"]      = CONFIGV2.mqttConfig.port;
    mqtt["user"]      = CONFIGV2.mqttConfig.user;
    mqtt["password"]  = CONFIGV2.mqttConfig.password;
    mqtt["baseTopic"] = CONFIGV2.mqttConfig.baseTopic;
    mqtt["clientId"]  = CONFIGV2.mqttConfig.clientId;

    // =========================
    // Schreiben
    // =========================
    File f = LittleFS.open("/config_v2.json", "w");
    if (!f) {
        Serial.println(F("Failed to open config_v2.json file for writing!"));
        return false;
    }

    size_t written = serializeJsonPretty(doc, f);
    f.close();

    if(CONFIGV2.system.debugMode) {
        Serial.print(F("Configuration V2 saved, bytes written: "));
        Serial.println(written);
    }   

    return written > 0;
}



bool importConfigJsonV2(JsonObject src) {
    if (!LittleFS.begin(true)) return false;

    // -------------------------
    // Versionsprüfung
    // -------------------------
    uint8_t version = src["version"] | 1;
    if (version != 2) {
        // optional: Migration hier einbauen
        return false;
    }

    // =========================
    // System
    // =========================
    if (src["system"].is<JsonObject>()) {
        JsonObject system = src["system"];
        CONFIGV2.system.darkmode      = system["darkmode"]      | CONFIGV2.system.darkmode;
        CONFIGV2.system.debugMode     = system["debugMode"]     | CONFIGV2.system.debugMode;
        CONFIGV2.system.webLEDTimeout = system["webLEDTimeout"] | CONFIGV2.system.webLEDTimeout;
        CONFIGV2.system.hostname      = system["hostname"]      | CONFIGV2.system.hostname;
        if(CONFIGV2.system.debugMode) {
            Serial.println(F("System configuration imported:"));
            Serial.print(F("  Hostname: ")); Serial.println(CONFIGV2.system.hostname);
            Serial.print(F("  Web LED Timeout: ")); Serial.println(CONFIGV2.system.webLEDTimeout);
            Serial.print(F("  Darkmode: ")); Serial.println(CONFIGV2.system.darkmode ? F("true") : F("false"));
            Serial.print(F("  Debug Mode: ")); Serial.println(CONFIGV2.system.debugMode ? F("true") : F("false"));
        }
    }

    // =========================
    // LED
    // =========================
    if (src["led"].is<JsonObject>()) {
        JsonObject led = src["led"];
        CONFIGV2.led.count      = led["count"]      | CONFIGV2.led.count;
        CONFIGV2.led.pin        = led["pin"]        | CONFIGV2.led.pin;
        CONFIGV2.led.brightness = led["brightness"] | CONFIGV2.led.brightness;
        CONFIGV2.led.timeout    = led["timeout"]    | CONFIGV2.led.timeout;

        if (led["color"].is<JsonArrayConst>()) {
            JsonArrayConst arr = led["color"].as<JsonArrayConst>();
            if (arr.size() >= 3) {
                CONFIGV2.led.color = colorFromArrayV2(arr);
            }
        }

        if (led["colorError"].is<JsonArrayConst>()) {
            JsonArrayConst arr = led["colorError"].as<JsonArrayConst>();
            if (arr.size() >= 3) {
                CONFIGV2.led.colorError = colorFromArrayV2(arr);
            }
        }

        if (led["colorPulse"].is<JsonArrayConst>()) {
            JsonArrayConst arr = led["colorPulse"].as<JsonArrayConst>();
            if (arr.size() >= 3) {
                CONFIGV2.led.colorPulse = colorFromArrayV2(arr);
            }
        }

        if(CONFIGV2.system.debugMode) {
            Serial.println(F("LED configuration imported:"));
            Serial.print(F("  LED count: "));       Serial.println(CONFIGV2.led.count);
            Serial.print(F("  LED pin: "));         Serial.println(CONFIGV2.led.pin);
            Serial.print(F("  LED brightness: "));  Serial.println(CONFIGV2.led.brightness);
            Serial.print(F("  LED timeout: "));     Serial.println(CONFIGV2.led.timeout);
            Serial.print(F("  LED color: 0x"));     Serial.println(CONFIGV2.led.color, HEX);
            Serial.print(F("  LED error color: 0x"));   Serial.println(CONFIGV2.led.colorError, HEX);
            Serial.print(F("  LED pulse color: 0x"));   Serial.println(CONFIGV2.led.colorPulse, HEX);
        }   
    }

    // =========================
    // NFC
    // =========================
    if (src["nfc"].is<JsonObject>()) {
        JsonObject nfc = src["nfc"];
        CONFIGV2.nfc.count      = nfc["count"]      | CONFIGV2.nfc.count;
        CONFIGV2.nfc.pin        = nfc["pin"]        | CONFIGV2.nfc.pin;
        CONFIGV2.nfc.brightness = nfc["brightness"] | CONFIGV2.nfc.brightness;
        CONFIGV2.nfc.timeout    = nfc["timeout"]    | CONFIGV2.nfc.timeout;

        if (nfc["colorSuccess"].is<JsonArrayConst>()) {
            JsonArrayConst arr = nfc["colorSuccess"].as<JsonArrayConst>();
            if (arr.size() >= 3) {
                CONFIGV2.nfc.colorSuccess = colorFromArrayV2(arr);
            }
        }

        if (nfc["colorError"].is<JsonArrayConst>()) {
            JsonArrayConst arr = nfc["colorError"].as<JsonArrayConst>();
            if (arr.size() >= 3) {
                CONFIGV2.nfc.colorError = colorFromArrayV2(arr);
            }
        }

        if (nfc["colorPulse"].is<JsonArrayConst>()) {
            JsonArrayConst arr = nfc["colorPulse"].as<JsonArrayConst>();
            if (arr.size() >= 3) {
                CONFIGV2.nfc.colorPulse = colorFromArrayV2(arr);
            }
        }

        CONFIGV2.nfc.successBlinkEnabled = nfc["successBlinkEnabled"] | CONFIGV2.nfc.successBlinkEnabled;
        CONFIGV2.nfc.successBlinkCount   = nfc["successBlinkCount"]   | CONFIGV2.nfc.successBlinkCount;
        CONFIGV2.nfc.successBlinkMs      = nfc["successBlinkMs"]      | CONFIGV2.nfc.successBlinkMs;

        if(CONFIGV2.system.debugMode) {
            Serial.println(F("NFC configuration imported:"));
            Serial.print(F("  NFC count: "));       Serial.println(CONFIGV2.nfc.count);
            Serial.print(F("  NFC pin: "));         Serial.println(CONFIGV2.nfc.pin);
            Serial.print(F("  NFC brightness: "));  Serial.println(CONFIGV2.nfc.brightness);
            Serial.print(F("  NFC timeout: "));     Serial.println(CONFIGV2.nfc.timeout);
            Serial.print(F("  NFC success color: 0x"));   Serial.println(CONFIGV2.nfc.colorSuccess, HEX);
            Serial.print(F("  NFC error color: 0x"));     Serial.println(CONFIGV2.nfc.colorError, HEX);
            Serial.print(F("  NFC pulse color: 0x"));     Serial.println(CONFIGV2.nfc.colorPulse, HEX);
            Serial.print(F("  NFC success blink enabled: ")); Serial.println(CONFIGV2.nfc.successBlinkEnabled ? F("true") : F("false"));
            Serial.print(F("  NFC success blink count: "));   Serial.println(CONFIGV2.nfc.successBlinkCount);
        }
    }

    // =========================
    // Button
    // =========================
    if (src["button"].is<JsonObject>()) {
        JsonObject button = src["button"];
        CONFIGV2.button.pin             = button["pin"]        | CONFIGV2.button.pin;
        CONFIGV2.button.pullup          = button["pullup"]     | CONFIGV2.button.pullup;
        CONFIGV2.button.debounceMs      = button["debounceMs"] | CONFIGV2.button.debounceMs;
        CONFIGV2.button.longMs          = button["longMs"]     | CONFIGV2.button.longMs;
        CONFIGV2.button.doubleGapMs     = button["doubleMs"]   | CONFIGV2.button.doubleGapMs;
        CONFIGV2.button.holdRepeatMs    = button["holdMs"]     | CONFIGV2.button.holdRepeatMs;

        if(CONFIGV2.system.debugMode) {
            Serial.println(F("Button configuration imported:"));
            Serial.print(F("  Button pin: "));        Serial.println(CONFIGV2.button.pin);
            Serial.print(F("  Button pullup: "));     Serial.println(CONFIGV2.button.pullup ? F("true") : F("false"));
            Serial.print(F("  Button debounceMs: ")); Serial.println(CONFIGV2.button.debounceMs);
            Serial.print(F("  Button longMs: "));     Serial.println(CONFIGV2.button.longMs);
            Serial.print(F("  Button doubleGapMs: "));Serial.println(CONFIGV2.button.doubleGapMs);
            Serial.print(F("  Button holdRepeatMs: "));Serial.println(CONFIGV2.button.holdRepeatMs);
        }
    }

    // =========================
    // Buzzer
    // =========================
    if (src["buzzer"].is<JsonObject>()) {
        JsonObject buzzer = src["buzzer"];
        CONFIGV2.buzzer.pin         = buzzer["pin"]        | CONFIGV2.buzzer.pin;
        CONFIGV2.buzzer.activeHigh  = buzzer["activeHigh"] | CONFIGV2.buzzer.activeHigh;
        CONFIGV2.buzzer.freqHz      = buzzer["freq"]       | CONFIGV2.buzzer.freqHz;
        CONFIGV2.buzzer.singleMs    = buzzer["singleMs"]   | CONFIGV2.buzzer.singleMs;
        CONFIGV2.buzzer.doubleOnMs  = buzzer["doubleOnMs"] | CONFIGV2.buzzer.doubleOnMs;
        CONFIGV2.buzzer.doubleGapMs = buzzer["doubleGapMs"]| CONFIGV2.buzzer.doubleGapMs;
        CONFIGV2.buzzer.errorOnMs   = buzzer["errorOnMs"]  | CONFIGV2.buzzer.errorOnMs;
        CONFIGV2.buzzer.errorGapMs  = buzzer["errorGapMs"] | CONFIGV2.buzzer.errorGapMs;
        CONFIGV2.buzzer.errorCount  = buzzer["errorCount"]| CONFIGV2.buzzer.errorCount;

        if(CONFIGV2.system.debugMode) {
            Serial.println(F("Buzzer configuration imported:"));
            Serial.print(F("  Buzzer pin: "));         Serial.println(CONFIGV2.buzzer.pin);
            Serial.print(F("  Buzzer activeHigh: "));  Serial.println(CONFIGV2.buzzer.activeHigh ? F("true") : F("false"));
            Serial.print(F("  Buzzer freqHz: "));      Serial.println(CONFIGV2.buzzer.freqHz);
            Serial.print(F("  Buzzer singleMs: "));    Serial.println(CONFIGV2.buzzer.singleMs);
            Serial.print(F("  Buzzer doubleOnMs: "));  Serial.println(CONFIGV2.buzzer.doubleOnMs);
            Serial.print(F("  Buzzer doubleGapMs: ")); Serial.println(CONFIGV2.buzzer.doubleGapMs);
            Serial.print(F("  Buzzer errorOnMs: "));    Serial.println(CONFIGV2.buzzer.errorOnMs);
            Serial.print(F("  Buzzer errorGapMs: "));   Serial.println(CONFIGV2.buzzer.errorGapMs);
            Serial.print(F("  Buzzer errorCount: "));   Serial.println(CONFIGV2.buzzer.errorCount);
        }
    }

    // =========================
    // MQTT
    // =========================
    if (src["mqttConfig"].is<JsonObject>()) {
        JsonObject mqtt = src["mqttConfig"];
        CONFIGV2.mqttConfig.enabled   = mqtt["enabled"]   | CONFIGV2.mqttConfig.enabled;
        CONFIGV2.mqttConfig.server    = mqtt["server"]    | CONFIGV2.mqttConfig.server;
        CONFIGV2.mqttConfig.port      = mqtt["port"]      | CONFIGV2.mqttConfig.port;
        CONFIGV2.mqttConfig.user      = mqtt["user"]      | CONFIGV2.mqttConfig.user;
        CONFIGV2.mqttConfig.password  = mqtt["password"]  | CONFIGV2.mqttConfig.password;
        CONFIGV2.mqttConfig.baseTopic = mqtt["baseTopic"] | CONFIGV2.mqttConfig.baseTopic;
        CONFIGV2.mqttConfig.clientId  = mqtt["clientId"]  | CONFIGV2.mqttConfig.clientId;

        if(CONFIGV2.system.debugMode) {
            Serial.println(F("MQTT configuration imported:"));
            Serial.print(F("  MQTT enabled: "));      Serial.println(CONFIGV2.mqttConfig.enabled ? F("true") : F("false"));
            Serial.print(F("  MQTT server: "));       Serial.println(CONFIGV2.mqttConfig.server);
            Serial.print(F("  MQTT port: "));         Serial.println(CONFIGV2.mqttConfig.port);
            Serial.print(F("  MQTT user: "));         Serial.println(CONFIGV2.mqttConfig.user);
            Serial.print(F("  MQTT baseTopic: "));    Serial.println(CONFIGV2.mqttConfig.baseTopic);
            Serial.print(F("  MQTT clientId: "));     Serial.println(CONFIGV2.mqttConfig.clientId);
        }
    }

    // =========================
    // Persistieren im V2-Format
    // =========================
    g_applyConfigPending = true;
    return saveConfigV2();   // nutzt deine neue saveConfig()
}


uint32_t colorFromArrayV2(JsonArrayConst arr) {
    return ((uint32_t)arr[0] << 16) |
           ((uint32_t)arr[1] << 8)  |
           (uint32_t)arr[2];
}

void setColorArrayV2(JsonObject& opt, const char* key, uint32_t color) {
  JsonArray arr = opt[key].to<JsonArray>();
  arr.add((color >> 16) & 0xFF);
  arr.add((color >>  8) & 0xFF);
  arr.add((color      ) & 0xFF);
}


// ============================================================================
// JSON-Helper für UI / Import / Export
// ============================================================================

bool loadConfigAsJsonV2(JsonObject target) {
  if (!LittleFS.exists("/config_v2.json")) return false;

  File f = LittleFS.open("/config_v2.json", "r");
  if (!f) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err || !doc.is<JsonObject>()) return false;

  // Flach kopieren (wie gehabt)
  for (JsonPair kv : doc.as<JsonObject>()) {
    target[kv.key()] = kv.value();
  }
  return true;
}

bool loadConfigAsStringV2(String& out) {
  if (!LittleFS.exists("/config_v2.json")) {
    out = "{}";
    return false;
  }
  File f = LittleFS.open("/config_v2.json", "r");
  if (!f) return false;

  out = f.readString();
  f.close();
  return true;
}