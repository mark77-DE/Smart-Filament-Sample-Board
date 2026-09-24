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
 * @brief Initializes the GPIO button and buzzer subsystem.
 * @details Re-applies the active configuration and resets internal debouncing state.
 * @note Safe to call repeatedly after a config reload.
 */
void gpiohw_init();

/**
 * @brief Updates the debounced button state and buzzer sequencer.
 * @note Call this once per loop iteration.
 */
void gpiohw_update();

/**
 * @brief Updates the GPIO state using an externally supplied timestamp.
 * @param now Current time in milliseconds.
 */
void gpiohw_tick(unsigned long now);

// ---------------------------------------------------------------------------
// Buzzer-API (Sequenzen laufen non-blocking, werden im Tick abgearbeitet)
// ---------------------------------------------------------------------------

/**
 * @brief Emits a single short beep.
 */
void buzzer_single_beep();

/**
 * @brief Emits a double-beep pattern.
 */
void buzzer_double_beep();

/**
 * @brief Emits the configured error beep sequence.
 */
void buzzer_error_beep();

/**
 * @brief Interrupt beep sequence -> buzzer off.
 */
void buzzer_stop();

/**
 * @brief true if buzzer busy, otherwise false.
 * @return return true if buzzer busy, otherwise false;
 */
bool buzzer_busy();

// ---------------------------------------------------------------------------
// Button events (auto-reset getters, like in the previous implementation)
// ---------------------------------------------------------------------------

/**
 * @brief Returns true once for a short press event.
 * @return true when a valid short press was detected, otherwise false.
 */
bool button_short_press();
/**
 * @brief Returns true once for a long press event.
 * @return true when a valid long press was detected, otherwise false.
 */
bool button_long_press();
/**
 * @brief Returns true once for a double press event.
 * @return true when a valid double press was detected, otherwise false.
 */
bool button_double_press();
/**
 * @brief Returns true once when button is in hold for more than CONFIGV2.button.holdRepeatMs.
 * @return true when button is hold for more than CONFIGV2.button.holdRepeatMs, otherwise false.
 */
bool button_hold();

// Fired immediately on release (if no long press was detected).
// Independent of the double-press window. True once (auto-reset).
bool button_tap_release();
void gpiohw_reset_click_state(); // Completely clear click/double/long logic state
