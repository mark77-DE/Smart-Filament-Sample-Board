#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "filament_db.h"
#include "ledctrl_filament.h"
#include "ledctrl_nfc.h"
#include "display.h"
#include "display_config.h"
#include "my_webserver.h"
#include "globals.h"
#include "display_anim.h"
#include "nfc.h"
#include "filehandling.h"
#include "gpio_hardware.h"



// Reboot-Steuerung
volatile bool rebootPending = false;
unsigned long rebootAt = 0;
static const uint16_t REBOOT_DELAY_MS = 1000; // zusätzliche Verzögerung

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
        LEDCTRL_FILAMENT::setPixel(targetLed, 0); // alte LED aus
    }

    if(index >= 0 && index < LED_COUNT){
        LEDCTRL_FILAMENT::setPixel(index, LED_COLOR);
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

    const bool isNfc = (source == UidSource::NFC);

    if (FilamentDB::findByUID(uid, entry)) {
        // --- BEKANNTES TAG ---

        // Deinen Zielpixel aktivieren (deine bestehende Logik)
        activateLed(entry.ledIndex);

        // Display mit Filament-Infos
        MYDISPLAY::show(entry);

        // NFC-Feedback (grün mit optionalem Blink → solid → Timeout ab Entfernung)
        if (isNfc) {
            LEDCTRL_NFC::showSuccess();
        }

        // nur piepen, wenn von NFC und (optional) kein laufender Beep
        if (isNfc && !buzzer_busy()) {
            buzzer_single_beep();
        }

        // Event fürs Websocket
        doc["action"]   = "knownUID";
        doc["ledIndex"] = entry.ledIndex;
        doc["vendor"]   = entry.vendor;
        doc["type"]     = entry.type;
        doc["color"]    = entry.color;

    } else {
        // --- UNBEKANNTES TAG ---

        // Falls vorher ein Zielpixel gesetzt war: ausmachen & zurücksetzen
        if (targetLed != -1) {
            LEDCTRL_FILAMENT::setPixel(targetLed, 0);
            targetLed = -1;
            ledStartTime = millis();
        }

        // Display-Hinweis
        MYDISPLAY::showCentered("UNBEKANNT");

        if (isNfc) {
            // Rotes Fehlerfeedback am NFC-Ring
            LEDCTRL_NFC::showError();

            // NEU: Filament-Stripe erst BLINKEN lassen,
            // danach (wenn Timeout nicht abgelaufen) automatisch errorAll()
            LEDCTRL_FILAMENT::errorBlink();
        }

        // Event fürs Websocket
        doc["action"] = "unknownUID";
    }

    String msg;
    serializeJson(doc, msg);
    ws.textAll(msg);
}


// ----------------------------- Setup -----------------------------
// -----------------------------------------------------------------
void setup(){
    Serial.begin(115200);
    while(!Serial);

    Serial.printf("Firmware Version: %s\n", FIRMWARE_VERSION);
    Serial.printf("Git Hash: %s\n", GIT_HASH);

    loadConfig();


    Wire.begin(SDA_PIN,SCL_PIN);

    

    // OLED init
    if (!initDisplay(display)) {
        Serial.println("OLED init failed!");
    while (1);
    }

    //Test: Wird bereits in loadConfig() aufgerufen ?! -> Prüfen !
    LEDCTRL_FILAMENT::init(LED_COUNT, LED_PIN, LED_TIMEOUT, LED_BRIGHTNESS, LED_COLOR, LED_COLOR_ERROR, LED_COLOR_PULSE);
    
    LEDCTRL_FILAMENT::allOff();

    LEDCTRL_NFC::init(NFC_LED_COUNT, NFC_LED_PIN, NFC_LED_TIMEOUT, NFC_LED_BRIGHTNESS,
                      NFC_LED_COLOR_SUCCESS, NFC_LED_COLOR_ERROR, NFC_LED_COLOR_PULSE,
                      NFC_LED_SUCCESS_BLINK_ENABLED, NFC_LED_SUCCESS_BLINK_COUNT, NFC_LED_SUCCESS_BLINK_MS);

    LEDCTRL_NFC::allOff();

    // 3. Display & DB init
    MYDISPLAY::init(&display);
    
    display.clearDisplay();
    MYDISPLAY::showCentered("WIFI CONNECTING...");

    // PN532 init
    NFC::init(&nfc);  // macht begin() + SAMConfig()

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

    // WLAN
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
  // ---------------------------------------------------------------------------
  // 0) Zeitbasis
  // ---------------------------------------------------------------------------
  now = millis();


  // 0a) Button/Buzzer tick (Entprellung, Sequencer, Events)
  gpiohw_tick(now);

  // 0b) Long-Press → Reboot einleiten (mit optionalem Doppel-Pieps)
  // Optional: akustische Bestätigung (funktioniert nur, wenn Buzzer konfiguriert ist)
  // Sanft verzögerten Reboot anfordern (siehe Schritt 2 in der loop)
  if (button_long_press()) {
  
        buzzer_double_beep(); 
        rebootPending = true;
        rebootAt      = now + REBOOT_DELAY_MS;

        }

  // ---------------------------------------------------------------------------
  // 1) NFC-Polling + Guards + LED-Trigger
  //    -> NFC::tick() erkennt Tags, triggert handleUID() beim Auflegen (Rising),
  //       setzt isActive/lastTagTime und versorgt LEDCTRL_NFC intern mit Präsenz.
  // ---------------------------------------------------------------------------
  bool tagPresent = false;
  NFC::tick(now, isActive, lastTagTime, tagPresent);

  // ---------------------------------------------------------------------------
  // 1b) Präsenz auch an den FILAMENT-Controller geben
  //     -> dessen Timeout startet erst, wenn der Tag entfernt wurde.
  // ---------------------------------------------------------------------------
  LEDCTRL_FILAMENT::tagPresenceTick(tagPresent);

  // ---------------------------------------------------------------------------
  // 2) Reboot (falls angefordert)
  // ---------------------------------------------------------------------------
  if (rebootPending && now > rebootAt) {
    ESP.restart();
  }

  // ---------------------------------------------------------------------------
  // 3) Display-Idle-Animation nur wenn nicht aktiv
  // ---------------------------------------------------------------------------
  if (!isActive) {
    DisplayAnim::tickIdle(display, now);
  }

  // ---------------------------------------------------------------------------
  // 4) LED-Controller: Filament (Auto-Off erst nach Tag-Entfernung)
  // ---------------------------------------------------------------------------
  LEDCTRL_FILAMENT::update();

  // ---------------------------------------------------------------------------
  // 5) LED-Controller: NFC (Blink -> Solid -> Timeout nach Entfernung)
  // ---------------------------------------------------------------------------
  LEDCTRL_NFC::update();

  // ---------------------------------------------------------------------------
  // 6) Übergang „aktiv → Idle“ (NFC-Controller bestimmt's)
  // ---------------------------------------------------------------------------
  static bool prevIdle = false;
  const bool idleNow = LEDCTRL_NFC::isIdle();
  if (idleNow && !prevIdle) {
    // LEDs sind in Idle übergegangen -> Display mitnehmen
    DisplayAnim::startIdleTextFirst(now);
    isActive = false;
  }
  prevIdle = idleNow;

  // ---------------------------------------------------------------------------
  // 7) (Optional) yield()
  // ---------------------------------------------------------------------------
  yield();
}




