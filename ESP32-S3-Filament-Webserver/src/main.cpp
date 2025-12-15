#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "filament_db.h"
#include "ledctrl.h"
#include "ledctrl_nfc.h"
#include "display.h"
#include "display_config.h"
#include "my_webserver.h"
#include "globals.h"
#include "display_anim.h"
#include "nfc.h"

// Reboot-Steuerung
volatile bool rebootPending = false;
unsigned long rebootAt = 0;

bool DEBUG_MODE = false;

// ----------------- Server & WS -----------------
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ----------------- OLED ---------------

DisplayType display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET_PIN);

// ----------------- PN532 SPI Settings -----------------
#define PN532_SCK 18
#define PN532_MOSI 23
#define PN532_MISO 19
#define PN532_CS 5
#define PN532_IRQ 2
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_CS);

// ----------------- I2C -----------------
#define SDA_PIN 21
#define SCL_PIN 22

// ----------------- LED & Display Timing -----------------
int targetLed = -1;
unsigned long ledStartTime = 0;


unsigned long lastTagTime = 0;
unsigned long now = 0;
bool isActive = false;




// ----------------- Globale Variablen -----------------
String activeUID = "";       // aktuell aktive UID


// ----------------- Hilfsfunktionen -----------------




void activateLed(int index) {
    if(targetLed != -1 && targetLed != index){
        LEDCTRL::setPixel(targetLed, 0); // alte LED aus
    }

    if(index >= 0 && index < LED_COUNT){
        LEDCTRL::setPixel(index, LED_COLOR);
        targetLed = index;
        ledStartTime = millis();
    } else {
        targetLed = -1;
    }

}




void handleUID(const String &uid, UidSource source) {
    lastTagTime = now;
    isActive = true;

    // Idle-Animation stoppen
    DisplayAnim::stop();

    FilamentEntry entry;
    JsonDocument doc;
    doc["uid"] = uid;

    bool isNfc = (source == UidSource::NFC);

    if (FilamentDB::findByUID(uid, entry)) {
        activateLed(entry.ledIndex);
        MYDISPLAY::show(entry);

        if (isNfc) {
            LEDCTRL_NFC::showSuccess();
        }

        doc["action"] = "knownUID";
        doc["ledIndex"] = entry.ledIndex;
        doc["vendor"] = entry.vendor;
        doc["type"] = entry.type;
        doc["color"] = entry.color;

    } else {
        if (targetLed != -1) {
            LEDCTRL::setPixel(targetLed, 0);
            targetLed = -1;
            ledStartTime = millis();
        }

        MYDISPLAY::showCentered("UNBEKANNT");

        if (isNfc) {
            LEDCTRL_NFC::showError();
        }

        doc["action"] = "unknownUID";
    }

    String msg;
    serializeJson(doc, msg);
    ws.textAll(msg);
}




