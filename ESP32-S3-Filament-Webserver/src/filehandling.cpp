#include "filehandling.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include "globals.h"
#include "ledctrl_filament.h"
#include "ledctrl_nfc.h"
#include "filament_db.h"
#include "gpio_hardware.h"   // für gpiohw_init()




// Globale Konfiguration
AppConfig CONFIG;

// ============================================================================
// Laden / Anwenden der Konfiguration
// ============================================================================

bool loadConfig() {
  // LittleFS mounten (mit Format-on-fail = true, wie bisher genutzt)
  Serial.println("Mounting LittleFS...");
  if (!LittleFS.begin(true)) {
    if (CONFIG.debugMode) {
      Serial.println(F("LittleFS mount failed!"));
    }
    // unverändertes Verhalten: blockieren
    while (1) { delay(10); }
  }

  if (!LittleFS.exists("/config.json")) return false;

  File f = LittleFS.open("/config.json", "r");
  if (!f) return false;

  JsonDocument doc;
  auto err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  if (!doc["options"].is<JsonObject>()) return false;
  JsonObject opt = doc["options"].as<JsonObject>();

  // --- Basis-Flags ---
  CONFIG.darkmode  = opt["darkmode"]  | false;
  CONFIG.mqtt      = opt["mqtt"]      | false;
  CONFIG.debugMode = opt["debugMode"] | false;
  CONFIG.hostname  = opt["hostname"]  | "FiSaBo";

  

  // --- Filament-LED ---
  CONFIG.led.count      = opt["ledCount"]      | 8;
  CONFIG.led.pin        = opt["ledPin"]        | 4;
  CONFIG.led.brightness = opt["ledBrightness"] | 50;
  CONFIG.led.timeout    = opt["ledTimeout"]    | 3000;


  // --- Dashboard (Virtuelle LED)---
  CONFIG.webLEDTimeout = opt["webLEDTimeout"] | (uint32_t)CONFIG.led.timeout; // Fallback auf ledTimeout



  if (opt["ledColor"].is<JsonArray>()) {
    JsonArray c = opt["ledColor"];
    CONFIG.led.color = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
  } else {
    CONFIG.led.color = 0xFFFFFF;
  }

  if (opt["ledColorError"].is<JsonArray>()) {
    JsonArray e = opt["ledColorError"];
    CONFIG.led.colorError = ((uint32_t)e[0] << 16) | ((uint32_t)e[1] << 8) | (uint32_t)e[2];
  } else {
    CONFIG.led.colorError = 0xFF0000;
  }

  if (opt["ledColorPulse"].is<JsonArray>()) {
    JsonArray p = opt["ledColorPulse"];
    CONFIG.led.colorPulse = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
  } else {
    CONFIG.led.colorPulse = 0x0000FF;
  }

  // --- NFC-LED ---
  CONFIG.nfc.count      = opt["nfcLedCount"]      | 8;
  CONFIG.nfc.pin        = opt["nfcLedPin"]        | 15;
  CONFIG.nfc.brightness = opt["nfcLedBrightness"] | 100;
  CONFIG.nfc.timeout    = opt["nfcLedTimeout"]    | 4000;

  if (opt["nfcLedColorSuccess"].is<JsonArray>()) {
    JsonArray c = opt["nfcLedColorSuccess"];
    CONFIG.nfc.colorSuccess = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
  } else {
    CONFIG.nfc.colorSuccess = 0xFFFF00;
  }

  if (opt["nfcLedColorError"].is<JsonArray>()) {
    JsonArray c = opt["nfcLedColorError"];
    CONFIG.nfc.colorError = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
  } else {
    CONFIG.nfc.colorError = 0x00FFFF;
  }

  if (opt["nfcLedColorPulse"].is<JsonArray>()) {
    JsonArray c = opt["nfcLedColorPulse"];
    CONFIG.nfc.colorPulse = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
  } else {
    CONFIG.nfc.colorPulse = 0xFF00FF;
  }

  CONFIG.nfc.successBlinkEnabled = opt["nfcLedSuccessBlinkEnabled"] | true;
  CONFIG.nfc.successBlinkCount   = opt["nfcLedSuccessBlinkCount"]   | 3;
  CONFIG.nfc.successBlinkMs      = opt["nfcLedSuccessBlinkMs"]      | 150;

  // --- Button ---
  CONFIG.button.pin          = opt["buttonPin"]         | CONFIG.button.pin;
  CONFIG.button.pullup       = opt["buttonPullup"]      | CONFIG.button.pullup;
  CONFIG.button.debounceMs   = opt["buttonDebounceMs"]  | CONFIG.button.debounceMs;
  CONFIG.button.longMs       = opt["buttonLongMs"]      | CONFIG.button.longMs;
  CONFIG.button.doubleGapMs  = opt["buttonDoubleMs"]    | CONFIG.button.doubleGapMs;
  CONFIG.button.holdRepeatMs = opt["buttonHoldMs"]      | CONFIG.button.holdRepeatMs;

  // --- Buzzer ---
  CONFIG.buzzer.pin          = opt["buzzerPin"]         | CONFIG.buzzer.pin;
  CONFIG.buzzer.activeHigh   = opt["buzzerActiveHigh"]  | CONFIG.buzzer.activeHigh;
  CONFIG.buzzer.passive      = opt["buzzerPassive"]     | CONFIG.buzzer.passive;
  CONFIG.buzzer.freqHz       = opt["buzzerFreq"]        | CONFIG.buzzer.freqHz;
  CONFIG.buzzer.singleMs     = opt["buzzerSingleMs"]    | CONFIG.buzzer.singleMs;
  CONFIG.buzzer.doubleOnMs   = opt["buzzerDoubleOnMs"]  | CONFIG.buzzer.doubleOnMs;
  CONFIG.buzzer.doubleGapMs  = opt["buzzerDoubleGapMs"] | CONFIG.buzzer.doubleGapMs;
  CONFIG.buzzer.errorOnMs    = opt["buzzerErrorOnMs"]   | CONFIG.buzzer.errorOnMs;
  CONFIG.buzzer.errorGapMs   = opt["buzzerErrorGapMs"]  | CONFIG.buzzer.errorGapMs;
  CONFIG.buzzer.errorCount   = opt["buzzerErrorCount"]  | CONFIG.buzzer.errorCount;

  // Filament-DB laden & Konfiguration anwenden
  loadFilaments();
  return true;
}

