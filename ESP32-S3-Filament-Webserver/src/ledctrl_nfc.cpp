#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Adafruit_NeoPixel.h>
#include "ledctrl_nfc.h"

int NFC_LED_COUNT = 0;
int NFC_LED_PIN = 15;
int NFC_LED_BRIGHTNESS = 255;
unsigned long NFC_LED_TIMEOUT = 20000; // 2 Sekunden

bool idlePulseEnabled = true;       // Pulsen im Idle



float minBrightness = 0.35f;        // Mindesthelligkeit

uint32_t NFC_LED_COLOR_SUCCESS = 0x00FF00; // grün
uint32_t NFC_LED_COLOR_ERROR   = 0xFF0000; // rot

uint32_t NFC_LED_COLOR_PULSE = 0x0033AA;   // statt 0x000066 (deutlich smoother)

// ---------- Idle-Puls Feinschliff (nur dafür neu) ----------
static unsigned long s_lastPulseUpdate = 0;
static const uint16_t PULSE_INTERVAL_MS = 16;     // ~60 FPS
static const uint16_t BREATHS_PER_MIN = 15;       // ähnlich beatsin8(15, ...)

// 4x4 Bayer-Matrix (Ordered Dither). Werte 0..15.
static const uint8_t BAYER4[16] = {
  0,  8,  2, 10,
 12,  4, 14,  6,
  3, 11,  1,  9,
 15,  7, 13,  5
};
static uint8_t s_ditherIndex = 0; // läuft 0..15
// -----------------------------------------------------------

Adafruit_NeoPixel* LEDCTRL_NFC::_leds = nullptr;

// State Tracking
enum LedState { LED_OFF, LED_SUCCESS, LED_ERROR };
LedState currentState = LED_OFF;
unsigned long stateStartTime = 0;

void LEDCTRL_NFC::init(int count, int pin, int timeout, int brightness) {
    NFC_LED_COUNT = count;
    NFC_LED_PIN = pin;
    NFC_LED_BRIGHTNESS = brightness;
    NFC_LED_TIMEOUT = timeout;

    if (_leds) delete _leds;

    _leds = new Adafruit_NeoPixel(NFC_LED_COUNT, NFC_LED_PIN, NEO_GRB + NEO_KHZ800);
    _leds->begin();
    _leds->setBrightness(NFC_LED_BRIGHTNESS);

    // Puls-Timer sauber initialisieren, damit beim Start keine Sprünge entstehen
    s_lastPulseUpdate = millis();

    allOff();
}

void LEDCTRL_NFC::setPixel(int index, uint32_t color) {
    if (!_leds) return;
    if (index < 0 || index >= NFC_LED_COUNT) return;

    _leds->setPixelColor(index, color);
    _leds->show();
}

void LEDCTRL_NFC::allOff() {
    if (!_leds) return;
    _leds->setBrightness(NFC_LED_BRIGHTNESS);
    for (int i = 0; i < NFC_LED_COUNT; i++) {
        _leds->setPixelColor(i, 0);
    }
    _leds->show();
}

// Nicht-blockierende Anzeige
void LEDCTRL_NFC::showSuccess() {
    currentState = LED_SUCCESS;
    stateStartTime = millis();
    _leds->setBrightness(NFC_LED_BRIGHTNESS); // zurück auf Basishelligkeit
    for (int i = 0; i < NFC_LED_COUNT; i++) {
        _leds->setPixelColor(i, NFC_LED_COLOR_SUCCESS);
    }

    _leds->show();
}

void LEDCTRL_NFC::showError() {
    currentState = LED_ERROR;
    stateStartTime = millis();
    _leds->setBrightness(NFC_LED_BRIGHTNESS); // zurück auf Basishelligkeit
    for (int i = 0; i < NFC_LED_COUNT; i++) {
        _leds->setPixelColor(i, NFC_LED_COLOR_ERROR);
    }

    _leds->show();
}

// 8-bit Sinus (0..255) aus millis() und BPM (breaths per minute)
// Kein float nötig. Nutzt sinf intern nicht, sondern eine kleine Approx per LUT wäre möglich,
// aber auf ESP32 ist sinf ok. Hier trotzdem integer-friendly: wir berechnen Phase und nutzen sinf einmal.
static uint8_t breath8(uint16_t bpm, uint32_t nowMs, uint8_t low, uint8_t high) {
    // Periodendauer in ms: 60.000 / bpm
    const float periodMs = 60000.0f / (float)bpm;
    float phase01 = fmodf((float)nowMs, periodMs) / periodMs;          // 0..1
    float s = (sinf(phase01 * 2.0f * PI) + 1.0f) * 0.5f;              // 0..1
    uint16_t v = (uint16_t)(low + s * (float)(high - low) + 0.5f);    // low..high
    if (v > 255) v = 255;
    return (uint8_t)v;
}


