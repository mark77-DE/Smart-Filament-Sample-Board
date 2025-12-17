#include "filehandling.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include "ledctrl_filament.h"
#include "ledctrl_nfc.h"
#include "filament_db.h"

AppConfig CONFIG;

bool loadConfig() {
    
    if(!LittleFS.begin(true)){
        Serial.println("LittleFS mount failed!");
        while(1);
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

    CONFIG.darkmode = opt["darkmode"] | false;
    CONFIG.mqtt     = opt["mqtt"] | false;
    CONFIG.debugMode = opt["debugMode"] | false;

    CONFIG.led.timeout    = opt["ledTimeout"]    | 3000;
    CONFIG.led.pin        = opt["ledPin"]        | 4;
    CONFIG.led.count      = opt["ledCount"]      | 8;
    CONFIG.led.brightness = opt["ledBrightness"] | 50;    


    // LED-Farben
    if (opt["ledColor"].is<JsonArray>()) {
        JsonArray c = opt["ledColor"];
        CONFIG.led.color = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
    } else {
        CONFIG.led.color = 0xFFFFFF; // Default
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

    
    // NFC
    if (opt["nfcLedColorSuccess"].is<JsonArray>()) {
        JsonArray c = opt["nfcLedColorSuccess"];
        CONFIG.nfc.colorSuccess = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
    } else {
        CONFIG.nfc.colorSuccess = 0xFFFF00; // Default
    }
    
    if (opt["nfcLedColorError"].is<JsonArray>()) {
        JsonArray c = opt["nfcLedColorError"];
        CONFIG.nfc.colorError = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
    } else {
        CONFIG.nfc.colorError = 0x00FFFF; // Default
    }

    if (opt["nfcLedColorPulse"].is<JsonArray>()) {
        JsonArray c = opt["nfcLedColorPulse"];
        CONFIG.nfc.colorPulse = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
    } else {
        CONFIG.nfc.colorPulse = 0xFF00FF; // Default
    }

    // Jetzt die übrigen NFC-Parameter
    CONFIG.nfc.count      = opt["nfcLedCount"]      | 8;
    CONFIG.nfc.pin        = opt["nfcLedPin"]        | 15;
    CONFIG.nfc.brightness = opt["nfcLedBrightness"] | 100;
    CONFIG.nfc.timeout    = opt["nfcLedTimeout"]    | 4000;
    CONFIG.nfc.successBlinkEnabled = opt["nfcLedSuccessBlinkEnabled"] | true;
    CONFIG.nfc.successBlinkCount   = opt["nfcLedSuccessBlinkCount"]   | 3;
    CONFIG.nfc.successBlinkMs      = opt["nfcLedSuccessBlinkMs"]      | 150;

   
    loadFilaments();
    applyConfig();
    return true;
}


void applyConfig() {
    // LED Strip
    LEDCTRL_FILAMENT::init(
        CONFIG.led.count,
        CONFIG.led.pin,
        CONFIG.led.timeout,
        CONFIG.led.brightness,
        CONFIG.led.color,
        CONFIG.led.colorError,
        CONFIG.led.colorPulse
    );

    // NFC LEDs
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


    Serial.println("--------------------");
    Serial.println("Config applied:");
    Serial.print(" LED_COUNT = "); Serial.println(CONFIG.led.count);
    Serial.print(" LED_PIN = "); Serial.println(CONFIG.led.pin);
    Serial.print(" LED_TIMEOUT = "); Serial.println(CONFIG.led.timeout);
    Serial.print(" LED_BRIGHTNESS = "); Serial.println(CONFIG.led.brightness);
    Serial.print(" LED_COLOR = 0x"); Serial.println(CONFIG.led.color, HEX);
    Serial.print(" LED_COLOR_ERROR = 0x"); Serial.println(CONFIG.led.colorError, HEX);
    Serial.print(" LED_COLOR_PULSE = 0x"); Serial.println(CONFIG.led.colorPulse, HEX);

    Serial.print(" NFC_LED_COUNT = "); Serial.println(CONFIG.nfc.count);    
    Serial.print(" NFC_LED_PIN = "); Serial.println(CONFIG.nfc.pin);
    Serial.print(" NFC_LED_TIMEOUT = "); Serial.println(CONFIG.nfc.timeout);
    Serial.print(" NFC_LED_BRIGHTNESS = "); Serial.println(CONFIG.nfc.brightness);
    Serial.print(" NFC_LED_COLOR_SUCCESS = 0x"); Serial.println(CONFIG.nfc.colorSuccess, HEX);
    Serial.print(" NFC_LED_COLOR_ERROR = 0x"); Serial.println(CONFIG.nfc.colorError, HEX);
    Serial.print(" NFC_LED_COLOR_PULSE = 0x"); Serial.println(CONFIG.nfc.colorPulse, HEX);  
    Serial.print(" DEBUG_MODE = "); Serial.println(CONFIG.debugMode ? "true" : "false");
    Serial.println("--------------------");

}

bool updateConfigFromJson(ArduinoJson::V742PB22::JsonDocument &doc) {
    if (!doc["options"].is<JsonObjectConst>()) return false;
    JsonObjectConst opt = doc["options"].as<JsonObjectConst>();

    // --- LED ---
    CONFIG.led.count      = opt["ledCount"]      | CONFIG.led.count;
    CONFIG.led.pin        = opt["ledPin"]        | CONFIG.led.pin;
    CONFIG.led.brightness = opt["ledBrightness"] | CONFIG.led.brightness;
    CONFIG.led.timeout    = opt["ledTimeout"]    | CONFIG.led.timeout;

    if (opt["ledColor"].is<JsonArrayConst>()) {
        JsonArrayConst arr = opt["ledColor"].as<JsonArrayConst>();
        if (arr.size() >= 3) {
            CONFIG.led.color = ((arr[0].as<int>() & 0xFF) << 16) |
                           ((arr[1].as<int>() & 0xFF) << 8) |
                           ((arr[2].as<int>() & 0xFF));
        }
    }

    if (opt["ledColorError"].is<JsonArrayConst>()) {
        JsonArrayConst arr = opt["ledColorError"].as<JsonArrayConst>();
        if (arr.size() >= 3) {
            CONFIG.led.colorError = ((arr[0].as<int>() & 0xFF) << 16) |
                                    ((arr[1].as<int>() & 0xFF) << 8) |
                                    ((arr[2].as<int>() & 0xFF));
        }
    }

    if (opt["ledColorPulse"].is<JsonArrayConst>()) {
        JsonArrayConst arr = opt["ledColorPulse"].as<JsonArrayConst>();
        if (arr.size() >= 3) {
            CONFIG.led.colorPulse = ((arr[0].as<int>() & 0xFF) << 16) |
                                ((arr[1].as<int>() & 0xFF) << 8) |
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
            CONFIG.nfc.colorSuccess =   ((arr[0].as<int>() & 0xFF) << 16) |
                                        ((arr[1].as<int>() & 0xFF) << 8) |
                                        ((arr[2].as<int>() & 0xFF));
        }
    }

    if (opt["nfcLedColorError"].is<JsonArrayConst>()) {
        JsonArrayConst arr = opt["nfcLedColorError"].as<JsonArrayConst>();
        if (arr.size() >= 3) {
            CONFIG.nfc.colorError = ((arr[0].as<int>() & 0xFF) << 16) |
                                    ((arr[1].as<int>() & 0xFF) << 8) |
                                    ((arr[2].as<int>() & 0xFF));
        }
    }

    if (opt["nfcLedColorPulse"].is<JsonArrayConst>()) {
        JsonArrayConst arr = opt["nfcLedColorPulse"].as<JsonArrayConst>();
        if (arr.size() >= 3) {
            CONFIG.nfc.colorPulse = ((arr[0].as<int>() & 0xFF) << 16) |
                                    ((arr[1].as<int>() & 0xFF) << 8) |
                                    ((arr[2].as<int>() & 0xFF));
        }
    }

    // --- NFC Blink ---
    CONFIG.nfc.successBlinkEnabled = opt["nfcLedSuccessBlinkEnabled"] | CONFIG.nfc.successBlinkEnabled;
    CONFIG.nfc.successBlinkCount   = opt["nfcLedSuccessBlinkCount"]   | CONFIG.nfc.successBlinkCount;
    CONFIG.nfc.successBlinkMs      = opt["nfcLedSuccessBlinkMs"]      | CONFIG.nfc.successBlinkMs;

    // --- Debug ---
    CONFIG.debugMode = opt["debugMode"] | CONFIG.debugMode;

    // Speichern und anwenden
    saveConfig();
    applyConfig();

    return true;
}





bool saveConfig() {
    if (!LittleFS.begin()) return false;

    JsonDocument doc;
    JsonObject opt = doc["options"].to<JsonObject>();

    // LED
    opt["ledCount"]         = CONFIG.led.count;
    opt["ledPin"]           = CONFIG.led.pin;
    opt["ledBrightness"]    = CONFIG.led.brightness;
    opt["ledTimeout"]       = CONFIG.led.timeout;
    opt["ledColor"]         = CONFIG.led.color;
    opt["ledColorError"]    = CONFIG.led.colorError;

    // NFC
    opt["nfcLedCount"]          = CONFIG.nfc.count;
    opt["nfcLedPin"]            = CONFIG.nfc.pin;
    opt["nfcLedBrightness"]     = CONFIG.nfc.brightness;
    opt["nfcLedTimeout"]        = CONFIG.nfc.timeout;
    opt["nfcLedColorSuccess"]   = CONFIG.nfc.colorSuccess;
    opt["nfcLedColorError"]     = CONFIG.nfc.colorError;
    opt["nfcLedColorPulse"]     = CONFIG.nfc.colorPulse;

    // Sonstige Optionen
    opt["darkmode"]  = CONFIG.darkmode;
    opt["mqtt"]      = CONFIG.mqtt;
    opt["debugMode"] = CONFIG.debugMode;

    // Schreiben
    File f = LittleFS.open("/config.json", "w");
    if (!f) return false;

    if (serializeJson(doc, f) == 0) {
        f.close();
        return false;
    }

    f.close();
    return true;
}




bool loadConfigAsJson(JsonObject target) {
    if (!LittleFS.exists("/config.json")) {
        // leeres Objekt füllen
        // target ist bereits ein JsonObject, also nichts tun
        return false;
    }

    File f = LittleFS.open("/config.json", "r");
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err || !doc.is<JsonObject>()) {
        // Fehler → Zielobjekt leer lassen
        return false;
    }

    // Alle Schlüssel/Werte ins Zielobjekt kopieren
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


bool loadFilamentsAsJson(JsonArray target) {
    if (!LittleFS.exists("/filaments.json")) {
        // leeres Array, nichts tun
        return false;
    }

    File f = LittleFS.open("/filaments.json", "r");
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (!err && doc.is<JsonArray>()) {
        // Alle Elemente ins Ziel-Array kopieren
        for (JsonVariant v : doc.as<JsonArray>()) {
            target.add(v);
        }
        return true;
    }

    // Fehler → leeres Array
    return false;
}


bool importConfigJson(JsonObject src) {
    if (!LittleFS.begin(true)) return false;

    // Optional: bestehende Config laden und Werte überschreiben
    AppConfig old = CONFIG; // Backup




    // Optionen
    if (src.containsKey("options")) {
        JsonObject opt = src["options"];
        CONFIG.darkmode  = opt["darkmode"] | CONFIG.darkmode;
        CONFIG.mqtt      = opt["mqtt"] | CONFIG.mqtt;
        CONFIG.debugMode = opt["debugMode"] | CONFIG.debugMode;

        CONFIG.led.count      = opt["ledCount"]      | CONFIG.led.count;
        CONFIG.led.pin        = opt["ledPin"]        | CONFIG.led.pin;
        CONFIG.led.brightness = opt["ledBrightness"] | CONFIG.led.brightness;
        CONFIG.led.timeout    = opt["ledTimeout"]    | CONFIG.led.timeout;

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
        CONFIG.nfc.successBlinkCount   = opt["nfcLedSuccessBlinkCount"]  | CONFIG.nfc.successBlinkCount;
        CONFIG.nfc.successBlinkMs      = opt["nfcLedSuccessBlinkMs"]     | CONFIG.nfc.successBlinkMs;


        
    }

    // config speichern
    DynamicJsonDocument doc(8192);
JsonObject root = doc.to<JsonObject>();
JsonObject options = root.createNestedObject("options");

// Standardwerte
options["darkmode"] = CONFIG.darkmode;
options["mqtt"] = CONFIG.mqtt;
options["debugMode"] = CONFIG.debugMode;

// LED
options["ledCount"] = CONFIG.led.count;
options["ledPin"] = CONFIG.led.pin;
options["ledBrightness"] = CONFIG.led.brightness;
options["ledTimeout"] = CONFIG.led.timeout;
options["ledColor"] = JsonArray();
options["ledColorError"] = JsonArray();
options["ledColorPulse"] = JsonArray();

// Farben als Array speichern
JsonArray arr = options["ledColor"].to<JsonArray>();
arr.add((CONFIG.led.color >> 16) & 0xFF);
arr.add((CONFIG.led.color >> 8) & 0xFF);
arr.add(CONFIG.led.color & 0xFF);

arr = options["ledColorError"].to<JsonArray>();
arr.add((CONFIG.led.colorError >> 16) & 0xFF);
arr.add((CONFIG.led.colorError >> 8) & 0xFF);
arr.add(CONFIG.led.colorError & 0xFF);

arr = options["ledColorPulse"].to<JsonArray>();
arr.add((CONFIG.led.colorPulse >> 16) & 0xFF);
arr.add((CONFIG.led.colorPulse >> 8) & 0xFF);
arr.add(CONFIG.led.colorPulse & 0xFF);

// NFC
options["nfcLedCount"] = CONFIG.nfc.count;
options["nfcLedPin"] = CONFIG.nfc.pin;
options["nfcLedBrightness"] = CONFIG.nfc.brightness;
options["nfcLedTimeout"] = CONFIG.nfc.timeout;

arr = options["nfcLedColorSuccess"].to<JsonArray>();
arr.add((CONFIG.nfc.colorSuccess >> 16) & 0xFF);
arr.add((CONFIG.nfc.colorSuccess >> 8) & 0xFF);
arr.add(CONFIG.nfc.colorSuccess & 0xFF);

arr = options["nfcLedColorError"].to<JsonArray>();
arr.add((CONFIG.nfc.colorError >> 16) & 0xFF);
arr.add((CONFIG.nfc.colorError >> 8) & 0xFF);
arr.add(CONFIG.nfc.colorError & 0xFF);

arr = options["nfcLedColorPulse"].to<JsonArray>();
arr.add((CONFIG.nfc.colorPulse >> 16) & 0xFF);
arr.add((CONFIG.nfc.colorPulse >> 8) & 0xFF);
arr.add(CONFIG.nfc.colorPulse & 0xFF);

options["nfcLedSuccessBlinkEnabled"] = CONFIG.nfc.successBlinkEnabled;
options["nfcLedSuccessBlinkCount"] = CONFIG.nfc.successBlinkCount;
options["nfcLedSuccessBlinkMs"] = CONFIG.nfc.successBlinkMs;

// Speichern
File f = LittleFS.open("/config.json", "w");
if(!f) return false;
serializeJson(root, f);
f.close();


    applyConfig();
    return true;
}


bool loadFilaments() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    if (!loadFilamentsAsJson(arr)) return false;
    return FilamentDB::loadFromJsonArray(arr);
}



bool saveFilamentsToFile() {
    StaticJsonDocument<32*1024> doc;
    JsonArray arr = FilamentDB::toJsonArray(doc);  // direkt aus Namespace

    File f = LittleFS.open("/filaments.json", "w");
    if (!f) {
        Serial.println("saveFilamentsToFile: Cannot open file for write!");
        return false;
    }

    size_t written = serializeJson(doc, f);
    f.close();
    Serial.printf("DB saved. bytes=%u entries=%d\n", (unsigned)written, FilamentDB::getAllCount());
    return true;
}