void applyConfig() {
  // Filament-LEDs
  LEDCTRL_FILAMENT::init(
    CONFIG.led.count,
    CONFIG.led.pin,
    CONFIG.led.timeout,
    CONFIG.led.brightness,
    CONFIG.led.color,
    CONFIG.led.colorError,
    CONFIG.led.colorPulse
  );

  // NFC-LEDs
  LEDCTRL_NFC::init(
    CONFIG.nfc.count,
    CONFIG.nfc.pin,
    CONFIG.nfc.timeout,
    CONFIG.nfc.brightness,
    CONFIG.nfc.colorSuccess,
    CONFIG.nfc.colorError,
    CONFIG.nfc.colorPulse,
    CONFIG.nfc.successBlinkEnabled,
    CONFIG.nfc.successBlinkCount,
    CONFIG.nfc.successBlinkMs
  );

  // GPIO-Hardware (Button/Buzzer)
  gpiohw_init();  // liest CONFIG.button / CONFIG.buzzer, richtet Pins & ISR/Timer ein

  if (CONFIG.debugMode) {
    Serial.println(F("--------------------"));
    Serial.println(F("Config V1 applied:"));

    Serial.print(F(" HOSTNAME = "));       Serial.println(CONFIG.hostname);

    Serial.print(F(" LED_COUNT = "));         Serial.println(CONFIG.led.count);
    Serial.print(F(" LED_PIN = "));           Serial.println(CONFIG.led.pin);
    Serial.print(F(" LED_TIMEOUT = "));       Serial.println(CONFIG.led.timeout);
    Serial.print(F(" LED_BRIGHTNESS = "));    Serial.println(CONFIG.led.brightness);
    Serial.print(F(" LED_COLOR = 0x"));       Serial.println(CONFIG.led.color, HEX);
    Serial.print(F(" LED_COLOR_ERROR = 0x")); Serial.println(CONFIG.led.colorError, HEX);
    Serial.print(F(" LED_COLOR_PULSE = 0x")); Serial.println(CONFIG.led.colorPulse, HEX);

    Serial.print(F(" WEB_LED_TIMEOUT = ")); Serial.println(CONFIG.webLEDTimeout);


    Serial.print(F(" NFC_LED_COUNT = "));     Serial.println(CONFIG.nfc.count);
    Serial.print(F(" NFC_LED_PIN = "));       Serial.println(CONFIG.nfc.pin);
    Serial.print(F(" NFC_LED_TIMEOUT = "));   Serial.println(CONFIG.nfc.timeout);
    Serial.print(F(" NFC_LED_BRIGHTNESS = "));Serial.println(CONFIG.nfc.brightness);
    Serial.print(F(" NFC_LED_COLOR_SUCCESS = 0x")); Serial.println(CONFIG.nfc.colorSuccess, HEX);
    Serial.print(F(" NFC_LED_COLOR_ERROR = 0x"));   Serial.println(CONFIG.nfc.colorError, HEX);
    Serial.print(F(" NFC_LED_COLOR_PULSE = 0x"));   Serial.println(CONFIG.nfc.colorPulse, HEX);
    Serial.print(F(" DEBUG_MODE = "));        Serial.println(CONFIG.debugMode ? F("true") : F("false"));

    Serial.print(F(" BUTTON pin="));          Serial.print(CONFIG.button.pin);
    Serial.print(F(" pullup="));              Serial.print(CONFIG.button.pullup);
    Serial.print(F(" debounce="));            Serial.print(CONFIG.button.debounceMs);
    Serial.print(F("ms long="));              Serial.print(CONFIG.button.longMs);
    Serial.print(F("ms double="));            Serial.print(CONFIG.button.doubleGapMs);
    Serial.print(F("ms holdRep="));           Serial.print(CONFIG.button.holdRepeatMs);
    Serial.println(F("ms"));

    Serial.print(F(" BUZZER pin="));          Serial.print(CONFIG.buzzer.pin);
    Serial.print(F(" activeHigh="));          Serial.print(CONFIG.buzzer.activeHigh);
    Serial.print(F(" freq="));                Serial.print(CONFIG.buzzer.freqHz);
    Serial.print(F("Hz single="));            Serial.print(CONFIG.buzzer.singleMs);
    Serial.print(F("ms dblOn="));             Serial.print(CONFIG.buzzer.doubleOnMs);
    Serial.print(F("ms dblGap="));            Serial.print(CONFIG.buzzer.doubleGapMs);
    Serial.print(F("ms errOn="));             Serial.print(CONFIG.buzzer.errorOnMs);
    Serial.print(F("ms errGap="));            Serial.print(CONFIG.buzzer.errorGapMs);
    Serial.print(F("ms errCount="));          Serial.println(CONFIG.buzzer.errorCount);

    Serial.println(F("--------------------"));
  }
}

