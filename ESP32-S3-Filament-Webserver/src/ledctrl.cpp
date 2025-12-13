#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include "ledctrl.h"

int LED_COUNT = 0;
int LED_PIN = 4;
int LED_BRIGHTNESS = 50; // default
int LED_TIMEOUT = 3000;

uint32_t LED_COLOR = 0xFF0000; // Standard: rot



Adafruit_NeoPixel* LEDCTRL::_leds = nullptr;

void LEDCTRL::init(int count, int pin){
    LED_COUNT = count;
    LED_PIN = pin;

    if(_leds) delete _leds; // evtl. alten Strip löschen
    _leds = new Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
    _leds->begin();
    _leds->setBrightness(LED_BRIGHTNESS); // <- Helligkeit anwenden

    allOff();
}

void LEDCTRL::setPixel(int index, uint32_t color){

    Serial.printf("Set LED %d to color 0x%06X\n", index, color);

    if(!_leds) return;
    if(index < 0 || index >= LED_COUNT) return;

    _leds->setPixelColor(index, color);
    _leds->show();
}

void LEDCTRL::allOff(){
    if(!_leds) return;

    for(int i=0;i<LED_COUNT;i++){
        _leds->setPixelColor(i, 0);
    }
    _leds->show();
}


void loadLedConfig() {
    if (!LittleFS.begin()) {
        Serial.println("LittleFS.begin() failed!");
        return;
    }

    String filename = LittleFS.exists("/config.json") ? "/config.json" : "/filament_default.json";
    File f = LittleFS.open(filename, "r");
    if (!f) {
        Serial.println("Failed to open " + filename);
        return;
    }

    // <-- hier ändern -->
    DynamicJsonDocument doc(2048); // korrekt: DynamicJsonDocument mit Größe
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.println("JSON parse failed: " + String(err.c_str()));
        return;
    }

    LED_COUNT = doc["options"]["ledCount"] | 8;
    LED_PIN   = doc["options"]["ledPin"] | 5;
    LED_BRIGHTNESS = doc["options"]["ledBrightness"] | 50;
    LED_TIMEOUT = doc["options"]["ledTimeout"] | 3000;

    JsonArray colorArr = doc["options"]["ledColor"];
    if (colorArr.size() == 3) {
        LED_COLOR = Adafruit_NeoPixel::Color(
            colorArr[0].as<int>(),
            colorArr[1].as<int>(),
            colorArr[2].as<int>()
        );
        
    } else {
        LED_COLOR = Adafruit_NeoPixel::Color(255, 0, 0);
    }

    
}