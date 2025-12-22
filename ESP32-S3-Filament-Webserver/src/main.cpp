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
#include "version_info.h"
#include "reboot_handler.h"

constexpr uint32_t SPLASH_CHAR_MS = 35;
constexpr uint32_t SPLASH_LINE_MS = 200;
constexpr uint32_t SPLASH_HOLD_MS = 2000;

constexpr uint32_t FIRMWARE_HOLD_MS = 5000;


//Debug
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

// Reboot
void renderRebootCountdown(unsigned long nowMs) {
  static bool     inReboot = false;
  static uint32_t lastSec  = 0xFFFFFFFF;

  if (!rebootPending) {
    if (inReboot) {
      inReboot = false;

      // Präsenz freigeben + sauber resetten, damit nächster Start bei Null beginnt
      LEDCTRL_NFC::tagPresenceTick(false);
      LEDCTRL_FILAMENT::tagPresenceTick(false);
      LEDCTRL_NFC::allOff();
      LEDCTRL_FILAMENT::allOff();

      lastSec = 0xFFFFFFFF;
    }
    return;
  }

  // --- Reboot aktiv ---
  if (!inReboot) {
    inReboot = true;
    lastSec  = 0xFFFFFFFF;
  }

  // Präsenz „halten“, damit Solid/Error nicht aus-Timeouten
  LEDCTRL_NFC::tagPresenceTick(true);
  LEDCTRL_FILAMENT::tagPresenceTick(true);

  // WICHTIG: idempotent „armen“, falls der Controller gerade im Idle ist
  if (LEDCTRL_NFC::isIdle()) {
    LEDCTRL_NFC::showError();       // sofort stabil rot
  }
  if (LEDCTRL_FILAMENT::isIdle()) {
    LEDCTRL_FILAMENT::errorBlink(); // blinkt, dann rot
  }

  // Countdown-Text nur bei Sekundenwechsel neu zeichnen
  const uint32_t remainingMs = (nowMs < rebootAt) ? (rebootAt - nowMs) : 0;
  const uint32_t sec = (remainingMs + 999U) / 1000U;
  if (sec != lastSec) {
    lastSec = sec;
    char line2[24];
    snprintf(line2, sizeof(line2), "%lu s", (unsigned long)sec);
    MYDISPLAY::showThreeCentered(
      F("Reboot in :"),
      String(line2),
      F("Press to Cancel")
    );
  }
}






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
    lastTagTime = millis();
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
void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.printf("Firmware Version: %s\n", FIRMWARE_VERSION);
  Serial.printf("Build Date: %s\n", BUILD_DATE_SHORT);

  // 1) Konfiguration laden
  loadConfig();

  // 2) I2C + DISPLAY FRÜH initialisieren (alles, was malen will, braucht das)
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!initDisplay(display)) {
    Serial.println("OLED init failed!");
    while (1) { delay(100); }
  }
  MYDISPLAY::init(&display);

  // 3) WLAN verbinden (Anzeige davor setzen)
  MYDISPLAY::showCentered("VERBINDUNG...");
  WiFiManager wifiManager;
  if (!wifiManager.autoConnect("NFC-Setup-AP")) {
    ESP.restart();
  }
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  // 4) IP kurz zeigen (nicht hart blockieren)
  {
    MYDISPLAY::showCentered(WiFi.localIP().toString());
    const uint32_t until = millis() + 1200UL;
    while ((int32_t)(until - millis()) > 0) {
      gpiohw_tick(millis());  // Button/Buzzer am Leben halten
      yield();
    }
  }

  // 5) WebSocket + Webserver starten (WebIF nun sofort erreichbar)
  // FIX: doppelte WS-Registrierung vermeiden – nur im Webserver-Modul hinzufügen
  // server.addHandler(&ws); // <-- ENTFERNT, Registrierung erfolgt in initWebServer()
  initWebServer(server, ws);
  WiFi.setSleep(false);

  // 6) LED/NFC Controller initialisieren (schreibt NICHT aufs Display)
  LEDCTRL_FILAMENT::init(LED_COUNT, LED_PIN, LED_TIMEOUT, LED_BRIGHTNESS,
                         LED_COLOR, LED_COLOR_ERROR, LED_COLOR_PULSE);
  LEDCTRL_FILAMENT::allOff();

  LEDCTRL_NFC::init(NFC_LED_COUNT, NFC_LED_PIN, NFC_LED_TIMEOUT,
                    NFC_LED_BRIGHTNESS, NFC_LED_COLOR_SUCCESS,
                    NFC_LED_COLOR_ERROR, NFC_LED_COLOR_PULSE,
                    NFC_LED_SUCCESS_BLINK_ENABLED, NFC_LED_SUCCESS_BLINK_COUNT,
                    NFC_LED_SUCCESS_BLINK_MS);
  LEDCTRL_NFC::allOff();

  // 7) FIRMWARE-BOOTSCREEN x s ANZEIGEN (WebIF ist bereits online)
  {
    MYDISPLAY::showBootVersion(FIRMWARE_VERSION, BUILD_DATE_SHORT);
    const uint32_t until = millis() + FIRMWARE_HOLD_MS; 
    while ((int32_t)(until - millis()) > 0) {
      // Währenddessen nichts blockieren:
      gpiohw_tick(millis());
      ws.cleanupClients();   // optional; AsyncWebServer schafft das auch alleine
      yield();
    }
  }

  // Nach dem Firmware-Bootscreen (10 s), WLAN+Webserver sind schon da
  DisplayAnim::playThreeLineTypewriter(display, F("Spot my"), F("Filament by"), F("Mark & Kolja"),
                                      SPLASH_CHAR_MS, SPLASH_LINE_MS, SPLASH_HOLD_MS);


  // 8) PN532 JETZT initialisieren (kann im Fehlerfall aufs Display schreiben)
  NFC::init(&nfc);  // begin() + SAMConfig()
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

  // 9) Idle-Animation vorbereiten
  DisplayAnim::startIdleTextFirst(millis());
}



