#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "filament_db.h"



// ============================================================================
// High-Level API
// ============================================================================



/**
 * @brief Loads the filament database from the configured file into memory.
 * @return true on success, otherwise false.
 */
bool loadFilaments();

/**
 * @brief Exports the filament database as a JSON array.
 * @param target JsonArray to populate.
 * @return true on success, otherwise false.
 */
bool loadFilamentsAsJson(JsonArray target);




/**
 * @brief Imports filaments from a JSON array (database + file)
 * @param src Source JsonArray
 * @return true on success, otherwise false
 */
bool importFilamentsJson(JsonArray src);

// ============================================================================
// Miscellaneous
// ============================================================================



/**
 * @brief Saves the current filament database to /filaments.json
 * @return true on success, otherwise false
 */
bool saveFilamentsToFile();

/**
 * @brief Writes a packed RGB color into a JSON object as an RGB array.
 * @param opt JSON object to update.
 * @param key Field name to write under.
 * @param color 24-bit RGB color value in 0xRRGGBB format.
 */
void setColorArray(JsonObject& opt, const char* key, uint32_t color);


// filehandling.h

/**
 * @brief Migrates a single filament entry to the current schema
 * @param entry Entry to migrate
 * @param configVersion Current CONFIGV2.system.version, controls which migration steps apply
 * @return true when something was changed
 */
inline bool migrateFilamentEntry(JsonObject entry, const String& configVersion)
{
    bool changed = false;

    // Example migration step (currently unnecessary because "storage" already exists
    // and is covered by the | default; kept as a template for future fields)
    // if (configVersion == "1.0" || configVersion == "2.0") {
    //     if (entry["someNewField"].isNull()) {
    //         entry["someNewField"] = <sinnvoller Default>;
    //         changed = true;
    //     }
    // }

    return changed;
}

/**
 * @brief Resets the filament database to the factory default.
 * @return true on success, otherwise false
 */
bool resetFilamentsToDefaults();

/**
 * @brief Migrates all entries in a filament array
 * @return true when at least one entry was changed
 */
inline bool migrateFilamentArray(JsonArray arr, const String& configVersion)
{
    bool anyChanged = false;
    for (JsonObject entry : arr) {
        if (migrateFilamentEntry(entry, configVersion)) anyChanged = true;
    }
    return anyChanged;
}