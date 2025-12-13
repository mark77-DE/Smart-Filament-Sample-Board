#include <ArduinoJson.h>
#include <LittleFS.h>
#include "ledctrl_nfc.h"

int NFC_LED_COUNT = 0;
int NFC_LED_PIN = 0;
int NFC_LED_BRIGHTNESS = 50;
uint32_t NFC_LED_COLOR = 0x00FF00; // Standard grün

Adafruit_NeoPixel* LEDCTRL_NFC::_leds = nullptr;

void LEDCTRL_NFC::init(int count, int pin) {
    NFC_LED_COUNT = count;
    NFC_LED_PIN = pin;

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

void loadNfcLedConfig() {
    if (!LittleFS.begin()) {
        Serial.println("LittleFS.begin failed");
        return;
    }

    File f = LittleFS.open("/config.json", "r");
    if (!f) return;

    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return;

    NFC_LED_COUNT      = doc["options"]["nfcLedCount"] | 8;
    NFC_LED_PIN        = doc["options"]["nfcLedPin"] | 6;
    NFC_LED_BRIGHTNESS = doc["options"]["nfcLedBrightness"] | 60;

    JsonArray c = doc["options"]["nfcLedColor"];
    if (c.size() == 3) {
        NFC_LED_COLOR = Adafruit_NeoPixel::Color(
            c[0].as<int>(),
            c[1].as<int>(),
            c[2].as<int>()
        );
    } else {
        NFC_LED_COLOR = Adafruit_NeoPixel::Color(0, 255, 0);
    }
}

