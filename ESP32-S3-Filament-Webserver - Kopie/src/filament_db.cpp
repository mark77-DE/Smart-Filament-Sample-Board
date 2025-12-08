#include "filament_db.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

static FilamentEntry db[50];   // max 50 Tags
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
    JsonDocument doc;
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
    if (!f) return false;

    serializeJson(doc, f);
    f.close();
    Serial.println("DB saved.");
    return true;
}

void load() {
    if (!loadFromFile()) {
        Serial.println("Using fallback built-in DB.");

        dbCount = 3;

        db[0] = {0, "04:D3:4F:51:6F:61:81", "HerstellerA", "PLA", "Rot"};
        db[1] = {3, "04:21:87:51:6F:61:80", "HerstellerB", "PETG", "Blau"};
        db[2] = {7, "04:92:89:51:6F:61:80", "HerstellerC", "ABS", "Grün"};

        saveToFile();
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

} // namespace
