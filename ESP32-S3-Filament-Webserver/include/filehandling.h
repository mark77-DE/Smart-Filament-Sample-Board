#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "filament_db.h"



// ============================================================================
// High-Level API
// ============================================================================



/**
 * @brief Lädt die Filament-Datenbank in den Speicher (FilamentDB)
 * @return true bei Erfolg, sonst false
 */
bool loadFilaments();

// ============================================================================
// JSON Hilfs-API
// ============================================================================



/**
 * @brief Lädt die Filament-Datenbank als JsonArray in ein bestehendes Dokument
 * @param target Ziel-JsonArray (wird befüllt)
 * @return true bei Erfolg, sonst false
 */
bool loadFilamentsAsJson(JsonArray target);




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
