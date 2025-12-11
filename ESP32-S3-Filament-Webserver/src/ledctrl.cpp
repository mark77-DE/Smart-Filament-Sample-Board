#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include "ledctrl.h"

Adafruit_NeoPixel* LEDCTRL::_leds = nullptr;

int LED_COUNT = 4;
int LED_PIN = 4;       // aus config.json
int LED_BRIGHTNESS = 50;
uint32_t LED_COLOR = 0xFF0000;  // default Rot


void loadConfig() {
    if (!LittleFS.begin()) {
        Serial.println("LittleFS.begin() failed!");
        return;
    }

    File f;
    String filename = LittleFS.exists("/config.json") ? "/config.json" : "/filament_default.json";
    f = LittleFS.open(filename, "r");

    if (!f) {
        Serial.println("Failed to open " + filename);
        return;
    }

    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.println("JSON parse failed: " + String(err.c_str()));
        return;
    }

    LED_COUNT = doc["options"]["ledCount"] | 8;
    LED_PIN   = doc["options"]["ledPin"] | 5;
    LED_BRIGHTNESS = doc["options"]["ledBrightness"] | 50;

    JsonArray colorArr = doc["options"]["ledColor"];
    if (colorArr.size() == 3) {
        LED_COLOR = Adafruit_NeoPixel::Color(
            colorArr[0].as<int>(),
            colorArr[1].as<int>(),
            colorArr[2].as<int>()
        );
        Serial.printf("Loaded LED color from config: R=%d, G=%d, B=%d\n",
                      colorArr[0].as<int>(),
                      colorArr[1].as<int>(),
                      colorArr[2].as<int>());
    } else {
        LED_COLOR = Adafruit_NeoPixel::Color(255, 0, 0);
    }

    Serial.printf("LED config loaded: count=%d, pin=%d, brightness=%d, color=0x%06X\n",
                  LED_COUNT, LED_PIN, LED_BRIGHTNESS, LED_COLOR);
}



// init jetzt dynamisch
void LEDCTRL::init(int count, int pin) {
    if (_leds) delete _leds; // alte Instanz löschen, falls vorhanden

    _leds = new Adafruit_NeoPixel(count, pin, NEO_GRB + NEO_KHZ800);
    _leds->begin();
    _leds->show();
}

// highlight z. B. mit konfigurierter Farbe und Helligkeit
void LEDCTRL::highlight(int index) {
    if (!_leds || index < 0 || index >= _leds->numPixels()) return;
    _leds->clear();
    uint32_t col = LED_COLOR;
    _leds->setPixelColor(index, col);
    _leds->setBrightness(LED_BRIGHTNESS);
    _leds->show();
}

void LEDCTRL::allOff() {
    if (!_leds) return;
    _leds->clear();
    _leds->show();
}

