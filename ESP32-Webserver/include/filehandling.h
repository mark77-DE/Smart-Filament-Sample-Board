#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "filament_db.h"



// ============================================================================
// High-Level API
// ============================================================================



/**
 * @brief Loads the filament database into memory (FilamentDB)
 * @return true on success, otherwise false
 */
bool loadFilaments();

// ============================================================================
// JSON helper API
// ============================================================================



/**
 * @brief Loads the filament database as a JsonArray into an existing document
 * @param target Target JsonArray (will be populated)
 * @return true on success, otherwise false
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
 * @brief Loads the filament database into an external array
 * @param dst Target array
 * @param maxEntries Maximum number of entries
 * @param outCount Number of entries actually loaded (by ref)
 * @return true on success, otherwise false
 * @note Declaration only; implementation may be elsewhere (depending on the FilamentDB API).
 */
// bool loadFilamentDB(FilamentEntry* dst, size_t maxEntries, size_t& outCount);

/**
 * @brief Saves the current filament database to /filaments.json
 * @return true on success, otherwise false
 */
bool saveFilamentsToFile();

/**
 * @brief Writes a 0xRRGGBB color as an [r,g,b] array into a JsonObject
 * @param opt Target JsonObject (e.g. "options")
 * @param key Key under which the array is created
 * @param color 0xRRGGBB
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