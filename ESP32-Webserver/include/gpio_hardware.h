#pragma once
#include <Arduino.h>
#include "filehandling.h"  // stellt CONFIG bereit (AppConfig)

// ============================================================================
// GPIO hardware module: button (debounced + events) & buzzer (sequences, non-blocking)
// - Uses CONFIG.button / CONFIG.buzzer, but falls back to defaults,
//   if keys are missing in /config_v2.json.
// - ESP32: passive buzzer via LEDC (PWM), active via digitalWrite.
// ============================================================================

/**
 * @brief Initializes the button and buzzer according to the configuration.
 *        Mehrfachaufruf ist erlaubt (re-init).
 */
void gpiohw_init();

/**
 * @brief Cyclic updater (non-blocking). Call in every loop().
 *        It internally calls gpiohw_tick(millis()).
 */
void gpiohw_update();

/**
 * @brief Cyclic updater with an external timestamp.
 * @param now Current time in milliseconds (millis()).
 */
void gpiohw_tick(unsigned long now);

// ---------------------------------------------------------------------------
// Buzzer-API (Sequenzen laufen non-blocking, werden im Tick abgearbeitet)
// ---------------------------------------------------------------------------

/** @brief A short beep. */
void buzzer_single_beep();
/** @brief Two short beeps with a short pause. */
void buzzer_double_beep();
/** @brief Error sequence: multiple short beeps (configurable). */
void buzzer_error_beep();
/** @brief Sequenz sofort abbrechen (Buzzer aus). */
void buzzer_stop();
/** @brief true while a sequence is running. */
bool buzzer_busy();

// ---------------------------------------------------------------------------
// Button events (auto-reset getters, like in the previous implementation)
// ---------------------------------------------------------------------------

/** @brief true once per short button press (no double, no long). */
bool button_short_press();
/** @brief true once when the long-press threshold is reached. */
bool button_long_press();
/** @brief true once when a double-click is detected. */
bool button_double_press();
/** @brief true on each hold interval after a long press. */
bool button_hold();

// Fired immediately on release (if no long press was detected).
// Independent of the double-press window. True once (auto-reset).
bool button_tap_release();
void gpiohw_reset_click_state(); // Completely clear click/double/long logic state
