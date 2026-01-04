#pragma once
#include <Arduino.h>
#include <Adafruit_PN532.h>
#include <WString.h>

/**
 * @file nfc.h
 * @brief Öffentliche NFC-Schnittstelle (PN532) + optionale Hooks.
 *
 * Diese API kapselt das nicht-blockierende Polling gegen den PN532 sowie
 * Guard-/Preemption-Logik. Die eigentliche Logik (LEDs/Display) wird über
 * handleUID() in der Anwendung bedient; hier stellen wir nur das Polling
 * und die Statussignale bereit.
 *
 * Wichtige Hinweise:
 *  - Alle Funktionen sind *single-threaded* gedacht (typischer Arduino-Loop).
 *  - `tick()` ist nicht-blockierend und soll regelmäßig im `loop()` aufgerufen werden.
 *  - `init()` übernimmt *keine* Ownership des übergebenen PN532-Zeigers.
 */

namespace NFC {

  /**
   * @brief Initialisiert den PN532 und internen Guard-State.
   *
   * Ruft intern `begin()` und `SAMConfig()` auf und loggt (falls verfügbar)
   * die Firmware-Version. Der Pointer bleibt im Besitz des Aufrufers.
   *
   * @param nfc  Gültiger Zeiger auf eine initialisierbare `Adafruit_PN532`-Instanz.
   */
  void init(Adafruit_PN532* nfc);

  /**
   * @brief Einmalige, eher „synchrone“ UID-Abfrage (Debug/Tools).
   *
   * Liest per `readPassiveTargetID(...)` eine UID und gibt sie als
   * Hex-String mit Doppelpunkten (z. B. "04:F5:8E:52:6F:61:80") zurück.
   * Wenn nichts erkannt wurde, wird `""` zurückgegeben.
   *
   * @return UID-String oder leerer String.
   */
  String checkTag();

  /**
   * @brief Setzt alle internen Guards/State zurück.
   *
   * Nützlich nach globalen Resets oder wenn externe Logik die
   * Edge-/Hold-Erkennung sicher neu starten möchte.
   */
  void resetGuard();

  /**
   * @brief Nicht-blockierendes NFC-Polling + Guard/Preempt-Logik.
   *
   * Diese Funktion wird idealerweise in jeder Loop-Iteration aufgerufen.
   * Sie liest einen evtl. erkannten Tag *non-blocking* aus, entprellt,
   * führt „Sticky Presence“ (Grace) aus und triggert bei Rising-Edges
   * über die externe `handleUID()`-Funktion (definiert in der App).
   *
   * @param now            Aktuelle Zeit in Millisekunden (typisch: `millis()`).
   * @param[out] isActive  Wird auf `true` gesetzt, wenn aktuell ein Tag präsent ist.
   * @param[out] lastTagTime  Zeitpunkt (ms) der letzten festgestellten Präsenz.
   * @param[out] tagPresentOut  Präsenzstatus nach Grace/Guards (true = „Tag gilt als da“).
   */
  void tick(unsigned long now,
            bool& isActive,
            unsigned long& lastTagTime,
            bool& tagPresentOut);
} // namespace NFC

// ============================================================================
// Optionale Hooks
// ============================================================================
// Diese Symbole werden in nfc.cpp als „weak“ definiert und können vom Projekt
// überschrieben werden, um unmittelbar bei Preemption/Aktivität zu reagieren.

/**
 * @brief Wird gerufen, wenn ein neues Tag einen laufenden Effekt „überfährt“.
 *        (z. B. um ein Display sofort zu aktualisieren)
 *
 * @param uid  UID des neuen, präemptiven Tags (Format wie in checkTag()).
 */
void NFC_OnPreempt(const String& uid);

/**
 * @brief Wird in jedem `tick()` gerufen, solange ein Tag als präsent gilt.
 *        (z. B. um eine Idle-Animation zu beenden)
 */
void NFC_OnActive();


struct NFCInfo {
    uint8_t fwVerMajor;
    uint8_t fwVerMinor;
    uint16_t chipID;
    bool available;
};

// globale Instanz
extern NFCInfo g_nfcInfo;