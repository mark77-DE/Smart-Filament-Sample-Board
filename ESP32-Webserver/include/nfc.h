#pragma once
#include <Arduino.h>
#include <Adafruit_PN532.h>
#include <WString.h>

/**
 * @file nfc.h
 * @brief Public NFC interface (PN532) + optional hooks.
 *
 * Diese API kapselt das nicht-blockierende Polling gegen den PN532 sowie
 * guard/preemption logic. The actual logic (LEDs/display) is handled through
 * handleUID() in der Anwendung bedient; hier stellen wir nur das Polling
 * and the status signals are provided here.
 *
 * Important notes:
 *  - Alle Funktionen sind *single-threaded* gedacht (typischer Arduino-Loop).
 *  - `tick()` is non-blocking and should be called regularly from `loop()`.
 *  - `init()` does *not* take ownership of the supplied PN532 pointer.
 */

namespace NFC
{

    /**
     * @brief Initializes the PN532 and internal guard state.
     *
     * Internally calls `begin()` and `SAMConfig()` and logs (when available)
     * the firmware version. The pointer remains owned by the caller.
     *
     * @param nfc  Valid pointer to an initializable `Adafruit_PN532` instance.
     */
    uint32_t init(Adafruit_PN532 *nfc);

    /**
     * @brief One-time, mostly "synchronous" UID query (debug/tools).
     *
     * Reads a UID using `readPassiveTargetID(...)` and returns it as a
     * hexadecimal string with colons (e.g. "04:F5:8E:52:6F:61:80").
     * Returns `""` when nothing was detected.
     *
     * @return UID string or an empty string.
     */
    String checkTag();

    /**
     * @brief Resets all internal guards/state.
     *
     * Useful after global resets or when external logic needs to
     * safely restart edge/hold detection.
     */
    void resetGuard();

    /**
     * @brief Non-blocking NFC polling + guard/preemption logic.
     *
     * Ideally call this function on every loop iteration.
     * It reads a detected tag *non-blocking*, debounces it,
     * applies "Sticky Presence" (grace), and triggers on rising edges
     * through the external `handleUID()` function (defined in the app).
     *
     * @param now            Aktuelle Zeit in Millisekunden (typisch: `millis()`).
     * @param[out] isActive  Set to `true` when a tag is currently present.
     * @param[out] lastTagTime  Time (ms) of the last detected presence.
     * @param[out] tagPresentOut  Presence status after grace/guards (true = "tag is considered present").
     */
    void tick(unsigned long now,
              bool &isActive,
              unsigned long &lastTagTime,
              bool &tagPresentOut);

    /**
     * @brief Returns the UID of the currently "held" tag (empty if none is active).
     *
     * Useful after an asynchronous operation (e.g. a FilaMan lookup) to verify whether
     * the tag for which the result was produced is still the same one currently displayed.
     */
    String currentHoldUid();

} // namespace NFC

// ============================================================================
// Optional hooks
// ============================================================================
// These symbols are defined as "weak" in nfc.cpp and may be overridden by the project
// to react immediately to preemption/activity.

/**
 * @brief Called when a new tag preempts an ongoing effect.
 *        (e.g. to refresh the display immediately)
 *
 * @param uid  UID of the new preemptive tag (same format as in checkTag()).
 */
void NFC_OnPreempt(const String &uid);

/**
 * @brief Called on every `tick()` while a tag is considered present.
 *        (e.g. to stop an idle animation)
 */
void NFC_OnActive();

struct NFCInfo
{
    uint8_t fwVerMajor;
    uint8_t fwVerMinor;
    uint16_t chipID;
    bool available;
};

// global instance
extern NFCInfo g_nfcInfo;