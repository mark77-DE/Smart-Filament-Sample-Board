#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct LedConfig {
    int count;
    int pin;
    int brightness;
    int timeout;
    uint32_t color;
    uint32_t colorError;
    uint32_t colorPulse;
};

struct NfcLedConfig {
    int count;
    int pin;
    int brightness;
    int timeout;
    uint32_t colorSuccess;
    uint32_t colorError;
    uint32_t colorPulse;
    bool     successBlinkEnabled;
    int  successBlinkCount;
    int successBlinkMs;
};

struct AppConfig {
    bool darkmode;
    bool mqtt;
    bool debugMode;

    LedConfig led;
    NfcLedConfig nfc;
};

extern AppConfig CONFIG;

// API
bool loadConfig();
void applyConfig();
bool saveConfig();

bool updateConfigFromJson(ArduinoJson::V742PB22::JsonDocument& doc);

// Liefert config.json als JsonObject in ein bestehendes Dokument
bool loadConfigAsJson(JsonObject target);

bool loadFilamentsAsJson(JsonArray target);

// Optional: direkt als String (für Download)
bool loadConfigAsString(String& out);

// neue Funktion zum Import von JSON
bool importConfigJson(JsonObject src);
