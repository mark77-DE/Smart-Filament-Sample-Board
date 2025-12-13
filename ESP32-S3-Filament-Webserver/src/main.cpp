#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <Adafruit_NeoPixel.h>
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
const unsigned long LED_TIMEOUT = 3000;
bool displayIdleShown = false;
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




void handleUID(const String &uid){
    lastTagTime = now;
    isActive = true; 

    // Idle-Animation stoppen, weil jetzt aktiv etwas angezeigt wird
    DisplayAnim::stop();


    FilamentEntry entry;

    DynamicJsonDocument doc(256);
    doc["uid"] = uid;

    if(FilamentDB::findByUID(uid, entry)){
        // Bekannte UID
        activateLed(entry.ledIndex);
        MYDISPLAY::show(entry);

        doc["action"] = "knownUID";
        doc["ledIndex"] = entry.ledIndex;
        doc["vendor"] = entry.vendor;
        doc["type"] = entry.type;
        doc["color"] = entry.color;

    } else {
        // Unbekannt → LEDs zurücksetzen
        if(targetLed != -1){
            LEDCTRL::setPixel(targetLed, 0);
            targetLed = -1;
            ledStartTime = millis();
        }
        MYDISPLAY::showCentered("UNBEKANNT");

        // WebUI informieren
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
    loadConfig(); // LED_COUNT und LED_PIN werden gesetzt
    loadNfcLedConfig();
    Serial.print("LED_COUNT = "); Serial.println(LED_COUNT);
    Serial.print("LED_PIN = "); Serial.println(LED_PIN);
    Serial.print("LED_COLOR = "); Serial.println(LED_COLOR);
    Serial.print("NFC_LED_COUNT = "); Serial.println(NFC_LED_COUNT);
    Serial.print("NFC_LED_PIN = "); Serial.println(NFC_LED_PIN);
    Serial.print("NFC_LED_COLOR = "); Serial.println(NFC_LED_COLOR);



    // 2. LED Strip initialisieren
    LEDCTRL::init(LED_COUNT, LED_PIN);
    LEDCTRL::allOff();

    LEDCTRL_NFC::init(NFC_LED_COUNT, NFC_LED_PIN);
    LEDCTRL_NFC::allOff();

    // 3. Display & DB init
    MYDISPLAY::init(&display);
    FilamentDB::load();

    display.clearDisplay();


    MYDISPLAY::showCentered("WIFI CONNECTING...");

    // PN532 init
    nfc.begin();
    uint32_t versiondata = nfc.getFirmwareVersion();
    if(!versiondata){
        Serial.println("PN532 not found!");
        MYDISPLAY::showCentered("PN532 FEHLT!");
        while(1);
    }
    nfc.SAMConfig();
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

// ----------------- Loop -----------------
void loop(){
    now = millis();

    // Idle-Animation nur laufen lassen, wenn das System nicht aktiv ist
    if (!isActive) {
        DisplayAnim::tickIdle(display, now);
    }

    // NFC Lesen
    uint8_t uid[7]; uint8_t uidLength = 0;
    nfc.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A);
    if(nfc.readDetectedPassiveTargetID(uid, &uidLength) && uidLength > 0){
        String uidStr;
        for(uint8_t i=0;i<uidLength;i++){
            if(uid[i]<0x10) uidStr += "0";
            uidStr += String(uid[i], HEX);
            if(i != uidLength-1) uidStr += ":";
        }
        uidStr.toUpperCase();
        Serial.println("FOUND UID: " + uidStr);
        handleUID(uidStr);
        nfc.SAMConfig();
    }   

    // LED Timeout
    if (now - lastTagTime > LED_TIMEOUT && isActive) {
        targetLed = -1;
        LEDCTRL::allOff();   // <-- alles über LEDCTRL
        Serial.println("LED Timeout - alle LEDs aus");
        Serial.println("Display idle");


        DisplayAnim::startIdleTextFirst(now);

        isActive = false;
    }

    

    
    delay(10);
}