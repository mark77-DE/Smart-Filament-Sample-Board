#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "filament_db.h"

/**
 * @brief LED-Konfiguration
 */
struct LedConfig {
    int count;          ///< Anzahl LEDs
    int pin;            ///< GPIO-Pin
    int brightness;     ///< Helligkeit [0..255]
    int timeout;        ///< Timeout in ms
    uint32_t color;     ///< Standardfarbe 0xRRGGBB
    uint32_t colorError;///< Fehlerfarbe 0xRRGGBB
    uint32_t colorPulse;///< Idle-Pulse-Farbe 0xRRGGBB
};

/**
 * @brief NFC-LED-Konfiguration
 */
struct NfcLedConfig {
    int count;                  ///< Anzahl LEDs
    int pin;                    ///< GPIO-Pin
    int brightness;             ///< Helligkeit [0..255]
    int timeout;                ///< Timeout in ms
    uint32_t colorSuccess;      ///< Farbe für Erfolg 0xRRGGBB
    uint32_t colorError;        ///< Farbe für Fehler 0xRRGGBB
    uint32_t colorPulse;        ///< Idle-Pulse-Farbe 0xRRGGBB
    bool successBlinkEnabled;   ///< True = Erfolgsblinken aktiv
    int successBlinkCount;      ///< Anzahl Blink-Zyklen
    int successBlinkMs;         ///< Blink-Intervall in ms
};

/**
 * @brief Haupt-Konfigurationsstruktur der App
 */
struct AppConfig {
    bool darkmode;      ///< Darkmode aktiv
    bool mqtt;          ///< MQTT aktiviert
    bool debugMode;     ///< Debug-Modus aktiv

    LedConfig led;      ///< LED-Konfiguration
    NfcLedConfig nfc;   ///< NFC-LED-Konfiguration
};

extern AppConfig CONFIG;

// --------------------------------------------------------------------------
/**
 * @brief Lädt die Konfiguration aus der Datei config.json
 * @return true, wenn erfolgreich geladen, false bei Fehler
 */
// --------------------------------------------------------------------------
bool loadConfig();

// --------------------------------------------------------------------------
/**
 * @brief Wendet die aktuelle Konfiguration an (Hardware/Software)
 */
// --------------------------------------------------------------------------
void applyConfig();

// --------------------------------------------------------------------------
/**
 * @brief Speichert die aktuelle Konfiguration in config.json
 * @return true bei Erfolg, false bei Fehler
 */
// --------------------------------------------------------------------------
bool saveConfig();

// --------------------------------------------------------------------------
/**
 * @brief Lädt die Filament-Datenbank in den Speicher
 * @return true bei Erfolg, false bei Fehler
 */
// --------------------------------------------------------------------------
bool loadFilaments();

// --------------------------------------------------------------------------
/**
 * @brief Aktualisiert die Konfiguration anhand eines JSON-Dokuments
 * @param doc  JSON-Dokument mit Konfigurationswerten
 * @return true bei Erfolg, false bei Fehler
 */
// --------------------------------------------------------------------------
bool updateConfigFromJson(ArduinoJson::V742PB22::JsonDocument& doc);

// --------------------------------------------------------------------------
/**
 * @brief Lädt die Konfiguration als JSON-Objekt in ein bestehendes Dokument
 * @param target  Ziel-JsonObject
 * @return true bei Erfolg, false bei Fehler
 */
// --------------------------------------------------------------------------
bool loadConfigAsJson(JsonObject target);

// --------------------------------------------------------------------------
/**
 * @brief Lädt die Filament-Datenbank als JsonArray
 * @param target  Ziel-JsonArray
 * @return true bei Erfolg, false bei Fehler
 */
// --------------------------------------------------------------------------
bool loadFilamentsAsJson(JsonArray target);

// --------------------------------------------------------------------------
/**
 * @brief Lädt die Konfiguration als String (für Download)
 * @param out  Zielstring
 * @return true bei Erfolg, false bei Fehler
 */
// --------------------------------------------------------------------------
bool loadConfigAsString(String& out);

// --------------------------------------------------------------------------
/**
 * @brief Importiert eine Konfiguration aus einem JSON-Objekt
 * @param src  Quell-JsonObject
 * @return true bei Erfolg, false bei Fehler
 */
// --------------------------------------------------------------------------
bool importConfigJson(JsonObject src);

// --------------------------------------------------------------------------
/**
 * @brief Importiert Filamente aus einem JSON-Objekt
 * @param src  Quell-JsonObject
 * @return true bei Erfolg, false bei Fehler
 */
// --------------------------------------------------------------------------
bool importFilamentsJson(JsonArray src);


// --------------------------------------------------------------------------
/**
 * @brief Lädt die Filament-Datenbank in ein externes Array
 * @param dst         Ziel-Array
 * @param maxEntries  Maximale Anzahl Einträge
 * @param outCount    Anzahl tatsächlich geladener Einträge
 * @return true bei Erfolg, false bei Fehler
 */
// --------------------------------------------------------------------------
bool loadFilamentDB(FilamentEntry *dst, size_t maxEntries, size_t &outCount);

// --------------------------------------------------------------------------
/**
 * @brief Speichert die aktuelle Filament-Datenbank in die Datei filaments.json
 * @return true bei Erfolg, false bei Fehler
 */
// --------------------------------------------------------------------------
bool saveFilamentsToFile();

// --------------------------------------------------------------------------
/**
 * @brief Wandelt die farbwerte in einem JsonArray in einen 0xRRGGBB-Wert um und setzt die Referenz
 * @param opt   JsonObject mit den Optionen
 * @param key   Schlüssel des Farb-Arrays
 * @param color Referenz auf den 0xRRGGBB-Farbwert
 */
// --------------------------------------------------------------------------
void setColorArray(JsonObject &opt, const char* key, uint32_t color);
