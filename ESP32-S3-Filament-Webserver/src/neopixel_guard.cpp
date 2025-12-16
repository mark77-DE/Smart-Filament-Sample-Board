#include "neopixel_guard.h"

/**
 * @file neopixel_guard.cpp
 * @brief Thread-sichere (oder optionale) Hüllfunktionen um Adafruit_NeoPixel::show().
 *
 * Ziel:
 *  - Den nicht-reentranten RMT-/NeoPixel-Treiber vor parallelen Aufrufen schützen.
 *  - Optional non-blocking Verhalten anbieten.
 *  - Optional zwei Stripes „so gleichzeitig wie möglich“ aktualisieren.
 *
 * Build-Optionen (einmalig in der Build-Konfiguration definieren):
 *   - NEOPIXEL_SHOW_DIRECT
 *       Keine Locks, keine Wartezyklen — maximal schnell, aber unsicher bei
 *       mehreren Tasks/Stripes. Nur nutzen, wenn wirklich single-threaded
 *       und genau ein Strip gesteuert wird.
 *
 *   - NEOPIXEL_SHOW_NONBLOCK
 *       Non-blocking: sendet nur, wenn sowohl der Treiber sofort senden kann
 *       (strip->canShow()) als auch der globale Lock sofort verfügbar ist.
 *       Andernfalls wird einfach nichts gesendet (Try-Semantik).
 *
 *   - (Default, wenn nichts definiert ist)
 *       Blocking & threadsafe: globaler Mutex + aktives Warten auf canShow().
 *       Empfohlen bei mehreren Tasks oder mehreren Stripes.
 */

#if !defined(NEOPIXEL_SHOW_DIRECT)
  // Im Blocking- und Non-Blocking-Modus nutzen wir einen globalen Mutex.
  #include <freertos/FreeRTOS.h>
  #include <freertos/semphr.h>

  /// Ein globaler Lock für ALLE Strips (Adafruit-Implementierung ist nicht reentrant).
  static SemaphoreHandle_t s_globalShowMutex = nullptr;

  /// Stellt sicher, dass der globale Mutex existiert.
  static inline void ensureMutex() {
    if (!s_globalShowMutex) {
      s_globalShowMutex = xSemaphoreCreateMutex();
    }
  }

  /// Kurzes aktives Warten, bis der Treiber wieder senden darf.
  static inline void waitCanShow(Adafruit_NeoPixel* s) {
    while (!s->canShow()) {
      delayMicroseconds(10);  // kleine Poll-Periode → geringe CPU-Last
    }
  }
#endif

// ============================================================================
//  Öffentliche API
// ============================================================================

/**
 * @brief Thread-sichere Variante von strip->show().
 *
 * Verhalten je nach Build-Option:
 *  - NEOPIXEL_SHOW_DIRECT:    Kein Lock, busy-spin bis canShow, dann show().
 *  - NEOPIXEL_SHOW_NONBLOCK:  Sendet nur, wenn canShow() und Mutex sofort frei.
 *  - (Default, Blocking):     Nimmt globalen Mutex, wartet auf canShow(), sendet.
 */
void neopixelShowSafe(Adafruit_NeoPixel* strip) {
  if (!strip) return;

#if defined(NEOPIXEL_SHOW_DIRECT)
  // Keine Safety – NUR benutzen, wenn garantiert single-threaded & ein Strip.
  while (!strip->canShow()) { /* busy spin (minimale Latenz) */ }
  strip->show();
  return;

#elif defined(NEOPIXEL_SHOW_NONBLOCK)
  // Non-blocking: sende nur, wenn sofort möglich.
  if (!strip->canShow()) return;
  ensureMutex();
  if (!s_globalShowMutex) { strip->show(); return; }
  if (xSemaphoreTake(s_globalShowMutex, 0) == pdTRUE) {
    if (strip->canShow()) strip->show();
    xSemaphoreGive(s_globalShowMutex);
  }
  return;

#else
  // Blocking & threadsafe (empfohlen, besonders bei MEHREREN Strips).
  ensureMutex();
  if (!s_globalShowMutex) {
    waitCanShow(strip);
    strip->show();
    return;
  }
  xSemaphoreTake(s_globalShowMutex, portMAX_DELAY);
  waitCanShow(strip);
  strip->show();
  xSemaphoreGive(s_globalShowMutex);
  return;
#endif
}

