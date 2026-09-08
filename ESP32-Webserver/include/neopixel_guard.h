#pragma once
/**
 * @file neopixel_guard.h
 * @brief Thread-safe (or optional) wrapper functions around Adafruit_NeoPixel::show().
 *
 * Hintergrund:
 *  - Die Adafruit-NeoPixel-Implementierung bzw. der darunterliegende RMT-Treiber
 *    ist nicht reentrant. Parallele show()-Aufrufe aus unterschiedlichen Tasks
 *    or multiple strips can cause glitches.
 *  - Dieses Modul stellt wahlweise blockierende, non-blocking oder „direct“
 *    (without protection) variants and enables updates that are as synchronized
 *    as possible for two strips under a global lock.
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
 *       * Useful when hard timing deadlines exist and blocking is not allowed.
 *
 *   - (Default, wenn keine der obigen Optionen definiert ist)
 *       * Blocking & threadsafe: globaler Mutex + aktives, kurzes Warten auf canShow().
 *       * Recommended for "normal" applications with multiple tasks/strips.
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
 * @brief Updates two strips under ONE global lock (as "simultaneously" as possible).
 *
 * Reihenfolge ist stets A dann B. Im Blocking-Default wird der globale Lock einmal
 * taken, waits for canShow() on both, then calls show() for A and then B
 * aufgerufen — minimaler zeitlicher Versatz innerhalb eines „Frames“.
 *
 * In NEOPIXEL_SHOW_NONBLOCK wird nur gesendet, wenn beide Stripes sofort senden dürfen
 * and the lock is available without waiting. NEOPIXEL_SHOW_DIRECT sends without a lock.
 *
 * @param a Erster Strip (darf nullptr sein; dann wird nur B gesendet).
 * @param b Zweiter Strip (darf nullptr sein; dann wird nur A gesendet).
 */
void neopixelShowPairSafe(Adafruit_NeoPixel* a, Adafruit_NeoPixel* b);

#ifdef __cplusplus
} // extern "C"
#endif