void loop() {
  // ---------------------------------------------------------------------------
  // 0) Zeitbasis
  // ---------------------------------------------------------------------------
  const unsigned long now = millis();

  // 0a) Button/Buzzer tick (Entprellung, Sequencer, Events)
  gpiohw_tick(now);

  // 0b) Double-Press → Reboot starten (nur, wenn keiner läuft)
  if (!rebootPending && button_long_press()) {
    buzzer_double_beep();
    rebootPending = true;
    rebootAt      = now + REBOOT_DELAY_MS;
    renderRebootCountdown(now);
  }

  // 0c) Cancel per Single-Press während Countdown
  if (rebootPending && (button_tap_release() || button_short_press())) {
    rebootPending = false;
    buzzer_stop();
    gpiohw_reset_click_state();
    DisplayAnim::startIdleTextFirst(now);
    renderRebootCountdown(now);
  }

  // ---------------------------------------------------------------------------
  // 1) NFC-Polling + Guards + LED-Trigger
  // ---------------------------------------------------------------------------
  bool tagPresent = false;
  NFC::tick(now, isActive, lastTagTime, tagPresent);

  // ---------------------------------------------------------------------------
  // 1b) Präsenz auch an den FILAMENT-Controller geben
  // ---------------------------------------------------------------------------
  LEDCTRL_FILAMENT::tagPresenceTick(tagPresent);

  // ---------------------------------------------------------------------------
  // 2) Reboot (falls angefordert) + Countdown-UI
  // ---------------------------------------------------------------------------
  // 
  if (rebootPending && now > rebootAt) {
    ESP.restart();
  }

  // ---------------------------------------------------------------------------
  // 3) Display: wenn kein Countdown → Idle-Animation
  // ---------------------------------------------------------------------------
  renderRebootCountdown(now);
  if (!rebootPending){
    if(!isActive){
      DisplayAnim::tickIdle(display, now);
    }
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
  if (!rebootPending && idleNow && !prevIdle) {
    DisplayAnim::startIdleTextFirst(now);
    isActive = false;
  }
  prevIdle = idleNow;

  // ---------------------------------------------------------------------------
  // 7) (Optional) yield()
  // ---------------------------------------------------------------------------
  yield();
}
