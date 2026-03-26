#include "i18n.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

StaticJsonDocument<16384> langDoc;

String I18N::_currentLang = "de";

void I18N::begin(const String& lang) {
    _currentLang = lang;
    if (lang == "de") {
        loadLanguage("/lang_de.json");
    } else {
        loadLanguage("/lang_en.json");
    }
}

bool I18N::loadLanguage(const char* path) {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS konnte nicht gemountet werden!");
        return false;
    }

    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.printf("Sprachdatei %s nicht gefunden!\n", path);
        return false;
    }

    DeserializationError err = deserializeJson(langDoc, file);
    file.close();

    if (err) {
        Serial.printf("Fehler beim Parsen der Sprachdatei %s: %s\n", path, err.c_str());
        return false;
    }

    Serial.printf("Sprache %s geladen.\n", path);
    return true;
}

const char* I18N::get(const char* key) {
    JsonVariant val = langDoc["i18n"][key];
    if (val.is<const char*>()) {
        return val.as<const char*>();
    }
    return key; // fallback: Key zurückgeben
}

const char* I18N::getNested(const char* path) {
    char buf[128];
    strncpy(buf, path, sizeof(buf));
    buf[sizeof(buf)-1] = 0;

    JsonVariant current = langDoc;

    char* token = strtok(buf, ".");
    while (token != nullptr) {
        if (!current.is<JsonObject>() || !current[token].is<JsonVariant>()) {
            return path; // fallback
        }
        current = current[token];
        token = strtok(nullptr, ".");
    }

    if (current.is<const char*>()) return current.as<const char*>();
    return path;
}

const String& I18N::currentLanguage() {
    return _currentLang;
}