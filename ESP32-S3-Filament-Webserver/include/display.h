#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_SSD1306.h>
#include "filament_db.h"
#include "display_config.h"
#include "globals.h"
#include "version_info.h"

// Typ-Alias kommt aus display_config.h, z. B.:
// using DisplayType = Adafruit_SH1106G;

/**
 * @brief Display-Hilfsklasse für Filament-Infos und Statusanzeigen.
 */
class MYDISPLAY {
public:
  /**
   * @brief Display-Backend setzen (einmalig im Setup).
   * @param disp Zeiger auf das Display-Objekt
   */
  static void init(DisplayType* disp);

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
  static void showThreeCentered(const String& line1, const String& line2, const String& line3);

  // --- Neu: Bootscreen mit Firmware-Version/Hash ---
  static void showBootVersion(const char* fw, const char* hash);

private:
  static DisplayType* _display;
};
