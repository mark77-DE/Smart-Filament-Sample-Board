#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Adafruit_NeoPixel.h>
#include "ledctrl_nfc.h"

int NFC_LED_COUNT = 0;
int NFC_LED_PIN = 15;
int NFC_LED_BRIGHTNESS = 50;
unsigned long NFC_LED_TIMEOUT = 20000; // 2 Sekunden

bool idlePulseEnabled = true;        // Pulsen im Idle
float pulsePhase = 0;                // Laufender Phasenwert
float pulseSpeed = 0.2;             // Geschwindigkeit der Sinuskurve
float minBrightness = 0.15f;   // 15 % Mindesthelligkeit

uint32_t NFC_LED_COLOR_SUCCESS = 0x00FF00; // grün
uint32_t NFC_LED_COLOR_ERROR   = 0xFF0000; // rot

uint32_t NFC_LED_COLOR_PULSE = 0x000066; // grün

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
    for (int i = 0; i < NFC_LED_COUNT; i++) {
        _leds->setPixelColor(i, NFC_LED_COLOR_SUCCESS);
    }
    pulsePhase = 0;
    _leds->show();
}

void LEDCTRL_NFC::showError() {
    currentState = LED_ERROR;
    stateStartTime = millis();
    for (int i = 0; i < NFC_LED_COUNT; i++) {
        _leds->setPixelColor(i, NFC_LED_COLOR_ERROR);
    }
    pulsePhase = 0;
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
        }
        return; // während Success/Error keine Idle-Animation
    }

    // Idle-Puls
    if (idlePulseEnabled) {
        // Sinuswert zwischen 0 und 1
        float raw = (sin(pulsePhase) + 1.0) / 2.0;   // 0..1
        float brightnessFactor = minBrightness + raw * (1.0f - minBrightness);

        pulsePhase += pulseSpeed;

        // RGB-Komponenten extrahieren
        uint8_t r = (NFC_LED_COLOR_PULSE >> 16) & 0xFF;
        uint8_t g = (NFC_LED_COLOR_PULSE >> 8)  & 0xFF;
        uint8_t b = (NFC_LED_COLOR_PULSE)       & 0xFF;

        // Mit Helligkeitsfaktor multiplizieren
        r = (uint8_t)(r * brightnessFactor);
        g = (uint8_t)(g * brightnessFactor);
        b = (uint8_t)(b * brightnessFactor);

        // Neue Farbe erzeugen
        uint32_t color = _leds->Color(r, g, b);

        // Alle LEDs setzen
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

    LEDCTRL_NFC::init(NFC_LED_COUNT, NFC_LED_PIN, NFC_LED_TIMEOUT, NFC_LED_BRIGHTNESS);

    Serial.println("NFC LED Config loaded");
    Serial.print("  NFC_LED_COUNT = "); Serial.println(NFC_LED_COUNT);  
    Serial.print("  NFC_LED_PIN = "); Serial.println(NFC_LED_PIN);
    Serial.print("  NFC_LED_BRIGHTNESS = "); Serial.println(NFC_LED_BRIGHTNESS);
    Serial.print("  NFC_LED_TIMEOUT = "); Serial.println(NFC_LED_TIMEOUT);
    Serial.print("  NFC_LED_COLOR_SUCCESS = "); Serial.println(NFC_LED_COLOR_SUCCESS, HEX);
    Serial.print("  NFC_LED_COLOR_ERROR = "); Serial.println(NFC_LED_COLOR_ERROR, HEX);
}
