#include "i18n.h"
#include <ArduinoJson.h>
#include "config.h"
#include "web_assets_generated.h"

JsonDocument langDoc;

String I18N::_currentLang = "de";

void I18N::begin(const String &lang)
{
    _currentLang = lang;
    if (lang == "de")
    {
        loadLanguage("/lang_de.json");
    }
    else
    {
        loadLanguage("/lang_en.json");
    }
}

bool I18N::loadLanguage(const char *path)
{
    const uint8_t *data = nullptr;
    size_t length = 0;

    if (strcmp(path, "/lang_de.json") == 0)
    {
        data = asset_lang_de_json;
        length = asset_lang_de_json_len;
    }
    else if (strcmp(path, "/lang_en.json") == 0)
    {
        data = asset_lang_en_json;
        length = asset_lang_en_json_len;
    }
    else
    {
        Serial.printf("Unbekannte Sprachdatei %s\n", path);
        return false;
    }

    langDoc.clear(); // clear old content before loading new language

    DeserializationError err = deserializeJson(langDoc, data, length);

    if (err)
    {
        Serial.printf("Fehler beim Parsen der Sprachdatei %s: %s\n", path, err.c_str());
        return false;
    }

    if (CONFIGV2.system.debugMode)
    {
        Serial.printf("[%s] %s %s.\n", get("txt_language"), path, get("txt_loaded"));
    }
    return true;
}

const char *I18N::get(const char *key)
{
    JsonVariant val = langDoc["i18n"][key];

    if (val.is<const char *>())
    {
        return val.as<const char *>();
    }
    return key; // fallback: return key
}

const char *I18N::getNested(const char *path)
{
    char buf[128];
    strncpy(buf, path, sizeof(buf));
    buf[sizeof(buf) - 1] = 0;

    JsonVariant current = langDoc;

    char *token = strtok(buf, ".");
    while (token != nullptr)
    {
        if (!current.is<JsonObject>() || !current[token].is<JsonVariant>())
        {
            return path; // fallback
        }
        current = current[token];
        token = strtok(nullptr, ".");
    }

    if (current.is<const char *>())
        return current.as<const char *>();
    return path;
}

const String &I18N::currentLanguage()
{
    return _currentLang;
}