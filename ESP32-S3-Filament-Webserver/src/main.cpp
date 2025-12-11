#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <ArduinoJson.h>
#include "filament_db.h"
#include "ledctrl.h"
#include "display.h"

#include <Fonts/FreeMono7pt7b.h>
#include "my_webserver.h"

#include <LittleFS.h>
#include "globals.h"

// ----------------- Server & WS -----------------
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ----------------- OLED Settings -----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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
void showCentered(const String &msg){
    display.clearDisplay();
    display.setFont(&FreeMono7pt7b);
    display.setTextSize(1);
    int16_t x1, y1; uint16_t w,h;
    display.getTextBounds(msg,0,0,&x1,&y1,&w,&h);
    display.setCursor((SCREEN_WIDTH - w)/2,(SCREEN_HEIGHT - h)/2 + 2);
    display.print(msg);
    display.display();
}

// ----------------- LED & Display -----------------
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
        showCentered("UNBEKANNT");

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
    if(!LittleFS.begin()){
        Serial.println("LittleFS mount failed!");
        while(1);
    }

    // OLED init
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)){
        Serial.println("OLED init failed!");
        while(1);
    }

    // 1. Config laden
    loadConfig(); // LED_COUNT und LED_PIN werden gesetzt
    Serial.print("LED_COUNT = "); Serial.println(LED_COUNT);

    // 2. LED Strip initialisieren
    LEDCTRL::init(LED_COUNT, LED_PIN);
    LEDCTRL::allOff();

    // 3. Display & DB init
    MYDISPLAY::init(&display);
    FilamentDB::load();

    display.clearDisplay();
    display.setFont(&FreeMono7pt7b);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    showCentered("WIFI CONNECTING...");

    // PN532 init
    nfc.begin();
    uint32_t versiondata = nfc.getFirmwareVersion();
    if(!versiondata){
        Serial.println("PN532 not found!");
        showCentered("PN532 FEHLT!");
        while(1);
    }
    nfc.SAMConfig();
    pinMode(PN532_IRQ, INPUT);

    // ----------------- WLAN -----------------
    WiFiManager wifiManager;
    showCentered("VERBINDUNG...");
    if(!wifiManager.autoConnect("NFC-Setup-AP")){
        ESP.restart();
    }
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    showCentered(WiFi.localIP().toString());
    delay(5000);
    showCentered("SCAN TAG");

    // WebSocket starten
    server.addHandler(&ws);

    // Webserver starten
    initWebServer(server, ws);
}

// ----------------- Loop -----------------
void loop(){
    now = millis();

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
        showCentered("SCAN TAG");
        isActive = false;
    }

    delay(10);
}
