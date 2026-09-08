#pragma once
#include <Arduino.h>

class I18N {
public:
    /**
    * Initializes the language and loads the JSON file
     * Muss nach loadConfig() aufgerufen werden
     */
    static void begin(const String& lang);

    /**
    * Returns a string for the given key
     * Beispiel: I18N::get("txt_unknown")
     */
    static const char* get(const char* key);

    /**
     * Liefert einen String anhand eines verschachtelten Keys
     * Beispiel: I18N::getNested("help.led.title")
     */
    static const char* getNested(const char* path);

    /**
    * Returns the currently loaded language
     */
    static const String& currentLanguage();

private:
    /**
    * Internal function: loads the language file
     */
    static bool loadLanguage(const char* path);

    static String _currentLang;
};