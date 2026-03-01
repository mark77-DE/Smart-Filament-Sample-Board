#include "globals.h"
#include "filament_db.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "filehandling.h"
#include "config.h"
#include "mqtt_manager.h"

static FilamentEntry db[150];   // max 100 Einträge
static int dbCount = 0;
static const int MAX_DB_ENTRIES = 150;

namespace FilamentDB {

// ----------------- Basisfunktionen -----------------
void getAll(std::vector<FilamentEntry> &list) {
    list.clear();
    for (int i = 0; i < dbCount; i++) {
        list.push_back(db[i]);
    }
}

int getAllCount() {
    return dbCount;
}


bool findByUID(const String &uid, FilamentEntry &entry) {
    for (int i = 0; i < dbCount; i++) {
        if (db[i].uid == uid) {
            entry = db[i];
            return true;
        }
    }
    return false;
}

// ----------------- Neue Funktionen -----------------

bool add(const FilamentEntry &entry) {
    if (dbCount >= 100) return false;
    db[dbCount++] = entry;
    return true;  // nur Erfolg im Speicher
}

bool update(const FilamentEntry &entry) {
    for (int i = 0; i < dbCount; i++) {
        if (db[i].uid == entry.uid) {
            db[i] = entry;
            return true; // nur Speicher
        }
    }
    return false;
}

bool remove(const String &uid) {
    for (int i = 0; i < dbCount; i++) {
        if (db[i].uid == uid) {
            for (int j = i; j < dbCount - 1; j++) {
                db[j] = db[j + 1];
            }
            dbCount--;
            return true; // nur Speicher
        }
    }
    return false;
}

bool updateAtIndex(int idx, const FilamentEntry &entry) {
    if (idx < 0 || idx >= dbCount) return false;
    db[idx] = entry;
    return true; // nur Speicher
}

bool loadFromJsonArray(JsonArray arr) {
    dbCount = 0;

    for (JsonObject o : arr) {
        if (dbCount >= MAX_DB_ENTRIES) break;

        db[dbCount].uid         = o["uid"]      | "";
        db[dbCount].vendor      = o["vendor"]   | "";
        db[dbCount].type        = o["type"]     | "";
        db[dbCount].color       = o["color"]    | "";
        db[dbCount].ledIndex    = o["ledIndex"] | -1;
        db[dbCount].info1       = o["info1"]    | "";
        db[dbCount].info2       = o["info2"]    | "";
        db[dbCount].storage     = o["storage"]  | "";

        dbCount++;
    }

    if (CONFIGV2.system.debugMode) {
        Serial.printf("Filament DB loaded: %d entries\n", dbCount);
    }

    return dbCount > 0;
}



JsonArray toJsonArray(JsonDocument &doc) {
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 0; i < dbCount; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["uid"]        = db[i].uid;
        o["vendor"]     = db[i].vendor;
        o["type"]       = db[i].type;
        o["color"]      = db[i].color;
        o["ledIndex"]   = db[i].ledIndex;
        o["info1"]      = db[i].info1;
        o["info2"]      = db[i].info2;
        o["storage"]    = db[i].storage;
    }

    return arr;
}






} // namespace FilamentDB

