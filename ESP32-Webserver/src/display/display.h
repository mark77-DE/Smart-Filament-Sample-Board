#pragma once

#include <Adafruit_GFX.h>
#include "filament_db.h"
#include "display_config.h"
#include "globals.h"
#include "version_info.h"


void displayInit();
void displayClear();
void displayFlush();

/**
 * @brief Display-Hilfsklasse für Filament-Infos und Statusanzeigen.
 */
class MYDISPLAY {
public:
    /**
     * @brief Display-Backend setzen (einmalig im Setup).
     * @param disp Zeiger auf das Display-Objekt
     */
    static void init(DisplayType* disp) { _display = disp; }

    /**
     * @brief Zeigt Filament-Daten (vendor / type / color) in drei Zeilen.
     */
    static void show(const FilamentEntry& entry);

    /**
     * @brief Zeigt eine einzelne zentrierte Zeile.
     * @param msg Text
     */
    static void showCentered(const String& msg);

    /**
     * @brief Zeigt zwei zentrierte Zeilen (bestehende API beibehalten).
     * @param line1 Erste Zeile
     * @param line2 Zweite Zeile
     */
    static void showCenteredTwoLines(const String& line1, const String& line2);

    /**
     * @brief Zeigt drei zentrierte Zeilen (neu, für Reboot-Countdown).
     *        Kürzt bei Bedarf mit "..." und passt den Font je nach Display-Höhe an.
     * @param line1 Erste Zeile (oben)
     * @param line2 Zweite Zeile (Mitte)
     * @param line3 Dritte Zeile (unten)
     */
    static void showThreeLinesCentered(const String& line1, const String& line2, const String& line3);

    /**
     * @brief Zeigt vier zentrierte Zeilen.
     * @param line1 Erste Zeile (oben)
     * @param line2 Zweite Zeile
     * @param line3 Dritte Zeile
     * @param line4 Vierte Zeile (unten)
     */
    static void showFourLinesCentered(const String& line1, const String& line2, const String& line3, const String& line4);

    /**
     * @brief Bootscreen mit Firmware-Version/Datum
     * @param version Versionstext
     * @param dateShort Datum
     */
    static void showBootVersion(const char* version, const char* dateShort);

    /**
     * @brief Bildschirm löschen (z.B. vor Idle-Animation).
     */
    static void clear();

private:
    static DisplayType* _display;
};