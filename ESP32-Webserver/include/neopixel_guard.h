#pragma once
/**
 * @file neopixel_guard.h
 * @brief Thread-safe (or optional) wrapper functions around Adafruit_NeoPixel::show().
 *
 * Background:
 *  - The Adafruit NeoPixel implementation and the underlying RMT driver are not reentrant.
 *    Parallel show() calls from different tasks or multiple strips can cause glitches.
 *  - This module provides blocking, non-blocking, or "direct" (without protection)
 *    variants and enables updates that are as synchronized as possible for two strips
 *    under a global lock.
 *
 * Build options (define exactly one – or none for the default):
 *
 *   - NEOPIXEL_SHOW_DIRECT
 *       * No mutex, no waiting loops.
 *       * Maximum speed, but unsafe with multi-tasking or multiple strips.
 *       * Only use when 100% sure: single-threaded, exactly one strip.
 *
 *   - NEOPIXEL_SHOW_NONBLOCK
 *       * Non-blocking: sends only when canShow() is immediately true and the
 *         global lock is available without waiting. Otherwise nothing is sent.
 *       * Useful when hard timing deadlines exist and blocking is not allowed.
 *
 *   - (Default, if none of the above options are defined)
 *       * Blocking & thread-safe: global mutex + short active waiting for canShow().
 *       * Recommended for "normal" applications with multiple tasks/strips.
 */

#include <Adafruit_NeoPixel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Thread-safe variant of strip->show().
 *
 * Behavior depends on build option:
 *  - NEOPIXEL_SHOW_DIRECT:    No lock, busy-spin until canShow(), then show().
 *  - NEOPIXEL_SHOW_NONBLOCK:  Sends only if canShow() and the lock are immediately free.
 *  - Default (Blocking):      Takes the global lock, waits for canShow(), then sends.
 *
 * @param strip Pointer to the NeoPixel strip to send (nullptr is ignored).
 */
void neopixelShowSafe(Adafruit_NeoPixel* strip);

/**
 * @brief "Try" variant: sends only if it can do so immediately (non-blocking semantics).
 *
 * Behavior depends on build option:
 *  - NEOPIXEL_SHOW_DIRECT:    Succeeds only if canShow() is true (no lock).
 *  - NEOPIXEL_SHOW_NONBLOCK:  Succeeds only if canShow() is true and the lock can be
 *                             acquired without waiting.
 *  - Default (Blocking):      Also uses "try" semantics: succeeds only if both
 *                             canShow() and the lock are immediately free.
 *
 * @param strip Pointer to the NeoPixel strip (nullptr → false).
 * @return true  if sent,
 * @return false if not sent (e.g. canShow() == false or the lock is busy).
 */
bool neopixelTryShow(Adafruit_NeoPixel* strip);

/**
 * @brief Updates two strips under ONE global lock (as "simultaneously" as possible).
 *
 * The order is always A then B. In the blocking default, the global lock is taken once,
 * waits for canShow() on both, then calls show() for A and then B, minimizing timing skew
 * within one "frame".
 *
 * In NEOPIXEL_SHOW_NONBLOCK, both strips are sent only when they can both send immediately
 * and the lock is available without waiting. NEOPIXEL_SHOW_DIRECT sends without a lock.
 *
 * @param a First strip (may be nullptr; then only B is sent).
 * @param b Second strip (may be nullptr; then only A is sent).
 */
void neopixelShowPairSafe(Adafruit_NeoPixel* a, Adafruit_NeoPixel* b);

#ifdef __cplusplus
} // extern "C"
#endif
