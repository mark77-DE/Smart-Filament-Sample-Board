#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <Adafruit_NeoPixel.h>
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

// ----------------- LEDs -----------------
#define LED_PIN 4
#define LED_COUNT 8
Adafruit_NeoPixel leds(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
#define LED_COLOR_R 255
#define LED_COLOR_G 0
#define LED_COLOR_B 0
#define LED_BRIGHTNESS 32

#define SDA_PIN 21
#define SCL_PIN 22

// ----------------- LED & Display Timing -----------------
//int targetLed = -1;
//unsigned long lastTagTime = 0;
const unsigned long LED_TIMEOUT = 3000;
bool displayIdleShown = false;

// ----------------- Globale Variablen -----------------
String activeUID = "";       // aktuell aktive UID
unsigned long ledStartTime = 0;  // Startzeit der LED

// ----------------- Hilfsfunktionen -----------------



// ----------------- LED & Display -----------------
void setLedBrightness(int index, int brightness){
    uint8_t r = (LED_COLOR_R * brightness) / 255;
    uint8_t g = (LED_COLOR_G * brightness) / 255;
    uint8_t b = (LED_COLOR_B * brightness) / 255;

    for(int i=0;i<LED_COUNT;i++){
        if(i == index) leds.setPixelColor(i,r,g,b);
        else leds.setPixelColor(i,0);
    }
    leds.show();
}

void showCentered(const String &msg){
    display.clearDisplay();
    display.setFont(&FreeMono7pt7b);
    display.setTextSize(1);
    int16_t x1, y1; uint16_t w,h;
    display.getTextBounds(msg,0,0,&x1,&y1,&w,&h);
    display.setCursor((SCREEN_WIDTH - w)/2,(SCREEN_HEIGHT - h)/2);
    display.print(msg);
    display.display();
}

// ----------------- Zentrale UID-Aktivierung -----------------
void setActiveUID(const String &uid){
    if(uid != activeUID){
        // alte LED löschen
        if(targetLed != -1){
            leds.clear();
            leds.show();
            targetLed = -1;
        }

        FilamentEntry entry;
        if(FilamentDB::findByUID(uid, entry) && entry.ledIndex >= 0 && entry.ledIndex < LED_COUNT){
            targetLed = entry.ledIndex;
            setLedBrightness(targetLed, LED_BRIGHTNESS);
            MYDISPLAY::show(entry);
        } else {
            targetLed = -1;
            showCentered("UNBEKANNT");
        }

        activeUID = uid;
        ledStartTime = millis();
        displayIdleShown = false;
    } else {
        // gleiche UID -> Timer neu starten
        ledStartTime = millis();
    }

    // --- Web UI informieren ---
    DynamicJsonDocument doc(256);
    doc["uid"] = uid;
    String msg;
    serializeJson(doc,msg);
    ws.textAll(msg);
}


void activateLed(int index){
    if(index < 0 || index >= LED_COUNT) return;

    // Alte LED löschen, falls eine andere aktiv ist
    if(targetLed != -1 && targetLed != index){
        setLedBrightness(targetLed, 0);
        leds.show();
    }

    targetLed = index;
    setLedBrightness(targetLed, LED_BRIGHTNESS);
    leds.show();
    lastTagTime = millis(); // Timer für Timeout
}


void notifyUID(const String &uid){
    FilamentEntry entry;
    bool known = FilamentDB::findByUID(uid, entry);

    if(known && entry.ledIndex >= 0 && entry.ledIndex < LED_COUNT){
        targetLed = entry.ledIndex;
        setLedBrightness(targetLed, LED_BRIGHTNESS); // LED einschalten
        lastTagTime = millis(); // Timer starten
        MYDISPLAY::show(entry); // Display aktualisieren
    } else {
        targetLed = -1;
        leds.clear(); leds.show();
        showCentered("UNBEKANNT");
        lastTagTime = millis();
    }

    // --- Web UI informieren ---
    DynamicJsonDocument doc(256);
    doc["uid"] = uid;
    String msg;
    serializeJson(doc,msg);
    ws.textAll(msg);
}







// ----------------- Setup -----------------
void setup(){
    Serial.begin(115200);
    while(!Serial);

    Wire.begin(SDA_PIN,SCL_PIN);

    // OLED init
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)){
        Serial.println("OLED init failed!");
        while(1);
    }

    LEDCTRL::init(&leds);
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

    // ----------------- Filesystem & Webserver -----------------
    if(!LittleFS.begin()){
        Serial.println("LittleFS mount failed!");
        while(1);
    }

    // WebSocket starten
    
    server.addHandler(&ws);

    // Webserver starten
    initWebServer(server, ws);
}

void loop(){
    unsigned long now = millis();

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
        setActiveUID(uidStr);
        nfc.SAMConfig();
    }

    // LED Timeout
    if(targetLed != -1 && now - ledStartTime > LED_TIMEOUT){
        leds.clear(); leds.show();
        targetLed = -1;
        activeUID = "";
        displayIdleShown = false;
    }

    // Display Timeout / Idle
    if(targetLed == -1 && !displayIdleShown){
        showCentered("SCAN TAG");
        displayIdleShown = true;
    }

    delay(10);
}