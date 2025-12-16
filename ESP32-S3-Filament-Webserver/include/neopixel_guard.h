#pragma once
/**
 * @file neopixel_guard.h
 * @brief Thread-sichere (oder optionale) Hüllfunktionen um Adafruit_NeoPixel::show().
 *
 * Hintergrund:
 *  - Die Adafruit-NeoPixel-Implementierung bzw. der darunterliegende RMT-Treiber
 *    ist nicht reentrant. Parallele show()-Aufrufe aus unterschiedlichen Tasks
 *    oder für mehrere Stripes können Glitches erzeugen.
 *  - Dieses Modul stellt wahlweise blockierende, non-blocking oder „direct“
 *    (ohne Schutz) Varianten bereit und ermöglicht ein möglichst synchrones
 *    Aktualisieren von zwei Stripes unter einem globalen Lock.
 *
 * Build-Optionen (GENAU EINE definieren – oder keine, dann Default):
 *
 *   - NEOPIXEL_SHOW_DIRECT
 *       * Kein Mutex, keine Wartezyklen.
 *       * Maximale Geschwindigkeit, ABER unsicher bei Multi-Tasking oder mehreren Stripes.
 *       * Nur verwenden, wenn 100% sicher: single-threaded, genau ein Strip.
 *
 *   - NEOPIXEL_SHOW_NONBLOCK
 *       * Non-Blocking: sendet nur, wenn canShow() SOFORT true ist und der globale Lock
 *         ohne Warten verfügbar ist. Andernfalls wird nicht gesendet.
 *       * Nützlich, wenn harte Timing-Deadlines existieren und Blockieren tabu ist.
 *
 *   - (Default, wenn keine der obigen Optionen definiert ist)
 *       * Blocking & threadsafe: globaler Mutex + aktives, kurzes Warten auf canShow().
 *       * Empfohlen für „normale“ Anwendungen mit mehreren Tasks/Stripes.
 */

#include <Adafruit_NeoPixel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Thread-sichere Variante von strip->show().
 *
 * Verhalten je nach Build-Option:
 *  - NEOPIXEL_SHOW_DIRECT:    Kein Lock, busy-spin bis canShow(), dann show().
 *  - NEOPIXEL_SHOW_NONBLOCK:  Sendet nur, wenn canShow() und Lock sofort frei.
 *  - Default (Blocking):      Holt globalen Lock, wartet auf canShow(), sendet.
 *
 * @param strip Zeiger auf den zu sendenden NeoPixel-Strip (nullptr wird ignoriert).
 */
void neopixelShowSafe(Adafruit_NeoPixel* strip);

/**
 * @brief „Try“-Variante: sendet nur, wenn es SOFORT geht (non-blocking Semantik).
 *
 * Verhalten je nach Build-Option:
 *  - NEOPIXEL_SHOW_DIRECT:    Erfolgreich nur, wenn canShow() true ist (kein Lock).
 *  - NEOPIXEL_SHOW_NONBLOCK:  Erfolgreich nur, wenn canShow() true ist UND der Lock
 *                             ohne Warten genommen werden kann.
 *  - Default (Blocking):      Ebenfalls „try“-Semantik: nur erfolgreich, wenn sowohl
 *                             canShow() als auch Lock sofort frei sind (kein Warten).
 *
 * @param strip Zeiger auf den NeoPixel-Strip (nullptr → false).
 * @return true  wenn gesendet wurde,
 * @return false wenn nicht gesendet wurde (z. B. canShow()==false oder Lock belegt).
 */
bool neopixelTryShow(Adafruit_NeoPixel* strip);

/**
 * @brief Aktualisiert zwei Stripes unter EINEM globalen Lock (so „gleichzeitig“ wie möglich).
 *
 * Reihenfolge ist stets A dann B. Im Blocking-Default wird der globale Lock einmal
 * genommen, auf canShow() für beide gewartet, anschließend show() für A und dann B
 * aufgerufen — minimaler zeitlicher Versatz innerhalb eines „Frames“.
 *
 * In NEOPIXEL_SHOW_NONBLOCK wird nur gesendet, wenn beide Stripes sofort senden dürfen
 * und der Lock ohne Warten verfügbar ist. In NEOPIXEL_SHOW_DIRECT wird ohne Lock gesendet.
 *
 * @param a Erster Strip (darf nullptr sein; dann wird nur B gesendet).
 * @param b Zweiter Strip (darf nullptr sein; dann wird nur A gesendet).
 */
void neopixelShowPairSafe(Adafruit_NeoPixel* a, Adafruit_NeoPixel* b);

#ifdef __cplusplus
} // extern "C"
#endif
