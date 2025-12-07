#include <WiFi.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "filament_db.h"
#include "ledctrl.h"
#include "display.h"

#include <Fonts/FreeMono7pt7b.h>



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



// ----------------- Auto-Off & Fade -----------------
int targetLed = -1;                 
int ledBrightness = 0;              
bool fadingIn = false;
unsigned long lastFadeTime = 0;

unsigned long lastTagTime = 0;      
const unsigned long LED_TIMEOUT = 3000; 
const int fadeStep = 16;            
const int fadeDelay = 15;           

// ----------------- Hilfsfunktionen -----------------
void showCentered(const String &msg) {
    display.clearDisplay();
    display.setFont(&FreeMono7pt7b);
    display.setTextSize(1);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w)/2, (SCREEN_HEIGHT - h)/2);
    display.print(msg);
    display.display();
}

void setLedBrightness(int index, int brightness) {
    uint8_t r = (LED_COLOR_R * brightness) / 255;
    uint8_t g = (LED_COLOR_G * brightness) / 255;
    uint8_t b = (LED_COLOR_B * brightness) / 255;

    for (int i = 0; i < LED_COUNT; i++) {
        if (i == index)
            leds.setPixelColor(i, leds.Color(r,g,b));
        else
            leds.setPixelColor(i, 0);
    }
    leds.show();
}

// ----------------- Setup -----------------
void setup() {
    Serial.begin(115200);
    while (!Serial);

    Wire.begin(SDA_PIN, SCL_PIN);

    
    // OLED init
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("OLED init failed!");
        while (1);
    }

    LEDCTRL::init(&leds);
    MYDISPLAY::init(&display);
    FilamentDB::load();

    display.clearDisplay();
    display.setFont(&FreeMono7pt7b); // Font, der ü unterstützt
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);


    String msg = "WIFI CONNECTING...";
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w)/2, (32 - h)/2);
    display.print(msg);
    display.display();


    // PN532 init
    nfc.begin();
    uint32_t versiondata = nfc.getFirmwareVersion();
    if (!versiondata) {
        Serial.println("PN532 not found");
        showCentered("PN532 FEHLT!");
        while (1);
    }
    nfc.SAMConfig();
    pinMode(PN532_IRQ, INPUT);

    // ----------------- WLAN Setup -----------------
    WiFiManager wifiManager;
    showCentered("VERBINDUNG..."); 
    if(!wifiManager.autoConnect("NFC-Setup-AP")) {
        ESP.restart();
    }
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    showCentered("IP: " + WiFi.localIP().toString());
    delay(5000);
    showCentered("SCAN TAG");
}

void loop() {
    unsigned long now = millis();

    // ----------- NFC Lesen -----------
    uint8_t uid[7]; 
    uint8_t uidLength = 0;
    nfc.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A);
    bool tagFound = nfc.readDetectedPassiveTargetID(uid, &uidLength);

    if (tagFound && uidLength > 0) {
        String uidStr = "";
        for (uint8_t i = 0; i < uidLength; i++) {
            if (uid[i] < 0x10) uidStr += "0";
            uidStr += String(uid[i], HEX);
            if (i != uidLength - 1) uidStr += ":";
        }
        uidStr.toUpperCase();
        Serial.print("FOUND UID: "); Serial.println(uidStr);

        FilamentEntry entry;
        bool known = FilamentDB::findByUID(uidStr, entry);

        if (known) {
            if (entry.ledIndex >= 0 && entry.ledIndex < LED_COUNT) {
                targetLed = entry.ledIndex;
                // LED direkt einschalten
                setLedBrightness(targetLed, LED_BRIGHTNESS);
            }
            MYDISPLAY::show(entry);
        } else {
            // Unbekannt → alle LEDs aus
            targetLed = -1;
            leds.clear();
            leds.show();
            showCentered("UNBEKANNT");
        }

        lastTagTime = now;
        nfc.SAMConfig(); // China Clone workaround
    }

    // ----------- LED Timeout -----------
    if (targetLed != -1 && now - lastTagTime > LED_TIMEOUT) {
        // LED ausschalten
        targetLed = -1;
        leds.clear();
        leds.show();
    }

    // ----------- Display Timeout -----------
    if (targetLed == -1 && now - lastTagTime > LED_TIMEOUT) {
        showCentered("SCAN TAG");
        lastTagTime = now + 999999; // verhindern ständiges redraw
    }

    delay(10);
}
