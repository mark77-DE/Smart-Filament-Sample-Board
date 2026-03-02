#pragma once
#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>

/**
 * @brief Ein einzelner Filament-Eintrag bestehend aus:
 * - LED-Index
 * - UID
 * - Hersteller
 * - Typ
 * - Farbe
 */
struct FilamentEntry {
    uint16_t ledIndex;   ///< LED-Index (0..n), uint16_t um Überlauf zu vermeiden
    String uid;          ///< Eindeutige ID des Filaments
    String vendor;       ///< Hersteller
    String type;         ///< Filament-Typ (z.B. PLA, PETG)
    String color;        ///< Farbe (String)
    String info1;         ///< Info Text
    String info2;         ///< link zum Hersteller oder ähnlichen
    String storage;
};

namespace FilamentDB {

    // --------------------------------------------------------------------------
    /**
     * @brief Alle Einträge der Datenbank abrufen
     * @param list  Ziel-Vektor, der mit allen Filament-Einträgen gefüllt wird
     */
    // --------------------------------------------------------------------------
    void getAll(std::vector<FilamentEntry> &list);

    // --------------------------------------------------------------------------
    /**
     * @brief Ein Filament-Eintrag anhand seiner UID suchen
     * @param uid    UID des gesuchten Filaments
     * @param entry  Referenz zum Eintrag, der gefunden wird
     * @return true, wenn das Filament gefunden wurde; false sonst
     */
    // --------------------------------------------------------------------------
    bool findByUID(const String &uid, FilamentEntry &entry);

    // --------------------------------------------------------------------------
    /**
     * @brief Einen neuen Filament-Eintrag hinzufügen
     * @param entry  Eintrag, der hinzugefügt werden soll
     * @return true bei Erfolg, false, wenn Maximalanzahl erreicht ist
     */
    // --------------------------------------------------------------------------
    bool add(const FilamentEntry &entry);

    // --------------------------------------------------------------------------
    /**
     * @brief Einen bestehenden Eintrag anhand der UID aktualisieren
     * @param entry  Eintrag mit UID, die ersetzt werden soll
     * @return true, wenn erfolgreich aktualisiert, false wenn UID nicht gefunden
     */
    // --------------------------------------------------------------------------
    bool update(const FilamentEntry &entry);

    // --------------------------------------------------------------------------
    /**
     * @brief Einen Eintrag anhand der UID löschen
     * @param uid  UID des zu löschenden Eintrags
     * @return true, wenn erfolgreich gelöscht, false wenn UID nicht gefunden
     */
    // --------------------------------------------------------------------------
    bool remove(const String &uid);

    // --------------------------------------------------------------------------
    /**
     * @brief Die gesamte Datenbank als JsonArray erzeugen
     * @param doc  JsonDocument, in das das Array eingefügt wird
     * @return JsonArray mit allen Filament-Einträgen
     */
    // --------------------------------------------------------------------------
    JsonArray toJsonArray(JsonDocument &doc);

    // --------------------------------------------------------------------------
    /**
     * @brief Einen Eintrag anhand seines Index in der DB aktualisieren
     * @param idx    Index des Eintrags
     * @param entry  Neuer Eintrag
     * @return true bei Erfolg, false bei ungültigem Index
     */
    // --------------------------------------------------------------------------
    bool updateAtIndex(int idx, const FilamentEntry &entry);

    // --------------------------------------------------------------------------
    /**
     * @brief Die DB aus einem JsonArray laden
     * @param arr  JsonArray mit den Filament-Einträgen
     * @return true, wenn mindestens ein Eintrag geladen wurde, false sonst
     */
    // --------------------------------------------------------------------------
    bool loadFromJsonArray(JsonArray arr);

    // --------------------------------------------------------------------------
    /**
     * @brief Anzahl der Einträge in der DB abrufen
     * @return Anzahl der gespeicherten Filament-Einträge
     */
    // --------------------------------------------------------------------------
    int getAllCount();

} // namespace FilamentDB
