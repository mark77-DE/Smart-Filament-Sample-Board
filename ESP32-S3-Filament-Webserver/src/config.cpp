#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ledctrl_filament.h"
#include "ledctrl_nfc.h"
#include "gpio_hardware.h"   // für gpiohw_init()


// ============================================================================
// Laden / Anwenden der Konfiguration
// ============================================================================

bool loadConfigV2()
{
    // LittleFS mounten (mit Format-on-fail = true, wie bisher genutzt)
    Serial.println("Mounting LittleFS...");
    if (!LittleFS.begin(true))
    {

        Serial.println(F("LittleFS mount failed!"));

        // unverändertes Verhalten: blockieren
        while (1)
        {
            delay(10);
        }
    }

    if (!LittleFS.exists("/config_v2.json"))
        return false;

    File f = LittleFS.open("/config_v2.json", "r");
    if (!f)
        return false;

    JsonDocument doc;
    auto err = deserializeJson(doc, f);
    f.close();
    if (err)
        return false;

    if (!doc["options"].is<JsonObject>())
        return false;
    JsonObject opt = doc["options"].as<JsonObject>();

    // --- Basis-Flags ---
    CONFIGV2.system.darkmode = opt["darkmode"] | false;
    CONFIGV2.mqttConfig.enabled = opt["mqtt"] | false;
    CONFIGV2.system.debugMode = opt["debugMode"] | false;
    CONFIGV2.system.hostname = opt["hostname"] | "FiSaBo";

    // --- Filament-LED ---
    CONFIGV2.led.count = opt["ledCount"] | 8;
    CONFIGV2.led.pin = opt["ledPin"] | 4;
    CONFIGV2.led.brightness = opt["ledBrightness"] | 50;
    CONFIGV2.led.timeout = opt["ledTimeout"] | 3000;
    // --- Dashboard (Virtuelle LED)---
    CONFIGV2.system.webLEDTimeout = opt["webLEDTimeout"] | (uint32_t)CONFIGV2.led.timeout; // Fallback auf ledTimeout

    if (opt["ledColor"].is<JsonArray>())
    {
        JsonArray c = opt["ledColor"];
        CONFIGV2.led.color = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
    }
    else
    {
        CONFIGV2.led.color = 0xFFFFFF;
    }

    if (opt["ledColorError"].is<JsonArray>())
    {
        JsonArray e = opt["ledColorError"];
        CONFIGV2.led.colorError = ((uint32_t)e[0] << 16) | ((uint32_t)e[1] << 8) | (uint32_t)e[2];
    }
    else
    {
        CONFIGV2.led.colorError = 0xFF0000;
    }

    if (opt["ledColorPulse"].is<JsonArray>())
    {
        JsonArray p = opt["ledColorPulse"];
        CONFIGV2.led.colorPulse = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
    }
    else
    {
        CONFIGV2.led.colorPulse = 0x0000FF;
    }

    // --- NFC-LED ---
    CONFIGV2.nfc.count = opt["nfcLedCount"] | 8;
    CONFIGV2.nfc.pin = opt["nfcLedPin"] | 15;
    CONFIGV2.nfc.brightness = opt["nfcLedBrightness"] | 100;
    CONFIGV2.nfc.timeout = opt["nfcLedTimeout"] | 4000;

    if (opt["nfcLedColorSuccess"].is<JsonArray>())
    {
        JsonArray c = opt["nfcLedColorSuccess"];
        CONFIGV2.nfc.colorSuccess = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
    }
    else
    {
        CONFIGV2.nfc.colorSuccess = 0xFFFF00;
    }

    if (opt["nfcLedColorError"].is<JsonArray>())
    {
        JsonArray c = opt["nfcLedColorError"];
        CONFIGV2.nfc.colorError = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
    }
    else
    {
        CONFIGV2.nfc.colorError = 0x00FFFF;
    }

    if (opt["nfcLedColorPulse"].is<JsonArray>())
    {
        JsonArray c = opt["nfcLedColorPulse"];
        CONFIGV2.nfc.colorPulse = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
    }
    else
    {
        CONFIGV2.nfc.colorPulse = 0xFF00FF;
    }

    CONFIGV2.nfc.successBlinkEnabled = opt["nfcLedSuccessBlinkEnabled"] | true;
    CONFIGV2.nfc.successBlinkCount = opt["nfcLedSuccessBlinkCount"] | 3;
    CONFIGV2.nfc.successBlinkMs = opt["nfcLedSuccessBlinkMs"] | 150;

    // --- Button ---
    CONFIGV2.button.pin = opt["buttonPin"] | CONFIGV2.button.pin;
    CONFIGV2.button.pullup = opt["buttonPullup"] | CONFIGV2.button.pullup;
    CONFIGV2.button.debounceMs = opt["buttonDebounceMs"] | CONFIGV2.button.debounceMs;
    CONFIGV2.button.longMs = opt["buttonLongMs"] | CONFIGV2.button.longMs;
    CONFIGV2.button.doubleGapMs = opt["buttonDoubleMs"] | CONFIGV2.button.doubleGapMs;
    CONFIGV2.button.holdRepeatMs = opt["buttonHoldMs"] | CONFIGV2.button.holdRepeatMs;

    // --- Buzzer ---
    CONFIGV2.buzzer.pin = opt["buzzerPin"] | CONFIGV2.buzzer.pin;
    CONFIGV2.buzzer.activeHigh = opt["buzzerActiveHigh"] | CONFIGV2.buzzer.activeHigh;
    CONFIGV2.buzzer.passive = opt["buzzerPassive"] | CONFIGV2.buzzer.passive;
    CONFIGV2.buzzer.freqHz = opt["buzzerFreq"] | CONFIGV2.buzzer.freqHz;
    CONFIGV2.buzzer.singleMs = opt["buzzerSingleMs"] | CONFIGV2.buzzer.singleMs;
    CONFIGV2.buzzer.doubleOnMs = opt["buzzerDoubleOnMs"] | CONFIGV2.buzzer.doubleOnMs;
    CONFIGV2.buzzer.doubleGapMs = opt["buzzerDoubleGapMs"] | CONFIGV2.buzzer.doubleGapMs;
    CONFIGV2.buzzer.errorOnMs = opt["buzzerErrorOnMs"] | CONFIGV2.buzzer.errorOnMs;
    CONFIGV2.buzzer.errorGapMs = opt["buzzerErrorGapMs"] | CONFIGV2.buzzer.errorGapMs;
    CONFIGV2.buzzer.errorCount = opt["buzzerErrorCount"] | CONFIGV2.buzzer.errorCount;

    // Filament-DB laden & Konfiguration anwenden
    loadFilaments();
    return true;
}

