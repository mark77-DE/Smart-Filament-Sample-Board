#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>


// ============================================================================
// Konfigurationsstrukturen
// ============================================================================




/**
 * @brief LED-Konfiguration (Filament-Stripe)
 */
struct LedConfigV2 {
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
struct NfcLedConfigV2 {
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
struct ButtonConfigV2 {
  bool enabled       = true; ///< Button aktiv?
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
struct BuzzerConfigV2 {
  bool enabled       = true; ///< Buzzer aktiv?
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
  String haDiscoveryPrefix;  ///< Präfix für HA Discovery (z. B. "homeassistant")
};

// OPTIONAL: Kennzeichen, damit gpio_hardware.cpp weiß,
// dass Button/Buzzer in CONFIG vorhanden sind:
#define CONFIG_HAS_GPIO

struct systemConfig {
  String        version = "error";
  bool          darkmode;    ///< Darkmode aktiv
  bool          debugMode;   ///< Debug-Modus aktiv
  uint32_t      webLEDTimeout;   // Default fürs Dashboard (ms)
  String        hostname;    ///< Hostname für WLAN
  bool          animationAfterBoot; ///< Startup-Animation nach Booten aktiv
  String        defaultLanguage; ///< Standard-Sprache (z. B. "en" oder "de")
};

/**
 * @brief Haupt-Konfigurationsstruktur der App
 */
struct AppConfigV2 {
  systemConfig system;     ///< System-Konfiguration
  uint32_t     webLEDTimeout;   // Default fürs Dashboard (ms)
  String       hostname;    ///< Hostname für WLAN

  LedConfigV2    led;         ///< LED-Konfiguration (Filament)
  NfcLedConfigV2 nfc;         ///< NFC-LED-Konfiguration
  ButtonConfigV2 button;      ///< Pushbutton-Konfiguration
  BuzzerConfigV2 buzzer;      ///< Buzzer-Konfiguration
  MqttConfigV2   mqttConfig;  ///< MQTT-Konfiguration
};

// Globale, aktuell geladene Konfiguration
extern AppConfigV2 CONFIGV2;

// ============================================================================
// High-Level API
// ============================================================================

/**
 * @brief Lädt die Konfiguration aus config_v2.json in CONFIGV2
 * @return true bei Erfolg, sonst false
 */
bool loadConfigV2();

/**
 * @brief Wendet die aktuelle CONFIG auf die Hardware/Module an
 */
void applyConfigV2();

/**
 * @brief Speichert CONFIGV2 in /config_v2.json
 * @return true bei Erfolg, sonst false
 */
bool saveConfigV2();



// ============================================================================
// JSON Hilfs-API
// ============================================================================

/**
 * @brief Aktualisiert CONFIG anhand eines JSON-Dokuments (doc["options"]…)
 * @param doc JSON-Dokument mit Konfigurationswerten
 * @return true bei Erfolg, sonst false
 */
bool updateConfigFromJsonV2(JsonDocument& doc);

/**
 * @brief Lädt die Konfiguration als JSON-Objekt in ein bestehendes Dokument
 * @param target Ziel-JsonObject (wird befüllt)
 * @return true bei Erfolg, sonst false
 */
bool loadConfigAsJsonV2(JsonObject target);




/**
 * @brief Importiert eine Konfiguration aus einem JSON-Objekt (schreibt Datei)
 * @param src Quell-JsonObject
 * @return true bei Erfolg, sonst false
 */
bool importConfigJsonV2(JsonObject src);


// ============================================================================
// Sonstiges
// ============================================================================



/**
 * @brief Schreibt eine 0xRRGGBB-Farbe als [r,g,b]-Array in ein JsonObject
 * @param opt Ziel-JsonObject (z. B. "options")
 * @param key Schlüssel, unter dem das Array erzeugt wird
 * @param color 0xRRGGBB
 */

uint32_t colorFromArrayV2(JsonArrayConst arr);

// void setColorArrayV2(JsonObject& opt, const char* key, uint32_t color);
