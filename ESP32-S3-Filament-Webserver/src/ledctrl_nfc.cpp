#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Adafruit_NeoPixel.h>
#include "ledctrl_nfc.h"

int NFC_LED_COUNT = 0;
int NFC_LED_PIN = 15;
int NFC_LED_BRIGHTNESS = 50;
unsigned long NFC_LED_TIMEOUT = 20000; // 2 Sekunden

bool idlePulseEnabled = true;       // Pulsen im Idle

// Diese Variablen bleiben aus Kompatibilitätsgründen erhalten.
// Der neue Puls ist zeitbasiert, daher werden pulsePhase/pulseSpeed im Idle-Puls nicht mehr verwendet.
float pulsePhase = 0;               // (legacy) Laufender Phasenwert
float pulseSpeed = 0.2;             // (legacy) Geschwindigkeit der Sinuskurve

float minBrightness = 0.35f;        // Mindesthelligkeit

uint32_t NFC_LED_COLOR_SUCCESS = 0x00FF00; // grün
uint32_t NFC_LED_COLOR_ERROR   = 0xFF0000; // rot

uint32_t NFC_LED_COLOR_PULSE = 0x0033AA;   // statt 0x000066 (deutlich smoother)

// ---------- Idle-Puls Feinschliff (nur dafür neu) ----------
// FPS-Limit sorgt für gleichmäßige Schritte und weniger "Jitter" durch variable loop()-Laufzeiten.
static unsigned long s_lastPulseUpdate = 0;
static const uint16_t PULSE_INTERVAL_MS = 16;    // ~60 FPS
static const float    PULSE_PERIOD_MS   = 2800.0f; // Pulsdauer in Millisekunden (1 kompletter "atmen"-Zyklus). Anpassen nach Geschmack.

// Temporal Dithering: sammelt Nachkommastellen über Frames
static float s_ditherAccR = 0.0f;
static float s_ditherAccG = 0.0f;
static float s_ditherAccB = 0.0f;

// Dithering für globale Brightness (0..255)
static float s_ditherAccBr = 0.0f;
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
    pulsePhase = 0; // legacy: bleibt, schadet nicht
    _leds->show();
}

void LEDCTRL_NFC::showError() {
    currentState = LED_ERROR;
    stateStartTime = millis();
    _leds->setBrightness(NFC_LED_BRIGHTNESS); // zurück auf Basishelligkeit
    for (int i = 0; i < NFC_LED_COUNT; i++) {
        _leds->setPixelColor(i, NFC_LED_COLOR_ERROR);
    }
    pulsePhase = 0; // legacy: bleibt, schadet nicht
    _leds->show();
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


    // Idle-Puls (super smooth: Puls über globale Brightness + temporal dithering)
    if (idlePulseEnabled) {

        // FPS-Limit -> gleichmäßige Updates
        if (now - s_lastPulseUpdate < PULSE_INTERVAL_MS) return;
        s_lastPulseUpdate = now;

        // Phase 0..1 (zeitbasiert)
        float phase01 = fmodf((float)now, PULSE_PERIOD_MS) / PULSE_PERIOD_MS;

        // Sinus 0..1
        float raw = (sinf(phase01 * 2.0f * PI) + 1.0f) * 0.5f;

        // Mindesthelligkeit + Amplitude
        float brightnessFactor = minBrightness + raw * (1.0f - minBrightness);

        // Ziel-Brightness (0..NFC_LED_BRIGHTNESS), gamma-korrigiert
        // Idee: wir nehmen NFC_LED_BRIGHTNESS als "Max", und pulsieren darunter.
        float target = (float)NFC_LED_BRIGHTNESS * brightnessFactor; // 0..NFC_LED_BRIGHTNESS

        // Gamma auf die Brightness anwenden (bessere visuelle Gleichmäßigkeit)
        // gamma8 erwartet 0..255, daher clampen und casten wir erst grob:
        uint8_t target8 = (uint8_t)constrain((int)lroundf(target), 0, 255);
        uint8_t gammaBr = Adafruit_NeoPixel::gamma8(target8);

        // Temporal Dithering auf Brightness (macht Zwischenstufen über Zeit sichtbar)
        // Wir dithern um den gamma-korrigierten Wert herum (wichtig!)
        float v = (float)gammaBr;
        float baseF = floorf(v);
        float frac  = v - baseF;

        s_ditherAccBr += frac;
        uint8_t outBr = (uint8_t)baseF;
        if (s_ditherAccBr >= 1.0f) {
            outBr = (uint8_t)min(255.0f, baseF + 1.0f);
            s_ditherAccBr -= 1.0f;
        }

        // Farbe bleibt konstant, nur Brightness pulsiert
        _leds->setBrightness(outBr);

        uint8_t r0 = (NFC_LED_COLOR_PULSE >> 16) & 0xFF;
        uint8_t g0 = (NFC_LED_COLOR_PULSE >>  8) & 0xFF;
        uint8_t b0 = (NFC_LED_COLOR_PULSE      ) & 0xFF;
        uint32_t color = _leds->Color(r0, g0, b0);

        for (int i = 0; i < NFC_LED_COUNT; i++) {
            _leds->setPixelColor(i, color);
        }
        _leds->show();

        // Wichtig: wenn später Success/Error kommt, setzt du da wieder setBrightness(NFC_LED_BRIGHTNESS)
        // -> machen wir in showSuccess/showError minimal dazu (siehe unten)
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
