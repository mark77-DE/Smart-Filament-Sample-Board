#include "filehandling.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include "globals.h"
#include "ledctrl_filament.h"
#include "ledctrl_nfc.h"
#include "filament_db.h"
#include "gpio_hardware.h"   // für gpiohw_init()
#include "config.h"        // für CONFIGV2





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
    if (CONFIGV2.system.debugMode) {
      Serial.println(F("saveFilamentsToFile: Cannot open file for write!"));
    }
    return false;
  }

  size_t written = serializeJson(doc, f);
  f.close();

  if (CONFIGV2.system.debugMode) {
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
    if (CONFIGV2.system.debugMode) Serial.println(F("importFilamentsJson: src is null"));
    return false;
  }
  if (src.size() == 0) {
    if (CONFIGV2.system.debugMode) Serial.println(F("importFilamentsJson: empty array"));
    return false;
  }

  if (CONFIGV2.system.debugMode) {
    Serial.printf("Importing %u filaments...\n", src.size());
  }

  // 1) In DB laden (überschreibt intern die bestehende DB)
  if (!FilamentDB::loadFromJsonArray(src)) {
    if (CONFIGV2.system.debugMode) {
      Serial.println(F("importFilamentsJson: FilamentDB loadFromJsonArray failed"));
    }
    return false;
  }

  // 2) Persistieren
  if (!saveFilamentsToFile()) {
    if (CONFIGV2.system.debugMode) {
      Serial.println(F("importFilamentsJson: saving filaments failed"));
    }
    return false;
  }

  if (CONFIGV2.system.debugMode) {
    Serial.printf("Filaments imported successfully. Count=%d\n",
                  FilamentDB::getAllCount());
  }
  g_reloadFilamentsPending = true;
  return true;
}

// // Optional-Hilfsfunktion (falls in filehandling.h deklariert)
// bool loadFilamentDB(FilamentEntry* dst, size_t maxEntries, size_t& outCount) {
//   outCount = 0;

//   // Hole DB als JSON-Array (verlustfrei aus dem Namespace)
//   JsonDocument doc;
//   JsonArray arr = FilamentDB::toJsonArray(doc);

//   for (JsonObject obj : arr) {
//     if (outCount >= maxEntries) break;

//     FilamentEntry e{};
//     // Felder gemäß bisheriger Verwendung in handleUID()
//     e.ledIndex  = obj["ledIndex"] | -1;
//     e.vendor    = obj["vendor"]   | String();
//     e.type      = obj["type"]     | String();
//     e.color     = obj["color"]    | String();
//     e.info1     = obj["info1"]    | String();
//     e.info2     = obj["info2"]    | String();
//     // ggf. weitere Felder analog ergänzen

//     dst[outCount++] = e;
//   }
//   return (outCount > 0);
// }

// ============================================================================
// Utilities
// ============================================================================

void setColorArray(JsonObject& opt, const char* key, uint32_t color) {
  JsonArray arr = opt[key].to<JsonArray>();
  arr.add((color >> 16) & 0xFF);
  arr.add((color >>  8) & 0xFF);
  arr.add((color      ) & 0xFF);
}
