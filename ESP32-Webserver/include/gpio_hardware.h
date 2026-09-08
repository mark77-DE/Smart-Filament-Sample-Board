#pragma once
#include <Arduino.h>
#include "filehandling.h"  // stellt CONFIG bereit (AppConfig)

// ============================================================================
// GPIO-Hardware-Modul: Button (entprellt + Events) & Buzzer (Sequenzen, non-blocking)
// - Uses CONFIG.button / CONFIG.buzzer, but falls back to defaults,
//   wenn Keys in /config.json fehlen.
// - ESP32: passiver Buzzer via LEDC (PWM), aktiver via digitalWrite.
// ============================================================================

/**
 * @brief Initializes the button and buzzer according to the configuration.
 *        Mehrfachaufruf ist erlaubt (re-init).
 */
void gpiohw_init();

/**
 * @brief Zyklischer Updater (non-blocking). In jeder loop() aufrufen.
 *        Alias ruft intern gpiohw_tick(millis()) auf.
 */
void gpiohw_update();

/**
 * @brief Zyklischer Updater mit externem Zeitstempel.
 * @param now Aktuelle Zeit in Millisekunden (millis()).
 */
void gpiohw_tick(unsigned long now);

// ---------------------------------------------------------------------------
// Buzzer-API (Sequenzen laufen non-blocking, werden im Tick abgearbeitet)
// ---------------------------------------------------------------------------

/** @brief Ein kurzer Pieps. */
void buzzer_single_beep();
/** @brief Two short beeps with a short pause. */
void buzzer_double_beep();
/** @brief Fehlersequenz: mehrere kurze Pieps (konfigurierbar). */
void buzzer_error_beep();
/** @brief Sequenz sofort abbrechen (Buzzer aus). */
void buzzer_stop();
/** @brief true while a sequence is running. */
bool buzzer_busy();

// ---------------------------------------------------------------------------
// Button-Events (Getter mit Auto-Reset, wie in deinem alten Stand)
// ---------------------------------------------------------------------------

/** @brief true genau einmal pro kurzem Tastendruck (kein Double, kein Long). */
bool button_short_press();
/** @brief true genau einmal, wenn Long-Press erreicht wurde. */
bool button_long_press();
/** @brief true genau einmal, wenn Double-Click erkannt wurde. */
bool button_double_press();
/** @brief true bei jedem Hold-Intervall nach Long-Press. */
bool button_hold();

// Feuert SOFORT beim Loslassen (wenn kein Long erkannt wurde).
// Independent of the double-press window. True once (auto-reset).
bool button_tap_release();
void gpiohw_reset_click_state(); // Click/Double/Long-Logik komplett flushen