//----------------------------------------------------------------------------
// Anwenden der geladenen Konfiguration (vor allem Hardware-bezogen)
//----------------------------------------------------------------------------

void applyConfigV2() {
  // Filament-LEDs
  LEDCTRL_FILAMENT::init(
    CONFIGV2.led.count,
    CONFIGV2.led.pin,
    CONFIGV2.led.timeout,
    CONFIGV2.led.brightness,
    CONFIGV2.led.color,
    CONFIGV2.led.colorError,
    CONFIGV2.led.colorPulse
  );

  // NFC-LEDs
  LEDCTRL_NFC::init(
    CONFIGV2.nfc.count,
    CONFIGV2.nfc.pin,
    CONFIGV2.nfc.timeout,
    CONFIGV2.nfc.brightness,
    CONFIGV2.nfc.colorSuccess,
    CONFIGV2.nfc.colorError,
    CONFIGV2.nfc.colorPulse,
    CONFIGV2.nfc.successBlinkEnabled,
    CONFIGV2.nfc.successBlinkCount,
    CONFIGV2.nfc.successBlinkMs
  );

  // GPIO-Hardware (Button/Buzzer)
  gpiohw_init();  // liest CONFIG.button / CONFIG.buzzer, richtet Pins & ISR/Timer ein

  if (CONFIGV2.system.debugMode) {
    Serial.println(F("--------------------"));
    Serial.println(F("Config applied:"));

    Serial.print(F(" HOSTNAME = "));       Serial.println(CONFIGV2.system.hostname);

    Serial.print(F(" LED_COUNT = "));         Serial.println(CONFIGV2.led.count);
    Serial.print(F(" LED_PIN = "));           Serial.println(CONFIGV2.led.pin);
    Serial.print(F(" LED_TIMEOUT = "));       Serial.println(CONFIGV2.led.timeout);
    Serial.print(F(" LED_BRIGHTNESS = "));    Serial.println(CONFIGV2.led.brightness);
    Serial.print(F(" LED_COLOR = 0x"));       Serial.println(CONFIGV2.led.color, HEX);
    Serial.print(F(" LED_COLOR_ERROR = 0x")); Serial.println(CONFIGV2.led.colorError, HEX);
    Serial.print(F(" LED_COLOR_PULSE = 0x")); Serial.println(CONFIGV2.led.colorPulse, HEX);

    Serial.print(F(" WEB_LED_TIMEOUT = ")); Serial.println(CONFIGV2.webLEDTimeout);


    Serial.print(F(" NFC_LED_COUNT = "));     Serial.println(CONFIGV2.nfc.count);
    Serial.print(F(" NFC_LED_PIN = "));       Serial.println(CONFIGV2.nfc.pin);
    Serial.print(F(" NFC_LED_TIMEOUT = "));   Serial.println(CONFIGV2.nfc.timeout);
    Serial.print(F(" NFC_LED_BRIGHTNESS = "));Serial.println(CONFIGV2.nfc.brightness);
    Serial.print(F(" NFC_LED_COLOR_SUCCESS = 0x")); Serial.println(CONFIGV2.nfc.colorSuccess, HEX);
    Serial.print(F(" NFC_LED_COLOR_ERROR = 0x"));   Serial.println(CONFIGV2.nfc.colorError, HEX);
    Serial.print(F(" NFC_LED_COLOR_PULSE = 0x"));   Serial.println(CONFIGV2.nfc.colorPulse, HEX);
    Serial.print(F(" DEBUG_MODE = "));        Serial.println(CONFIGV2.system.debugMode ? F("true") : F("false"));

    Serial.print(F(" BUTTON pin="));          Serial.print(CONFIGV2.button.pin);
    Serial.print(F(" pullup="));              Serial.print(CONFIGV2.button.pullup);
    Serial.print(F(" debounce="));            Serial.print(CONFIGV2.button.debounceMs);
    Serial.print(F("ms long="));              Serial.print(CONFIGV2.button.longMs);
    Serial.print(F("ms double="));            Serial.print(CONFIGV2.button.doubleGapMs);
    Serial.print(F("ms holdRep="));           Serial.print(CONFIGV2.button.holdRepeatMs);
    Serial.println(F("ms"));

    Serial.print(F(" BUZZER pin="));          Serial.print(CONFIGV2.buzzer.pin);
    Serial.print(F(" activeHigh="));          Serial.print(CONFIGV2.buzzer.activeHigh);
    Serial.print(F(" freq="));                Serial.print(CONFIGV2.buzzer.freqHz);
    Serial.print(F("Hz single="));            Serial.print(CONFIGV2.buzzer.singleMs);
    Serial.print(F("ms dblOn="));             Serial.print(CONFIGV2.buzzer.doubleOnMs);
    Serial.print(F("ms dblGap="));            Serial.print(CONFIGV2.buzzer.doubleGapMs);
    Serial.print(F("ms errOn="));             Serial.print(CONFIGV2.buzzer.errorOnMs);
    Serial.print(F("ms errGap="));            Serial.print(CONFIGV2.buzzer.errorGapMs);
    Serial.print(F("ms errCount="));          Serial.println(CONFIG.buzzer.errorCount);

    Serial.println(F("--------------------"));
  }
}