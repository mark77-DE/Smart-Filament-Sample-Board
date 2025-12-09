#include "filament_db.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

static FilamentEntry db[100];   // max 50 Tags
static int dbCount = 0;

namespace FilamentDB {

void getAll(std::vector<FilamentEntry> &list) {
    list.clear();
    for (int i = 0; i < dbCount; i++) {
        list.push_back(db[i]);
    }
}

bool loadFromFile() {
    if (!LittleFS.exists("/filaments.json")) {
        Serial.println("No DB file, using defaults.");
        return false;
    }

    File f = LittleFS.open("/filaments.json", "r");
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.println("JSON parse failed");
        return false;
    }

    dbCount = 0;
    for (JsonObject o : doc.as<JsonArray>()) {
        db[dbCount].uid = o["uid"].as<String>();
        db[dbCount].vendor = o["vendor"].as<String>();
        db[dbCount].type = o["type"].as<String>();
        db[dbCount].color = o["color"].as<String>();
        db[dbCount].ledIndex = o["ledIndex"].as<int>();
        dbCount++;
    }

    Serial.printf("DB loaded: %d entries\n", dbCount);
    return true;
}

bool saveToFile() {
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 0; i < dbCount; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["uid"] = db[i].uid;
        o["vendor"] = db[i].vendor;
        o["type"] = db[i].type;
        o["color"] = db[i].color;
        o["ledIndex"] = db[i].ledIndex;
    }

    File f = LittleFS.open("/filaments.json", "w");
    if (!f) {
        Serial.println("saveToFile: Cannot open file for write!");
        return false;
    }

    size_t written = serializeJson(doc, f);
    f.close();
    Serial.printf("DB saved. bytes=%u entries=%d\n", (unsigned)written, dbCount);
    return true;
}


void load() {
    if (!LittleFS.exists("/filaments.json")) {
        Serial.println("DB file not found. Start with empty DB.");
        dbCount = 0;   // keine Voreinträge
    } else {
        if (!loadFromFile()) {
            Serial.println("Error loading DB from file!");
            dbCount = 0; // leere DB bei Fehler
        }
    }
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

// Update bestehender Eintrag nach UID
bool update(const FilamentEntry &entry) {
    for(int i = 0; i < dbCount; i++) {
        if(db[i].uid == entry.uid) {
            db[i] = entry;
            return true;
        }
    }
    return false;
}

// Eintrag hinzufügen
bool add(const FilamentEntry &entry) {
    if(dbCount >= 50) return false;
    db[dbCount++] = entry;
    saveToFile();
    return true;
}

// Eintrag löschen nach UID
bool remove(const String &uid) {
    for(int i = 0; i < dbCount; i++) {
        if(db[i].uid == uid) {
            // alle folgenden Einträge nach vorne verschieben
            for(int j = i; j < dbCount-1; j++) {
                db[j] = db[j+1];
            }
            dbCount--;
            return true;
        }
    }
    return false;
}

bool deleteEntry(int index) {
    if (index < 0 || index >= dbCount) {
        return false;
    }

    // Eintrag entfernen indem alles nach vorne geschoben wird
    for (int i = index; i < dbCount - 1; i++) {
        db[i] = db[i + 1];
    }

    dbCount--;

    // Neue DB speichern
    return saveToFile();
}

} // namespace
