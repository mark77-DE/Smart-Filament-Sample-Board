#pragma once
#include <Arduino.h>

class I18N {
public:
    /**
     * Initialisiert die Sprache und lädt die JSON-Datei
     * Muss nach loadConfig() aufgerufen werden
     */
    static void begin(const String& lang);

    /**
     * Liefert einen String anhand des Keys zurück
     * Beispiel: I18N::get("txt_unknown")
     */
    static const char* get(const char* key);

    /**
     * Liefert einen String anhand eines verschachtelten Keys
     * Beispiel: I18N::getNested("help.led.title")
     */
    static const char* getNested(const char* path);

    /**
     * Gibt aktuell geladene Sprache zurück
     */
    static const String& currentLanguage();

private:
    /**
     * Interne Funktion: lädt die Sprachdatei
     */
    static bool loadLanguage(const char* path);

    static String _currentLang;
};