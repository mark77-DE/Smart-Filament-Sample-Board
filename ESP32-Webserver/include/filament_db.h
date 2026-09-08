#pragma once
#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>

/**
 * @brief A single filament entry consisting of:
 * - LED-Index
 * - UID
 * - Vendor
 * - Type
 * - Color
 */
struct FilamentEntry {
    uint16_t ledIndex;   ///< LED index (0..n), uint16_t prevents overflow
    String uid;          ///< Unique filament ID
    String vendor;       ///< Vendor
    String type;         ///< Filament type (e.g. PLA, PETG)
    String color;        ///< Color (string)
    String info1;         ///< Info text
    String info2;         ///< Link to the vendor or similar
    String storage;
};

namespace FilamentDB {

    // --------------------------------------------------------------------------
    /**
    * @brief Retrieves all database entries
    * @param list  Target vector populated with all filament entries
     */
    // --------------------------------------------------------------------------
    void getAll(std::vector<FilamentEntry> &list);

    // --------------------------------------------------------------------------
    /**
    * @brief Finds a filament entry by its UID
    * @param uid    UID of the filament to find
    * @param entry  Reference to the found entry
    * @return true when the filament was found; false otherwise
     */
    // --------------------------------------------------------------------------
    bool findByUID(const String &uid, FilamentEntry &entry);

    // --------------------------------------------------------------------------
    /**
    * @brief Adds a new filament entry
    * @param entry  Entry to add
    * @return true on success, false when the maximum count is reached
     */
    // --------------------------------------------------------------------------
    bool add(const FilamentEntry &entry);

    // --------------------------------------------------------------------------
    /**
    * @brief Updates an existing entry by UID
    * @param entry  Entry with the UID to replace
    * @return true when updated successfully, false when the UID was not found
     */
    // --------------------------------------------------------------------------
    bool update(const FilamentEntry &entry);

    // --------------------------------------------------------------------------
    /**
    * @brief Deletes an entry by UID
    * @param uid  UID of the entry to delete
    * @return true when deleted successfully, false when the UID was not found
     */
    // --------------------------------------------------------------------------
    bool remove(const String &uid);

    // --------------------------------------------------------------------------
    /**
    * @brief Creates the entire database as a JsonArray
    * @param doc  JsonDocument into which the array is inserted
    * @return JsonArray containing all filament entries
     */
    // --------------------------------------------------------------------------
    JsonArray toJsonArray(JsonDocument &doc);

    // --------------------------------------------------------------------------
    /**
    * @brief Updates an entry by its database index
    * @param idx    Entry index
    * @param entry  New entry
    * @return true on success, false for an invalid index
     */
    // --------------------------------------------------------------------------
    bool updateAtIndex(int idx, const FilamentEntry &entry);

    // --------------------------------------------------------------------------
    /**
    * @brief Loads the database from a JsonArray
    * @param arr  JsonArray containing the filament entries
    * @return true when at least one entry was loaded, false otherwise
     */
    // --------------------------------------------------------------------------
    bool loadFromJsonArray(JsonArray arr);

    // --------------------------------------------------------------------------
    /**
    * @brief Retrieves the number of database entries
    * @return Number of stored filament entries
     */
    // --------------------------------------------------------------------------
    int getAllCount();

} // namespace FilamentDB
