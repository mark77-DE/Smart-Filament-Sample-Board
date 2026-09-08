#include "filehandling.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include "globals.h"
#include "ledctrl_filament.h"
#include "ledctrl_nfc.h"
#include "filament_db.h"
#include "gpio_hardware.h"   // for gpiohw_init()
#include "config.h"        // for CONFIGV2


// ============================================================================
// Filament-DB
// ============================================================================

bool loadFilaments() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  if (!loadFilamentsAsJson(arr)) return false;

  bool changed = migrateFilamentArray(arr, CONFIGV2.system.version);  // NEU

  bool ok = FilamentDB::loadFromJsonArray(arr);

  if (ok && changed) {
    saveFilamentsToFile();  // nur schreiben, wenn wirklich migriert wurde
  }

  return ok;
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

  migrateFilamentArray(src, CONFIGV2.system.version);  // NEU – genau hier, vor FilamentDB::loadFromJsonArray

  // 1) Load into the database (internally replaces the existing database)
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


// ============================================================================
// Utilities
// ============================================================================

void setColorArray(JsonObject& opt, const char* key, uint32_t color) {
  JsonArray arr = opt[key].to<JsonArray>();
  arr.add((color >> 16) & 0xFF);
  arr.add((color >>  8) & 0xFF);
  arr.add((color      ) & 0xFF);
}
