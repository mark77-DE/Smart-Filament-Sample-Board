#include "neopixel_guard.h"

/**
 * @file neopixel_guard.cpp
 * @brief Thread-safe (or optional) wrapper functions around Adafruit_NeoPixel::show().
 *
 * Goal:
 *  - Protect the non-reentrant RMT/NeoPixel driver from parallel calls.
 *  - Optionally provide non-blocking behavior.
 *  - Optionally update two strips "as simultaneously as possible".
 *
 * Build options (define once in the build configuration):
 *   - NEOPIXEL_SHOW_DIRECT
 *       No locks, no waiting loops — maximum speed, but unsafe with multiple
 *       tasks/strips. Only use when truly single-threaded and a single strip is used.
 *
 *   - NEOPIXEL_SHOW_NONBLOCK
 *       Non-blocking: sends only when the driver can send immediately
 *       (strip->canShow()) and the global lock is immediately available.
 *       Otherwise nothing is sent (try-semantics).
 *
 *   - (Default, if nothing is defined)
 *       Blocking & thread-safe: global mutex + active waiting on canShow().
 *       Recommended for multiple tasks or multiple strips.
 */

#if !defined(NEOPIXEL_SHOW_DIRECT)
  // In blocking and non-blocking modes we use a global mutex.
  #include <freertos/FreeRTOS.h>
  #include <freertos/semphr.h>

  /// One global lock for ALL strips (Adafruit implementation is not reentrant).
  static SemaphoreHandle_t s_globalShowMutex = nullptr;

  /// Ensures that the global mutex exists.
  static inline void ensureMutex() {
    if (!s_globalShowMutex) {
      s_globalShowMutex = xSemaphoreCreateMutex();
    }
  }

  /// Short active wait until the driver is allowed to send again.
  static inline void waitCanShow(Adafruit_NeoPixel* s) {
    while (!s->canShow()) {
      delayMicroseconds(10);  // short poll interval → low CPU load
    }
  }
#endif

// ============================================================================
//  Public API
// ============================================================================

/**
 * @brief Thread-safe variant of strip->show().
 *
 * Behavior depends on the build option:
 *  - NEOPIXEL_SHOW_DIRECT:    No lock; busy-spin until canShow(), then show().
 *  - NEOPIXEL_SHOW_NONBLOCK:  Sends only when canShow() and the mutex are immediately free.
 *  - (Default, Blocking):     Takes the global mutex, waits for canShow(), then sends.
 */
void neopixelShowSafe(Adafruit_NeoPixel* strip) {
  if (!strip) return;

#if defined(NEOPIXEL_SHOW_DIRECT)
  // No safety — use only when single-threaded & exactly one strip are guaranteed.
  while (!strip->canShow()) { /* busy spin (minimum latency) */ }
  strip->show();
  return;

#elif defined(NEOPIXEL_SHOW_NONBLOCK)
  // Non-blocking: send only if possible immediately.
  if (!strip->canShow()) return;
  ensureMutex();
  if (!s_globalShowMutex) { strip->show(); return; }
  if (xSemaphoreTake(s_globalShowMutex, 0) == pdTRUE) {
    if (strip->canShow()) strip->show();
    xSemaphoreGive(s_globalShowMutex);
  }
  return;

#else
  // Blocking & thread-safe (recommended, especially with multiple strips).
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
 * @brief "Try" variant of show(): sends only if it can do so immediately.
 *
 * @return true  if sent,
 *         false if either canShow() was false or (in mutex mode) the lock was busy.
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
  // Blocking mode: "try" succeeds only if the lock + canShow are immediately free.
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
 * @brief Updates two strips under ONE global lock (as simultaneously as possible).
 *
 * - DIRECT:     No lock; sends A then B (only safe in single-thread setups).
 * - NONBLOCK:   Sends only if both canShow() are true and the lock is immediately free.
 * - Blocking:   Takes the global lock, waits for A/B canShow(), then show() A, show() B.
 */
void neopixelShowPairSafe(Adafruit_NeoPixel* a, Adafruit_NeoPixel* b) {
  if (!a && !b) return;
  if (a && !b) { neopixelShowSafe(a); return; }
  if (b && !a) { neopixelShowSafe(b); return; }

#if defined(NEOPIXEL_SHOW_DIRECT)
  // Best effort directly, without a global lock (only safe in a single-thread setup!).
  while (!a->canShow()) { /* busy spin */ }
  a->show();
  while (!b->canShow()) { /* busy spin */ }
  b->show();
  return;

#elif defined(NEOPIXEL_SHOW_NONBLOCK)
  // Non-blocking: send only if both can send immediately and the lock is free.
  if (!a->canShow() || !b->canShow()) return;
  ensureMutex();
  if (!s_globalShowMutex) { a->show(); b->show(); return; }
  if (xSemaphoreTake(s_globalShowMutex, 0) != pdTRUE) return;
  bool sentA = false, sentB = false;
  if (a->canShow()) { a->show(); sentA = true; }
  if (b->canShow()) { b->show(); sentB = true; }
  xSemaphoreGive(s_globalShowMutex);
  (void)sentA; (void)sentB; // placeholder for future logging if desired.
  return;

#else
  // Blocking: global lock -> A then B, minimal time offset in the same "frame".
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
