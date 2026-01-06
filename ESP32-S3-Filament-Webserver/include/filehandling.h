#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "filament_db.h"

// ============================================================================
// Konfigurationsstrukturen
// ============================================================================




/**
 * @brief LED-Konfiguration (Filament-Stripe)
 */
struct LedConfig {
  int      count;       ///< Anzahl LEDs
  int      pin;         ///< GPIO-Pin
  int      brightness;  ///< Helligkeit [0..255]
  int      timeout;     ///< Timeout in ms
  uint32_t color;       ///< Standardfarbe 0xRRGGBB
  uint32_t colorError;  ///< Fehlerfarbe 0xRRGGBB
  uint32_t colorPulse;  ///< Idle-Pulse-Farbe 0xRRGGBB
};

/**
 * @brief NFC-LED-Konfiguration
 */
struct NfcLedConfig {
  int      count;                 ///< Anzahl LEDs
  int      pin;                   ///< GPIO-Pin
  int      brightness;            ///< Helligkeit [0..255]
  int      timeout;               ///< Timeout in ms
  uint32_t colorSuccess;          ///< Farbe für Erfolg 0xRRGGBB
  uint32_t colorError;            ///< Farbe für Fehler 0xRRGGBB
  uint32_t colorPulse;            ///< Idle-Pulse-Farbe 0xRRGGBB
  bool     successBlinkEnabled;   ///< True = Erfolgsblinken aktiv
  int      successBlinkCount;     ///< Anzahl Blink-Zyklen
  int      successBlinkMs;        ///< Blink-Intervall in ms
};

/**
 * @brief Button-Konfiguration
 */
struct ButtonConfig {
  int  pin           = -1;   ///< GPIO des Buttons (-1 = aus)
  bool pullup        = true; ///< interner PullUp -> active-low
  int  debounceMs    = 30;   ///< Entprellzeit
  int  longMs        = 800;  ///< Schwelle Long-Press
  int  doubleGapMs   = 400;  ///< Fenster für Double-Press
  int  holdRepeatMs  = 250;  ///< Wiederholrate bei Hold
};

/**
 * @brief Buzzer-Konfiguration
 */
struct BuzzerConfig {
  int  pin           = -1;    ///< GPIO des Buzzers (-1 = aus)
  bool activeHigh    = true;  ///< Aktivpegel HIGH?
  bool passive       = false; ///< false = aktiver Buzzer, true = passiver (PWM)
  int  freqHz        = 4000;  ///< Frequenz für tone()
  int  singleMs      = 80;    ///< Dauer Single-Beep
  int  doubleOnMs    = 60;    ///< Ein-Zeit Double-Beep
  int  doubleGapMs   = 80;    ///< Pause Double-Beep
  int  errorOnMs     = 50;    ///< Ein-Zeit Error-Sequenz
  int  errorGapMs    = 60;    ///< Pause Error-Sequenz
  int  errorCount    = 3;     ///< Wiederholungen Error-Sequenz
};

// OPTIONAL: Kennzeichen, damit gpio_hardware.cpp weiß,
// dass Button/Buzzer in CONFIG vorhanden sind:
#define CONFIG_HAS_GPIO

/**
 * @brief Haupt-Konfigurationsstruktur der App
 */
struct AppConfig {
  bool         darkmode;    ///< Darkmode aktiv
  bool         mqtt;        ///< MQTT aktiviert
  bool         debugMode;   ///< Debug-Modus aktiv
  uint32_t     webLEDTimeout;   // Default fürs Dashboard (ms)
  String       hostname;    ///< Hostname für WLAN

  LedConfig    led;         ///< LED-Konfiguration (Filament)
  NfcLedConfig nfc;         ///< NFC-LED-Konfiguration
  ButtonConfig button;      ///< Pushbutton-Konfiguration
  BuzzerConfig buzzer;      ///< Buzzer-Konfiguration
};

// Globale, aktuell geladene Konfiguration
extern AppConfig CONFIG;

// ============================================================================
// High-Level API
// ============================================================================

/**
 * @brief Lädt die Konfiguration aus /config.json in CONFIG
 * @return true bei Erfolg, sonst false
 */
bool loadConfig();

/**
 * @brief Wendet die aktuelle CONFIG auf die Hardware/Module an
 */
void applyConfig();

/**
 * @brief Speichert CONFIG in /config.json
 * @return true bei Erfolg, sonst false
 */
bool saveConfig();

/**
 * @brief Lädt die Filament-Datenbank in den Speicher (FilamentDB)
 * @return true bei Erfolg, sonst false
 */
bool loadFilaments();

// ============================================================================
// JSON Hilfs-API
// ============================================================================

/**
 * @brief Aktualisiert CONFIG anhand eines JSON-Dokuments (doc["options"]…)
 * @param doc JSON-Dokument mit Konfigurationswerten
 * @return true bei Erfolg, sonst false
 */
bool updateConfigFromJson(JsonDocument& doc);

/**
 * @brief Lädt die Konfiguration als JSON-Objekt in ein bestehendes Dokument
 * @param target Ziel-JsonObject (wird befüllt)
 * @return true bei Erfolg, sonst false
 */
bool loadConfigAsJson(JsonObject target);

/**
 * @brief Lädt die Filament-Datenbank als JsonArray in ein bestehendes Dokument
 * @param target Ziel-JsonArray (wird befüllt)
 * @return true bei Erfolg, sonst false
 */
bool loadFilamentsAsJson(JsonArray target);

/**
 * @brief Lädt die Konfiguration als String (z. B. für Download)
 * @param out Rückgabe-String
 * @return true bei Erfolg, sonst false
 */
bool loadConfigAsString(String& out);

/**
 * @brief Importiert eine Konfiguration aus einem JSON-Objekt (schreibt Datei)
 * @param src Quell-JsonObject
 * @return true bei Erfolg, sonst false
 */
bool importConfigJson(JsonObject src);

/**
 * @brief Importiert Filamente aus einem JSON-Array (DB + Datei)
 * @param src Quell-JsonArray
 * @return true bei Erfolg, sonst false
 */
bool importFilamentsJson(JsonArray src);

// ============================================================================
// Sonstiges
// ============================================================================

/**
 * @brief Lädt die Filament-DB in ein externes Array
 * @param dst Ziel-Array
 * @param maxEntries maximale Anzahl Einträge
 * @param outCount Anzahl tatsächlich geladener Einträge (by ref)
 * @return true bei Erfolg, sonst false
 * @note Nur deklariert – Implementierung ggf. an anderer Stelle (abhängig von FilamentDB-API).
 */
bool loadFilamentDB(FilamentEntry* dst, size_t maxEntries, size_t& outCount);

/**
 * @brief Speichert die aktuelle Filament-DB in /filaments.json
 * @return true bei Erfolg, sonst false
 */
bool saveFilamentsToFile();

/**
 * @brief Schreibt eine 0xRRGGBB-Farbe als [r,g,b]-Array in ein JsonObject
 * @param opt Ziel-JsonObject (z. B. "options")
 * @param key Schlüssel, unter dem das Array erzeugt wird
 * @param color 0xRRGGBB
 */
void setColorArray(JsonObject& opt, const char* key, uint32_t color);