// ============================================================================
// JSON-basierte Aktualisierung / Persistenz
// ============================================================================

bool updateConfigFromJson(JsonDocument& doc) {
  if (!doc["options"].is<JsonObjectConst>()) return false;
  JsonObjectConst opt = doc["options"].as<JsonObjectConst>();

  // --- LED ---
  CONFIG.led.count      = opt["ledCount"]      | CONFIG.led.count;
  CONFIG.led.pin        = opt["ledPin"]        | CONFIG.led.pin;
  CONFIG.led.brightness = opt["ledBrightness"] | CONFIG.led.brightness;
  CONFIG.led.timeout    = opt["ledTimeout"]    | CONFIG.led.timeout;

  // --- Dashboard (Virtuelle LED)---
  CONFIG.webLEDTimeout  = opt["webLEDTimeout"] | CONFIG.webLEDTimeout;



  if (opt["ledColor"].is<JsonArrayConst>()) {
    JsonArrayConst arr = opt["ledColor"].as<JsonArrayConst>();
    if (arr.size() >= 3) {
      CONFIG.led.color = ((arr[0].as<int>() & 0xFF) << 16) |
                         ((arr[1].as<int>() & 0xFF) <<  8) |
                         ((arr[2].as<int>() & 0xFF));
    }
  }
  if (opt["ledColorError"].is<JsonArrayConst>()) {
    JsonArrayConst arr = opt["ledColorError"].as<JsonArrayConst>();
    if (arr.size() >= 3) {
      CONFIG.led.colorError = ((arr[0].as<int>() & 0xFF) << 16) |
                              ((arr[1].as<int>() & 0xFF) <<  8) |
                              ((arr[2].as<int>() & 0xFF));
    }
  }
  if (opt["ledColorPulse"].is<JsonArrayConst>()) {
    JsonArrayConst arr = opt["ledColorPulse"].as<JsonArrayConst>();
    if (arr.size() >= 3) {
      CONFIG.led.colorPulse = ((arr[0].as<int>() & 0xFF) << 16) |
                              ((arr[1].as<int>() & 0xFF) <<  8) |
                              ((arr[2].as<int>() & 0xFF));
    }
  }

  // --- NFC LED ---
  CONFIG.nfc.count      = opt["nfcLedCount"]      | CONFIG.nfc.count;
  CONFIG.nfc.pin        = opt["nfcLedPin"]        | CONFIG.nfc.pin;
  CONFIG.nfc.brightness = opt["nfcLedBrightness"] | CONFIG.nfc.brightness;
  CONFIG.nfc.timeout    = opt["nfcLedTimeout"]    | CONFIG.nfc.timeout;

  if (opt["nfcLedColorSuccess"].is<JsonArrayConst>()) {
    JsonArrayConst arr = opt["nfcLedColorSuccess"].as<JsonArrayConst>();
    if (arr.size() >= 3) {
      CONFIG.nfc.colorSuccess = ((arr[0].as<int>() & 0xFF) << 16) |
                                ((arr[1].as<int>() & 0xFF) <<  8) |
                                ((arr[2].as<int>() & 0xFF));
    }
  }
  if (opt["nfcLedColorError"].is<JsonArrayConst>()) {
    JsonArrayConst arr = opt["nfcLedColorError"].as<JsonArrayConst>();
    if (arr.size() >= 3) {
      CONFIG.nfc.colorError = ((arr[0].as<int>() & 0xFF) << 16) |
                              ((arr[1].as<int>() & 0xFF) <<  8) |
                              ((arr[2].as<int>() & 0xFF));
    }
  }
  if (opt["nfcLedColorPulse"].is<JsonArrayConst>()) {
    JsonArrayConst arr = opt["nfcLedColorPulse"].as<JsonArrayConst>();
    if (arr.size() >= 3) {
      CONFIG.nfc.colorPulse = ((arr[0].as<int>() & 0xFF) << 16) |
                              ((arr[1].as<int>() & 0xFF) <<  8) |
                              ((arr[2].as<int>() & 0xFF));
    }
  }

  // --- NFC Blink ---
  CONFIG.nfc.successBlinkEnabled = opt["nfcLedSuccessBlinkEnabled"] | CONFIG.nfc.successBlinkEnabled;
  CONFIG.nfc.successBlinkCount   = opt["nfcLedSuccessBlinkCount"]   | CONFIG.nfc.successBlinkCount;
  CONFIG.nfc.successBlinkMs      = opt["nfcLedSuccessBlinkMs"]      | CONFIG.nfc.successBlinkMs;

  // --- Button ---
  CONFIG.button.pin          = opt["buttonPin"]         | CONFIG.button.pin;
  CONFIG.button.pullup       = opt["buttonPullup"]      | CONFIG.button.pullup;
  CONFIG.button.debounceMs   = opt["buttonDebounceMs"]  | CONFIG.button.debounceMs;
  CONFIG.button.longMs       = opt["buttonLongMs"]      | CONFIG.button.longMs;
  CONFIG.button.doubleGapMs  = opt["buttonDoubleMs"]    | CONFIG.button.doubleGapMs;
  CONFIG.button.holdRepeatMs = opt["buttonHoldMs"]      | CONFIG.button.holdRepeatMs;

  // --- Buzzer ---
  CONFIG.buzzer.pin          = opt["buzzerPin"]         | CONFIG.buzzer.pin;
  CONFIG.buzzer.activeHigh   = opt["buzzerActiveHigh"]  | CONFIG.buzzer.activeHigh;
  CONFIG.buzzer.passive      = opt["buzzerPassive"]     | CONFIG.buzzer.passive;
  CONFIG.buzzer.freqHz       = opt["buzzerFreq"]        | CONFIG.buzzer.freqHz;
  CONFIG.buzzer.singleMs     = opt["buzzerSingleMs"]    | CONFIG.buzzer.singleMs;
  CONFIG.buzzer.doubleOnMs   = opt["buzzerDoubleOnMs"]  | CONFIG.buzzer.doubleOnMs;
  CONFIG.buzzer.doubleGapMs  = opt["buzzerDoubleGapMs"] | CONFIG.buzzer.doubleGapMs;
  CONFIG.buzzer.errorOnMs    = opt["buzzerErrorOnMs"]   | CONFIG.buzzer.errorOnMs;
  CONFIG.buzzer.errorGapMs   = opt["buzzerErrorGapMs"]  | CONFIG.buzzer.errorGapMs;
  CONFIG.buzzer.errorCount   = opt["buzzerErrorCount"]  | CONFIG.buzzer.errorCount;

  if (CONFIG.debugMode) {
    Serial.printf("Updating debug mode from JSON: %s\n",
                  (opt["debugMode"].is<bool>() && opt["debugMode"].as<bool>()) ? "on" : "off");
  }
  CONFIG.debugMode = opt["debugMode"] | CONFIG.debugMode;

  // Persistieren & anwenden
  saveConfig();
  g_applyConfigPending = true;
  return true;
}

