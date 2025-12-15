#include "neopixel_guard.h"

#if !defined(NEOPIXEL_SHOW_DIRECT)
  #include <freertos/FreeRTOS.h>
  #include <freertos/semphr.h>
  // EIN globaler Lock für alle Strips (RMT-Treiber & Adafruit-Impl sind nicht reentrant)
  static SemaphoreHandle_t s_globalShowMutex = nullptr;

  static inline void ensureMutex() {
    if (!s_globalShowMutex) {
      s_globalShowMutex = xSemaphoreCreateMutex();
    }
  }

  // kleine Hilfen
  static inline void waitCanShow(Adafruit_NeoPixel* s) {
    while (!s->canShow()) { delayMicroseconds(10); } // kurze Poll-Wartezeit
  }
#endif

void neopixelShowSafe(Adafruit_NeoPixel* strip) {
  if (!strip) return;

#if defined(NEOPIXEL_SHOW_DIRECT)
  // Keine Safety – NUR benutzen, wenn garantiert single-threaded & ein Strip
  while (!strip->canShow()) { /* busy spin (min Delay) */ }
  strip->show();
  return;

#elif defined(NEOPIXEL_SHOW_NONBLOCK)
  // Non-blocking: sende nur, wenn sofort möglich
  if (!strip->canShow()) return;
  ensureMutex();
  if (!s_globalShowMutex) { strip->show(); return; }
  if (xSemaphoreTake(s_globalShowMutex, 0) == pdTRUE) {
    if (strip->canShow()) strip->show();
    xSemaphoreGive(s_globalShowMutex);
  }
  return;

#else
  // Blocking & threadsafe (empfohlen, besonders bei MEHREREN Strips)
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
  // Blocking-Modus: „try“ nur erfolgreich, wenn Lock + canShow sofort frei
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

void neopixelShowPairSafe(Adafruit_NeoPixel* a, Adafruit_NeoPixel* b) {
  if (!a && !b) return;
  if (a && !b) { neopixelShowSafe(a); return; }
  if (b && !a) { neopixelShowSafe(b); return; }

#if defined(NEOPIXEL_SHOW_DIRECT)
  // Best-Effort: direkt, ohne globalen Lock (nur sicher, wenn single-threaded!)
  while (!a->canShow()) { }
  a->show();
  while (!b->canShow()) { }
  b->show();
  return;

#elif defined(NEOPIXEL_SHOW_NONBLOCK)
  // Non-blocking: sende nur, wenn beide sofort können und Lock frei ist
  if (!a->canShow() || !b->canShow()) return;
  ensureMutex();
  if (!s_globalShowMutex) { a->show(); b->show(); return; }
  if (xSemaphoreTake(s_globalShowMutex, 0) != pdTRUE) return;
  bool sentA=false, sentB=false;
  if (a->canShow()) { a->show(); sentA=true; }
  if (b->canShow()) { b->show(); sentB=true; }
  xSemaphoreGive(s_globalShowMutex);
  (void)sentA; (void)sentB;
  return;

#else
  // Blocking: globaler Lock -> A dann B, minimaler Abstand im selben Frame
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