// Diese Funktion muss regelmäßig im loop() aufgerufen werden
void LEDCTRL_NFC::update() {
    if (!_leds) return;

    unsigned long now = millis();

    // Erfolgs-/Fehlerzustand prüfen
    if (currentState == LED_SUCCESS || currentState == LED_ERROR) {
        if (now - stateStartTime >= NFC_LED_TIMEOUT) {
            allOff();
            currentState = LED_OFF;

            // Nach einem State-Ende den Puls-Timer neu setzen, damit es weich weitergeht
            s_lastPulseUpdate = now;
        }
        return; // während Success/Error keine Idle-Animation
    }


    // Idle-Puls (8-bit "beatsin" style + ordered dithering)
    if (idlePulseEnabled) {

        if (now - s_lastPulseUpdate < PULSE_INTERVAL_MS) return;
        s_lastPulseUpdate = now;

        // minBrightness (0..1) -> als 8-bit low clampen
        uint8_t low8  = (uint8_t)constrain((int)lroundf(minBrightness * (float)NFC_LED_BRIGHTNESS), 0, 255);
        uint8_t high8 = (uint8_t)constrain(NFC_LED_BRIGHTNESS, 0, 255);

        // 8-bit Sinus-Level (low..high)
        uint8_t level = breath8(BREATHS_PER_MIN, now, low8, high8);

        // Gamma auf Brightness (visuell gleichmäßiger)
        uint8_t gLevel = Adafruit_NeoPixel::gamma8(level);

        // --- Verbesserter Ordered Dither (sauber 12-bit) ---
        // Wir erzeugen ein 12-bit Ziel (gLevel * 16 + frac).
        // frac nehmen wir aus dem Sinus vor der Rundung, damit es wirklich "Zwischenstufen" gibt.
        //
        // Trick: wir berechnen zusätzlich ein "feineres" Level, indem wir breath8 zweimal samplen:
        // einmal normal (grob), einmal mit minimaler Zeitverschiebung (fein). Das liefert eine stabile Fraction.
        //
        // Alternativ wäre echtes float->12bit, aber das hier bleibt simpel und funktioniert sehr gut.
        uint8_t levelNext = breath8(BREATHS_PER_MIN, now + (PULSE_INTERVAL_MS / 2), low8, high8);

        // Fraction 0..15 aus der Differenz (clamped)
        int diff = (int)levelNext - (int)level;     // -255..255
        if (diff < 0) diff = -diff;
        uint8_t frac = (uint8_t)constrain(diff << 2, 0, 15); // diff grob in 0..15 skalieren

        uint16_t v12 = ((uint16_t)gLevel << 4) | frac; // 12-bit Zielwert

        uint8_t thresh = BAYER4[s_ditherIndex];     // 0..15
        s_ditherIndex = (s_ditherIndex + 1) & 0x0F;

        // Basis = obere 8 bit, +1 wenn frac > threshold
        uint8_t outBr = (uint8_t)(v12 >> 4);
        if ((v12 & 0x0F) > thresh && outBr < 255) outBr++;

        _leds->setBrightness(outBr);

        uint8_t r0 = (NFC_LED_COLOR_PULSE >> 16) & 0xFF;
        uint8_t g0 = (NFC_LED_COLOR_PULSE >>  8) & 0xFF;
        uint8_t b0 = (NFC_LED_COLOR_PULSE      ) & 0xFF;

        uint32_t color = _leds->Color(r0, g0, b0);
        for (int i = 0; i < NFC_LED_COUNT; i++) {
            _leds->setPixelColor(i, color);
        }
        _leds->show();
    }



}

void loadNfcLedConfig() {
    Serial.println("Load NFC LED Config");

    if (!LittleFS.begin()) {
        Serial.println("LittleFS.begin failed");
        return;
    }

    File f = LittleFS.open("/config.json", "r");
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return;

    NFC_LED_COUNT      = doc["options"]["nfcLedCount"] | 8;
    NFC_LED_PIN        = doc["options"]["nfcLedPin"] | 15;
    NFC_LED_BRIGHTNESS = doc["options"]["nfcLedBrightness"] | 50;
    NFC_LED_TIMEOUT    = doc["options"]["nfcLedTimeout"] | 10000;

    JsonArray c = doc["options"]["nfcLedColorSuccess"];
    if (c.size() == 3) {
        NFC_LED_COLOR_SUCCESS = Adafruit_NeoPixel::Color(
            c[0].as<int>(),
            c[1].as<int>(),
            c[2].as<int>()
        );
    }

    JsonArray e = doc["options"]["nfcLedColorError"];
    if (e.size() == 3) {
        NFC_LED_COLOR_ERROR = Adafruit_NeoPixel::Color(
            e[0].as<int>(),
            e[1].as<int>(),
            e[2].as<int>()
        );
    }

    JsonArray p = doc["options"]["nfcLedColorPulse"];
    if (p.size() == 3) {
        uint8_t r = p[0].as<int>();
        uint8_t g = p[1].as<int>();
        uint8_t b = p[2].as<int>();
        NFC_LED_COLOR_PULSE = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;  // 0xRRGGBB
    }

    LEDCTRL_NFC::init(NFC_LED_COUNT, NFC_LED_PIN, NFC_LED_TIMEOUT, NFC_LED_BRIGHTNESS);

    Serial.println("NFC LED Config loaded");
    Serial.print("  NFC_LED_COUNT = "); Serial.println(NFC_LED_COUNT);
    Serial.print("  NFC_LED_PIN = "); Serial.println(NFC_LED_PIN);
    Serial.print("  NFC_LED_BRIGHTNESS = "); Serial.println(NFC_LED_BRIGHTNESS);
    Serial.print("  NFC_LED_TIMEOUT = "); Serial.println(NFC_LED_TIMEOUT);
    Serial.print("  NFC_LED_COLOR_SUCCESS = "); Serial.println(NFC_LED_COLOR_SUCCESS, HEX);
    Serial.print("  NFC_LED_COLOR_ERROR = "); Serial.println(NFC_LED_COLOR_ERROR, HEX);
    Serial.print("  NFC_LED_COLOR_PULSE = "); Serial.println(NFC_LED_COLOR_PULSE, HEX);

}