bool saveConfig() {
  if (!LittleFS.begin()) return false;

  JsonDocument doc;
  JsonObject opt = doc["options"].to<JsonObject>();

  // --- LED ---
  opt["ledCount"]      = CONFIG.led.count;
  opt["ledPin"]        = CONFIG.led.pin;
  opt["ledBrightness"] = CONFIG.led.brightness;
  opt["ledTimeout"]    = CONFIG.led.timeout;
  setColorArray(opt, "ledColor",       CONFIG.led.color);
  setColorArray(opt, "ledColorError",  CONFIG.led.colorError);
  setColorArray(opt, "ledColorPulse",  CONFIG.led.colorPulse);



  // --- Dashboard (Virtuelle LED)---
  opt["webLEDTimeout"] = CONFIG.webLEDTimeout;


  // --- NFC ---
  opt["nfcLedCount"]       = CONFIG.nfc.count;
  opt["nfcLedPin"]         = CONFIG.nfc.pin;
  opt["nfcLedBrightness"]  = CONFIG.nfc.brightness;
  opt["nfcLedTimeout"]     = CONFIG.nfc.timeout;
  setColorArray(opt, "nfcLedColorSuccess", CONFIG.nfc.colorSuccess);
  setColorArray(opt, "nfcLedColorError",   CONFIG.nfc.colorError);
  setColorArray(opt, "nfcLedColorPulse",   CONFIG.nfc.colorPulse);
  opt["nfcLedSuccessBlinkEnabled"] = CONFIG.nfc.successBlinkEnabled;
  opt["nfcLedSuccessBlinkCount"]   = CONFIG.nfc.successBlinkCount;
  opt["nfcLedSuccessBlinkMs"]      = CONFIG.nfc.successBlinkMs;

  // --- Button ---
  opt["buttonPin"]        = CONFIG.button.pin;
  opt["buttonPullup"]     = CONFIG.button.pullup;
  opt["buttonDebounceMs"] = CONFIG.button.debounceMs;
  opt["buttonLongMs"]     = CONFIG.button.longMs;
  opt["buttonDoubleMs"]   = CONFIG.button.doubleGapMs;
  opt["buttonHoldMs"]     = CONFIG.button.holdRepeatMs;

  // --- Buzzer ---
  opt["buzzerPin"]         = CONFIG.buzzer.pin;
  opt["buzzerActiveHigh"]  = CONFIG.buzzer.activeHigh;
  opt["buzzerPassive"]     = CONFIG.buzzer.passive;
  opt["buzzerFreq"]        = CONFIG.buzzer.freqHz;
  opt["buzzerSingleMs"]    = CONFIG.buzzer.singleMs;
  opt["buzzerDoubleOnMs"]  = CONFIG.buzzer.doubleOnMs;
  opt["buzzerDoubleGapMs"] = CONFIG.buzzer.doubleGapMs;
  opt["buzzerErrorOnMs"]   = CONFIG.buzzer.errorOnMs;
  opt["buzzerErrorGapMs"]  = CONFIG.buzzer.errorGapMs;
  opt["buzzerErrorCount"]  = CONFIG.buzzer.errorCount;

  // --- Sonstige ---
  opt["darkmode"]  = CONFIG.darkmode;
  opt["mqtt"]      = CONFIG.mqtt;
  opt["debugMode"] = CONFIG.debugMode;

  // Schreiben
  File f = LittleFS.open("/config.json", "w");
  if (!f) return false;
  const size_t written = serializeJson(doc, f);
  f.close();
  return (written > 0);
}