/**
 * @brief „Try“-Variante von show(): sendet nur, wenn es SOFORT geht.
 *
 * @return true  wenn gesendet wurde,
 *         false wenn entweder canShow() false war oder (im Mutex-Modus) der Lock belegt war.
 */
bool neopixelTryShow(Adafruit_NeoPixel* strip) {
  if (!strip) return false;

#if defined(NEOPIXEL_SHOW_DIRECT)
  if (!strip->canShow()) return false;
  strip->show();
  return true;

#elif defined(NEOPIXEL_SHOW_NONBLOCK)
  if (!strip->canShow()) return false;
  ensureMutex();
  if (!s_globalShowMutex) { strip->show(); return true; }
  if (xSemaphoreTake(s_globalShowMutex, 0) != pdTRUE) return false;
  bool ok = false;
  if (strip->canShow()) { strip->show(); ok = true; }
  xSemaphoreGive(s_globalShowMutex);
  return ok;

#else
  // Blocking-Modus: „try“ nur erfolgreich, wenn Lock + canShow sofort frei.
  if (!strip->canShow()) return false;
  ensureMutex();
  if (!s_globalShowMutex) { strip->show(); return true; }
  if (xSemaphoreTake(s_globalShowMutex, 0) != pdTRUE) return false;
  bool ok = false;
  if (strip->canShow()) { strip->show(); ok = true; }
  xSemaphoreGive(s_globalShowMutex);
  return ok;
#endif
}

/**
 * @brief Aktualisiert zwei Stripes unter EINEM globalen Lock (so „gleichzeitig“ wie möglich).
 *
 * - DIRECT:     Kein Lock; sendet A gefolgt von B (nur sicher in Single-Thread-Setups).
 * - NONBLOCK:   Sendet nur, wenn beide canShow() true und der Lock sofort frei ist.
 * - Blocking:   Holt den globalen Lock, wartet auf A/B canShow(), dann show() A, show() B.
 */
void neopixelShowPairSafe(Adafruit_NeoPixel* a, Adafruit_NeoPixel* b) {
  if (!a && !b) return;
  if (a && !b) { neopixelShowSafe(a); return; }
  if (b && !a) { neopixelShowSafe(b); return; }

#if defined(NEOPIXEL_SHOW_DIRECT)
  // Best-Effort direkt, ohne globalen Lock (nur sicher, wenn single-threaded!).
  while (!a->canShow()) { /* busy spin */ }
  a->show();
  while (!b->canShow()) { /* busy spin */ }
  b->show();
  return;

#elif defined(NEOPIXEL_SHOW_NONBLOCK)
  // Non-blocking: sende nur, wenn beide sofort können und der Lock frei ist.
  if (!a->canShow() || !b->canShow()) return;
  ensureMutex();
  if (!s_globalShowMutex) { a->show(); b->show(); return; }
  if (xSemaphoreTake(s_globalShowMutex, 0) != pdTRUE) return;
  bool sentA = false, sentB = false;
  if (a->canShow()) { a->show(); sentA = true; }
  if (b->canShow()) { b->show(); sentB = true; }
  xSemaphoreGive(s_globalShowMutex);
  (void)sentA; (void)sentB; // Platzhalter, falls später Logging gewünscht ist.
  return;

#else
  // Blocking: globaler Lock -> A dann B, minimaler Zeitversatz im selben „Frame“.
  ensureMutex();
  if (!s_globalShowMutex) {
    while (!a->canShow()) { delayMicroseconds(10); }
    a->show();
    while (!b->canShow()) { delayMicroseconds(10); }
    b->show();
    return;
  }
  xSemaphoreTake(s_globalShowMutex, portMAX_DELAY);
  while (!a->canShow()) { delayMicroseconds(10); }
  a->show();
  while (!b->canShow()) { delayMicroseconds(10); }
  b->show();
  xSemaphoreGive(s_globalShowMutex);
  return;
#endif
}