// ----------------- Setup -----------------
void setup(){
    Serial.begin(115200);
    while(!Serial);

    Serial.println("Firmware Version: " FIRMWARE_VERSION);
    Serial.println("Git Hash: " GIT_HASH);

    Wire.begin(SDA_PIN,SCL_PIN);

    // ----------------- Filesystem & Webserver -----------------
    if(!LittleFS.begin(true)){
        Serial.println("LittleFS mount failed!");
        while(1);
    }

    // OLED init
    if (!initDisplay(display)) {
        Serial.println("OLED init failed!");
    while (1);
    }

    // 1. Config laden
    loadLedConfig(); // LED_COUNT und LED_PIN werden gesetzt
    loadNfcLedConfig();
    Serial.print("LED_COUNT = "); Serial.println(LED_COUNT);
    Serial.print("LED_PIN = "); Serial.println(LED_PIN);
    Serial.print("LED_COLOR = "); Serial.println(LED_COLOR);
    Serial.print("LED_TIMEOUT = "); Serial.println(LED_TIMEOUT);
    Serial.print("LED_BRIGHTNESS = "); Serial.println(LED_BRIGHTNESS);

    Serial.print("NFC_LED_COUNT = "); Serial.println(NFC_LED_COUNT);
    Serial.print("NFC_LED_PIN = "); Serial.println(NFC_LED_PIN);
    Serial.print("NFC_LED_COLOR_Success = "); Serial.println(NFC_LED_COLOR_SUCCESS);
    Serial.print("NFC_LED_COLOR_ERROR = "); Serial.println(NFC_LED_COLOR_ERROR);
    Serial.print("NFC_LED_COLOR_PULSE = "); Serial.println(NFC_LED_COLOR_PULSE); 
    Serial.print("NFC_LED_TIMEOUT = "); Serial.println(NFC_LED_TIMEOUT);
    Serial.print("NFC_LED_BRIGHTNESS = "); Serial.println(NFC_LED_BRIGHTNESS);


    // 2. LED Strip initialisieren
    LEDCTRL::init(LED_COUNT, LED_PIN, LED_TIMEOUT, LED_BRIGHTNESS);
    LEDCTRL::allOff();

    LEDCTRL_NFC::init(NFC_LED_COUNT, NFC_LED_PIN, NFC_LED_TIMEOUT, NFC_LED_BRIGHTNESS);
    LEDCTRL_NFC::allOff();

    // 3. Display & DB init
    MYDISPLAY::init(&display);
    FilamentDB::load();

    display.clearDisplay();


    MYDISPLAY::showCentered("WIFI CONNECTING...");

    // PN532 init
    NFC::init(&nfc);                   // macht begin() + SAMConfig()

    // Optional: Firmware anzeigen (Diagnose)
    uint32_t version = nfc.getFirmwareVersion();
    if (!version) {
        Serial.println("PN532 not found!");
        MYDISPLAY::showCentered("PN532 FEHLT!");
        while (1) { delay(100); }
    } else {
        Serial.print("PN532 FW "); Serial.print((version>>24)&0xFF);
        Serial.print('.');        Serial.print((version>>16)&0xFF);
        Serial.print(" chip=0x"); Serial.println(version & 0xFFFF, HEX);
    }

    // IRQ-Pin bei I2C: INPUT (schadet nicht), bei SPI meist egal
    pinMode(PN532_IRQ, INPUT);

    // ----------------- WLAN -----------------
    WiFiManager wifiManager;
    MYDISPLAY::showCentered("VERBINDUNG...");
    if(!wifiManager.autoConnect("NFC-Setup-AP")){
        ESP.restart();
    }
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    MYDISPLAY::showCentered(WiFi.localIP().toString());
    delay(5000);

    // Erst "SCAN TAG" anzeigen, dann später in die Animation wechseln
    DisplayAnim::startIdleTextFirst(millis());

    

    // WebSocket starten
    
    server.addHandler(&ws);

    // Webserver starten
    initWebServer(server, ws);

    WiFi.setSleep(false);
}



void loop() {
    now = millis();

    // 1) NFC + Guards + LED-Trigger (liefert tagPresent)
    bool tagPresent = false;
    NFC::tick(now, isActive, lastTagTime, tagPresent);

    // 2) Reboot
    if (rebootPending && now > rebootAt) {
        ESP.restart();
    }

    // 3) Display-Idle nur wenn inaktiv
    if (!isActive) {
        DisplayAnim::tickIdle(display, now);
    }

    // 4) KEIN externes LED-Timeout mehr! (LEDCTRL_NFC macht das intern)
    //    -> Keine Aufrufe von LEDCTRL_NFC::allOff() hier.

    // 5) LEDs am Ende ticken lassen
    LEDCTRL_NFC::update();

    // 6) Einfacher Übergangserkenner: !idle → idle => Display auf Idle
    static bool prevIdle = false;
    const bool idleNow = LEDCTRL_NFC::isIdle();
    if (idleNow && !prevIdle) {
        // LEDs sind in Idle übergegangen -> Display auch
        DisplayAnim::startIdleTextFirst(now);
        isActive = false;
    }
    prevIdle = idleNow;
}