// ============================================================================
// JSON-Helper für UI / Import / Export
// ============================================================================

bool loadConfigAsJson(JsonObject target) {
  if (!LittleFS.exists("/config.json")) return false;

  File f = LittleFS.open("/config.json", "r");
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

bool loadConfigAsString(String& out) {
  if (!LittleFS.exists("/config.json")) {
    out = "{}";
    return false;
  }
  File f = LittleFS.open("/config.json", "r");
  if (!f) return false;

  out = f.readString();
  f.close();
  return true;
}

bool importConfigJson(JsonObject src) {
  if (!LittleFS.begin(true)) return false;

  // Optional: bestehende CONFIG sichern (hier ungenutzt)
  AppConfig old = CONFIG;
  (void)old;

  // --- Optionen aus src übernehmen (falls vorhanden) ---
  if (src.containsKey("options")) {
    JsonObject opt = src["options"];

    // Basis
    CONFIG.darkmode  = opt["darkmode"]  | CONFIG.darkmode;
    CONFIG.mqtt      = opt["mqtt"]      | CONFIG.mqtt;
    CONFIG.debugMode = opt["debugMode"] | CONFIG.debugMode;

    // LED
    CONFIG.led.count      = opt["ledCount"]      | CONFIG.led.count;
    CONFIG.led.pin        = opt["ledPin"]        | CONFIG.led.pin;
    CONFIG.led.brightness = opt["ledBrightness"] | CONFIG.led.brightness;
    CONFIG.led.timeout    = opt["ledTimeout"]    | CONFIG.led.timeout;


    // --- Dashboard (Virtuelle LED)---
    CONFIG.webLEDTimeout  = opt["webLEDTimeout"] | CONFIG.webLEDTimeout;


    if (opt["ledColor"].is<JsonArray>()) {
      JsonArray arr = opt["ledColor"];
      CONFIG.led.color = ((uint32_t)arr[0] << 16) | ((uint32_t)arr[1] << 8) | (uint32_t)arr[2];
    }
    if (opt["ledColorError"].is<JsonArray>()) {
      JsonArray arr = opt["ledColorError"];
      CONFIG.led.colorError = ((uint32_t)arr[0] << 16) | ((uint32_t)arr[1] << 8) | (uint32_t)arr[2];
    }
    if (opt["ledColorPulse"].is<JsonArray>()) {
      JsonArray arr = opt["ledColorPulse"];
      CONFIG.led.colorPulse = ((uint32_t)arr[0] << 16) | ((uint32_t)arr[1] << 8) | (uint32_t)arr[2];
    }

    // NFC
    CONFIG.nfc.count      = opt["nfcLedCount"]      | CONFIG.nfc.count;
    CONFIG.nfc.pin        = opt["nfcLedPin"]        | CONFIG.nfc.pin;
    CONFIG.nfc.brightness = opt["nfcLedBrightness"] | CONFIG.nfc.brightness;
    CONFIG.nfc.timeout    = opt["nfcLedTimeout"]    | CONFIG.nfc.timeout;

    if (opt["nfcLedColorSuccess"].is<JsonArray>()) {
      JsonArray arr = opt["nfcLedColorSuccess"];
      CONFIG.nfc.colorSuccess = ((uint32_t)arr[0] << 16) | ((uint32_t)arr[1] << 8) | (uint32_t)arr[2];
    }
    if (opt["nfcLedColorError"].is<JsonArray>()) {
      JsonArray arr = opt["nfcLedColorError"];
      CONFIG.nfc.colorError = ((uint32_t)arr[0] << 16) | ((uint32_t)arr[1] << 8) | (uint32_t)arr[2];
    }
    if (opt["nfcLedColorPulse"].is<JsonArray>()) {
      JsonArray arr = opt["nfcLedColorPulse"];
      CONFIG.nfc.colorPulse = ((uint32_t)arr[0] << 16) | ((uint32_t)arr[1] << 8) | (uint32_t)arr[2];
    }

    CONFIG.nfc.successBlinkEnabled = opt["nfcLedSuccessBlinkEnabled"] | CONFIG.nfc.successBlinkEnabled;
    CONFIG.nfc.successBlinkCount   = opt["nfcLedSuccessBlinkCount"]   | CONFIG.nfc.successBlinkCount;
    CONFIG.nfc.successBlinkMs      = opt["nfcLedSuccessBlinkMs"]      | CONFIG.nfc.successBlinkMs;

    // Button
    CONFIG.button.pin          = opt["buttonPin"]         | CONFIG.button.pin;
    CONFIG.button.pullup       = opt["buttonPullup"]      | CONFIG.button.pullup;
    CONFIG.button.debounceMs   = opt["buttonDebounceMs"]  | CONFIG.button.debounceMs;
    CONFIG.button.longMs       = opt["buttonLongMs"]      | CONFIG.button.longMs;
    CONFIG.button.doubleGapMs  = opt["buttonDoubleMs"]    | CONFIG.button.doubleGapMs;
    CONFIG.button.holdRepeatMs = opt["buttonHoldMs"]      | CONFIG.button.holdRepeatMs;

    // Buzzer
    CONFIG.buzzer.pin          = opt["buzzerPin"]         | CONFIG.buzzer.pin;
    CONFIG.buzzer.activeHigh   = opt["buzzerActiveHigh"]  | CONFIG.buzzer.activeHigh;
    CONFIG.buzzer.freqHz       = opt["buzzerFreq"]        | CONFIG.buzzer.freqHz;
    CONFIG.buzzer.singleMs     = opt["buzzerSingleMs"]    | CONFIG.buzzer.singleMs;
    CONFIG.buzzer.doubleOnMs   = opt["buzzerDoubleOnMs"]  | CONFIG.buzzer.doubleOnMs;
    CONFIG.buzzer.doubleGapMs  = opt["buzzerDoubleGapMs"] | CONFIG.buzzer.doubleGapMs;
    CONFIG.buzzer.errorOnMs    = opt["buzzerErrorOnMs"]   | CONFIG.buzzer.errorOnMs;
    CONFIG.buzzer.errorGapMs   = opt["buzzerErrorGapMs"]  | CONFIG.buzzer.errorGapMs;
    CONFIG.buzzer.errorCount   = opt["buzzerErrorCount"]  | CONFIG.buzzer.errorCount;
  }

  // Datei persistieren (gleiches Format wie saveConfig)
  JsonDocument doc;
  JsonObject root    = doc.to<JsonObject>();
  JsonObject options = root.createNestedObject("options");

  // Basis
  options["darkmode"]  = CONFIG.darkmode;
  options["mqtt"]      = CONFIG.mqtt;
  options["debugMode"] = CONFIG.debugMode;

  // LED
  options["ledCount"]      = CONFIG.led.count;
  options["ledPin"]        = CONFIG.led.pin;
  options["ledBrightness"] = CONFIG.led.brightness;
  options["ledTimeout"]    = CONFIG.led.timeout;
  setColorArray(options, "ledColor",      CONFIG.led.color);
  setColorArray(options, "ledColorError", CONFIG.led.colorError);
  setColorArray(options, "ledColorPulse", CONFIG.led.colorPulse);


  // --- Dashboard (Virtuelle LED)---
  options["webLEDTimeout"] = CONFIG.webLEDTimeout;


  // NFC
  options["nfcLedCount"]       = CONFIG.nfc.count;
  options["nfcLedPin"]         = CONFIG.nfc.pin;
  options["nfcLedBrightness"]  = CONFIG.nfc.brightness;
  options["nfcLedTimeout"]     = CONFIG.nfc.timeout;
  setColorArray(options, "nfcLedColorSuccess", CONFIG.nfc.colorSuccess);
  setColorArray(options, "nfcLedColorError",   CONFIG.nfc.colorError);
  setColorArray(options, "nfcLedColorPulse",   CONFIG.nfc.colorPulse);
  options["nfcLedSuccessBlinkEnabled"] = CONFIG.nfc.successBlinkEnabled;
  options["nfcLedSuccessBlinkCount"]   = CONFIG.nfc.successBlinkCount;
  options["nfcLedSuccessBlinkMs"]      = CONFIG.nfc.successBlinkMs;

  // Button
  options["buttonPin"]        = CONFIG.button.pin;
  options["buttonPullup"]     = CONFIG.button.pullup;
  options["buttonDebounceMs"] = CONFIG.button.debounceMs;
  options["buttonLongMs"]     = CONFIG.button.longMs;
  options["buttonDoubleMs"]   = CONFIG.button.doubleGapMs;
  options["buttonHoldMs"]     = CONFIG.button.holdRepeatMs;

  // Buzzer
  options["buzzerPin"]         = CONFIG.buzzer.pin;
  options["buzzerActiveHigh"]  = CONFIG.buzzer.activeHigh;
  options["buzzerFreq"]        = CONFIG.buzzer.freqHz;
  options["buzzerSingleMs"]    = CONFIG.buzzer.singleMs;
  options["buzzerDoubleOnMs"]  = CONFIG.buzzer.doubleOnMs;
  options["buzzerDoubleGapMs"] = CONFIG.buzzer.doubleGapMs;
  options["buzzerErrorOnMs"]   = CONFIG.buzzer.errorOnMs;
  options["buzzerErrorGapMs"]  = CONFIG.buzzer.errorGapMs;
  options["buzzerErrorCount"]  = CONFIG.buzzer.errorCount;

  File f = LittleFS.open("/config.json", "w");
  if (!f) return false;
  serializeJson(root, f);
  f.close();

  g_applyConfigPending = true;
  return true;
}

// ============================================================================
// Filament-DB
// ============================================================================

bool loadFilaments() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  if (!loadFilamentsAsJson(arr)) return false;
  return FilamentDB::loadFromJsonArray(arr);
}

bool saveFilamentsToFile() {
  JsonDocument doc;
  JsonArray arr = FilamentDB::toJsonArray(doc);  // liefert Array im doc

  File f = LittleFS.open("/filaments.json", "w");
  if (!f) {
    if (CONFIG.debugMode) {
      Serial.println(F("saveFilamentsToFile: Cannot open file for write!"));
    }
    return false;
  }

  size_t written = serializeJson(doc, f);
  f.close();

  if (CONFIG.debugMode) {
    Serial.printf("DB saved. bytes=%u entries=%d\n",
                  (unsigned)written, FilamentDB::getAllCount());
  }
  return true;
}

bool loadFilamentsAsJson(JsonArray target) {
  if (!LittleFS.exists("/filaments.json")) return false;

  File f = LittleFS.open("/filaments.json", "r");
  if (!f) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (!err && doc.is<JsonArray>()) {
    for (JsonVariant v : doc.as<JsonArray>()) {
      target.add(v);
    }
    return true;
  }
  return false;
}

bool importFilamentsJson(JsonArray src) {
  if (src.isNull()) {
    if (CONFIG.debugMode) Serial.println(F("importFilamentsJson: src is null"));
    return false;
  }
  if (src.size() == 0) {
    if (CONFIG.debugMode) Serial.println(F("importFilamentsJson: empty array"));
    return false;
  }

  if (CONFIG.debugMode) {
    Serial.printf("Importing %u filaments...\n", src.size());
  }

  // 1) In DB laden (überschreibt intern die bestehende DB)
  if (!FilamentDB::loadFromJsonArray(src)) {
    if (CONFIG.debugMode) {
      Serial.println(F("importFilamentsJson: FilamentDB loadFromJsonArray failed"));
    }
    return false;
  }

  // 2) Persistieren
  if (!saveFilamentsToFile()) {
    if (CONFIG.debugMode) {
      Serial.println(F("importFilamentsJson: saving filaments failed"));
    }
    return false;
  }

  if (CONFIG.debugMode) {
    Serial.printf("Filaments imported successfully. Count=%d\n",
                  FilamentDB::getAllCount());
  }
  g_reloadFilamentsPending = true;
  return true;
}

// Optional-Hilfsfunktion (falls in filehandling.h deklariert)
bool loadFilamentDB(FilamentEntry* dst, size_t maxEntries, size_t& outCount) {
  outCount = 0;

  // Hole DB als JSON-Array (verlustfrei aus dem Namespace)
  JsonDocument doc;
  JsonArray arr = FilamentDB::toJsonArray(doc);

  for (JsonObject obj : arr) {
    if (outCount >= maxEntries) break;

    FilamentEntry e{};
    // Felder gemäß bisheriger Verwendung in handleUID()
    e.ledIndex = obj["ledIndex"] | -1;
    e.vendor   = obj["vendor"]   | String();
    e.type     = obj["type"]     | String();
    e.color    = obj["color"]    | String();
    // ggf. weitere Felder analog ergänzen

    dst[outCount++] = e;
  }
  return (outCount > 0);
}

// ============================================================================
// Utilities
// ============================================================================

void setColorArray(JsonObject& opt, const char* key, uint32_t color) {
  JsonArray arr = opt[key].to<JsonArray>();
  arr.add((color >> 16) & 0xFF);
  arr.add((color >>  8) & 0xFF);
  arr.add((color      ) & 0xFF);
}
