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
#include "display/display.h"
#include "display/display_config.h"
#include "display/display_anim.h"
#include "display/st7789/display_st7789.h"
#include "my_webserver.h"
#include "globals.h"

#include "nfc.h"
#include "filehandling.h"
#include "gpio_hardware.h"
#include "version_info.h"
#include "reboot_handler.h"
#include "pins.h"
#include "Arduino.h"
#include "esp_ota_ops.h"
#include "config.h"
#include "mqtt_manager.h"
#include "i18n/i18n.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "update_manager.h"





constexpr uint32_t SPLASH_CHAR_MS = 35;       //timing for typewriter effect at boot (ms per char)
constexpr uint32_t SPLASH_LINE_MS = 200;      //extra delay after each line at boot (ms)
constexpr uint32_t SPLASH_HOLD_MS = 2000;     //how long the full splash is shown at boot (after typewriter effect, before animation starts)

constexpr uint32_t FIRMWARE_HOLD_MS = 2000;   //how long fw version is shown at boot (before animation starts)

SysInfo g_sysInfo;

NFCInfo g_nfcInfo = {0,0,0,false};





static void printOtaInfo() {
  const esp_partition_t* boot = esp_ota_get_boot_partition();
  const esp_partition_t* run  = esp_ota_get_running_partition();

  Serial.printf("OTA boot: name=%s addr=0x%06X subtype=0x%02X\n",
                boot ? boot->label : "null",
                boot ? (unsigned)boot->address : 0,
                boot ? (unsigned)boot->subtype : 0);

  Serial.printf("OTA run : name=%s addr=0x%06X subtype=0x%02X\n",
                run ? run->label : "null",
                run ? (unsigned)run->address : 0,
                run ? (unsigned)run->subtype : 0);

  if (run) {
    esp_ota_img_states_t st{};
    if (esp_ota_get_state_partition(run, &st) == ESP_OK) {
      Serial.printf("OTA state: %d (PENDING_VERIFY=%d)\n",
                    (int)st, (int)ESP_OTA_IMG_PENDING_VERIFY);
    }
  }
}

static void markOtaImageValidIfNeeded() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (!running) return;

  esp_ota_img_states_t state{};
  if (esp_ota_get_state_partition(running, &state) != ESP_OK) return;

  if (state == ESP_OTA_IMG_PENDING_VERIFY) {
    Serial.println("OTA: marking app valid (cancel rollback)...");
    esp_ota_mark_app_valid_cancel_rollback();
  }
}


// ----------------- Server & WS -----------------
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ----------------- OLED ---------------


// ----------------- PN532 SPI Settings -----------------
//Adafruit_PN532(uint8_t clk, uint8_t miso, uint8_t mosi, uint8_t ss);
#if DISPLAY_TYPE == DISPLAY_TYPE_ST7789
  Adafruit_PN532 nfc(NFC_SCK, NFC_MISO, NFC_MOSI,  PN532_CS);
#else
  Adafruit_PN532 nfc(PN532_CS);
#endif


// ----------------- LED & Display Timing -----------------
int targetLed = -1;
unsigned long ledStartTime = 0;
unsigned long lastTagTime = 0;
unsigned long now = 0;
bool isActive = false;

// ----------------- global variables -----------------
String activeUID = "";       // active UID

volatile bool g_applyConfigPending = false;
volatile bool g_reloadFilamentsPending = false;


//globale WebIF-Timer-variables + Setter

static bool     s_webifIdleArmed = false;
static uint32_t s_webifIdleUntil = 0;

void webifArmIdleTimeout(uint32_t ms) {
  if (ms == 0) ms = 1;
  s_webifIdleArmed = true;
  s_webifIdleUntil = millis() + ms;
}

bool webifIsArmed() {
  return s_webifIdleArmed;
}

void webifCancelIdleTimeout() {
  s_webifIdleArmed = false;
}

bool webifIdleDue(uint32_t now) {
  if (!s_webifIdleArmed) return false;
  return (int32_t)(now - s_webifIdleUntil) >= 0;
}



// ----------------- Hilfsfunktionen -----------------

// WiFiManager: wird aufgerufen, sobald das Config-Portal/AP gestartet ist.
// Zweck: Wenn der ESP "neu" ist und (noch) nicht am Router hängt, soll sofort die AP-IP angezeigt werden,
// damit der User weiß, wo er verbinden muss (typisch: http://192.168.4.1).
// NEU: Zusätzlich die SSID in der zweiten Zeile anzeigen.
static void onWiFiManagerConfigPortalStarted(WiFiManager* wm) {
  (void)wm;
  const IPAddress apIp = WiFi.softAPIP();
  Serial.printf("WiFiManager AP started. AP IP: %s\n", apIp.toString().c_str());

  // Anzeige NICHT blockieren: autoConnect() läuft weiter; Display bleibt bis zur nächsten Anzeigeänderung so stehen.
  MYDISPLAY::showThreeLinesCentered(
    F("WLAN-SETUP AP"),
    F("SSID: SpotMyFilament"),  //SpotMyFilament AP
    apIp.toString(), TFT_ORANGE
  );
}

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

  // Beim Start des Countdowns IMMER auf Error umschalten (einmalig)
  LEDCTRL_NFC::showError();        // NFC-Ring sofort rot (solid)
  LEDCTRL_FILAMENT::errorBlink();  // Filament: blinkt -> rot (wie gewünscht)
}

// Präsenz „halten“, damit Solid/Error nicht aus-Timeouten
LEDCTRL_NFC::tagPresenceTick(true);
LEDCTRL_FILAMENT::tagPresenceTick(true);


  // Countdown-Text nur bei Sekundenwechsel neu zeichnen
  const uint32_t remainingMs = (nowMs < rebootAt) ? (rebootAt - nowMs) : 0;
  const uint32_t sec = (remainingMs + 999U) / 1000U;
  if (sec != lastSec) {
    lastSec = sec;
    char line2[24];
    snprintf(line2, sizeof(line2), "%lu s", (unsigned long)sec);
    MYDISPLAY::showThreeLinesCentered(
      F("Reboot in :"),
      String(line2),
      F("Press to Cancel")
    );
  }

  
}


void activateLed(int index) {

  if(CONFIGV2.system.debugMode) {
                Serial.print("activateLed: index=");
                Serial.println(index);

  }

  
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

    if(CONFIGV2.system.debugMode) {
        Serial.print("handleUID: UID=");
        Serial.print(uid);
        Serial.print(" Source=");
        Serial.println((source == UidSource::NFC) ? "NFC" : "WebIF");
    }

    const bool isNfc = (source == UidSource::NFC);

    if (FilamentDB::findByUID(uid, entry)) {
        // --- BEKANNTES TAG ---

        // Deinen Zielpixel aktivieren (deine bestehende Logik)
        activateLed(entry.ledIndex);

        // Display mit Filament-Infos
        //MYDISPLAY::show(entry);
        MYDISPLAY::showFourLinesCentered(entry.vendor, entry.type, entry.color, entry.storage);

        if (CONFIGV2.mqttConfig.enabled) {
            publishFilamentState(entry);
        }

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
        MYDISPLAY::showErrorCentered(I18N::get("txt_unknown"), TFT_RED);
      

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



void printChipInfo() {
    
    Serial.printf("ESP-Typ: %s\n", g_sysInfo.chipName);
    Serial.printf("Cores: %d\n", g_sysInfo.cores);
    Serial.printf("Rev: %d\n", g_sysInfo.revision);
    Serial.printf("Flash Size: %lu bytes\n", g_sysInfo.flashSize);
    Serial.printf("Board Variant: %s\n", BOARD_VARIANT);
    Serial.println();
    Serial.println();
}



// ----------------------------- Setup -----------------------------
// -----------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(5000);

  Serial.println();
  Serial.println();
  Serial.println("++-------------------------------++");
  Serial.println("++  SMART FILAMENT SAMPLE BOARD  ++");
  Serial.println("++-------------------------------++");
  Serial.println();
  Serial.println("Booting...");
  Serial.println();

  printOtaInfo();
  
  markOtaImageValidIfNeeded();
  

  Serial.println();
  Serial.printf("Firmware Version Info: %s\n", FIRMWARE_VERSION);
  Serial.printf("Build Date: %s\n", BUILD_DATE_SHORT);
  Serial.println();

  g_sysInfo = getSysInfo();
  
  printChipInfo();
  Serial.println();
  Serial.println("Setup starting...");
  Serial.println();

  
  loadConfigV2();
  applyConfigV2();
  I18N::begin(CONFIGV2.system.defaultLanguage);
  
  LEDCTRL_FILAMENT::allOff();
  LEDCTRL_NFC::allOff();

  // 2) initialize I2C + DISPLAY
  //SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  displayInit();

  

  // 3) WLAN verbinden
  //    Gewünschtes Verhalten:
  //    - Wenn er NICHT verbunden ist und WiFiManager das AP-Config-Portal startet:
  //      -> AP-IP sofort anzeigen (Callback), damit der User weiß, wo er verbinden muss.
  //    - Erst WENN er mit dem Router verbunden ist:
  //      -> "VERBINDUNG..." zeigen und anschließend die Router-IP.
  WiFiManager wifiManager;
  wifiManager.setAPCallback(onWiFiManagerConfigPortalStarted);

  if (CONFIGV2.system.hostname.length() > 0) {
    WiFi.setHostname(CONFIGV2.system.hostname.c_str());  // <- hier
    Serial.printf("Hostname gesetzt: %s\n", CONFIGV2.system.hostname.c_str());
  }

  // Optional: neutrale Anzeige während autoConnect() entscheidet (Router vs. AP-Portal).
  // Wenn AP startet, überschreibt der Callback diese Anzeige automatisch.
  MYDISPLAY::showCentered("WLAN...");
  

  if (!wifiManager.autoConnect("SpotMyFilament")) {
    ESP.restart();
  }

  // Ab hier: Router verbunden
  MYDISPLAY::showCentered("VERBINDUNG...");

  Serial.printf("IP-Address: %s\n", WiFi.localIP().toString().c_str());
  String mac = WiFi.macAddress();
  Serial.printf("MAC-Address: %s\n", mac.c_str());

  // 4) IP kurz zeigen (nicht hart blockieren)
  {
    MYDISPLAY::showCentered(WiFi.localIP().toString(), TFT_GREEN);
    const uint32_t until = millis() + 1200UL;
    while ((int32_t)(until - millis()) > 0) {
      gpiohw_tick(millis());  // Button/Buzzer am Leben halten
      yield();
    }
  }

  

  //init MQTT
  if(CONFIGV2.mqttConfig.enabled) {
    Serial.println("MQTT is enabled, initializing...");
  
    if (WiFi.status() == WL_CONNECTED) {
      mqttInit();
    }
  


  } else {
    Serial.println("MQTT is disabled, skipping initialization.");
  }
  

  // Upadte Check

{

  updateInit();
  updateLoop(); // initial einmal

  
  MYDISPLAY::showBootVersion(FIRMWARE_VERSION, BUILD_DATE_SHORT);
  

  const uint32_t until = millis() + FIRMWARE_HOLD_MS;
  while ((int32_t)(until - millis()) > 0) {
    gpiohw_tick(millis());
    ws.cleanupClients();
    yield();
  }
}

  // Nach dem Firmware-Bootscreen (10 s), WLAN+Webserver sind schon da
  displayClear();
  DisplayAnim::playThreeLineTypewriter(display, F("Spot my"), F("Filament by"), F("Mark & Kolja"),
                                      SPLASH_CHAR_MS, SPLASH_LINE_MS, SPLASH_HOLD_MS);


  // 7) PN532 JETZT initialisieren (kann im Fehlerfall aufs Display schreiben)
  NFC::init(&nfc);  // begin() + SAMConfig()
  uint32_t version = nfc.getFirmwareVersion();
  if (!version) {
    Serial.println("PN532 not found!");
    MYDISPLAY::showCentered("PN532 FEHLT!");
    g_nfcInfo.available = false;
    // while (1) { delay(100); }
  } else {
    g_nfcInfo.available = true;
    g_nfcInfo.fwVerMajor = (version >> 24) & 0xFF;
    g_nfcInfo.fwVerMinor = (version >> 16) & 0xFF;
    g_nfcInfo.chipID     = version & 0xFFFF, HEX;
    Serial.print("[NFC] PN532 FW "); Serial.print((version>>24)&0xFF);
    Serial.print('.');        Serial.print((version>>16)&0xFF);
    Serial.print(" chip=0x"); Serial.println(version & 0xFFFF, HEX);
  }

  

 
    // 8) Idle-Animation vorbereiten
    DisplayAnim::startIdleTextFirst(millis());
  
    // 5) WebSocket + Webserver starten (WebIF nun sofort erreichbar)
  // FIX: doppelte WS-Registrierung vermeiden – nur im Webserver-Modul hinzufügen
  // server.addHandler(&ws); // <-- ENTFERNT, Registrierung erfolgt in initWebServer()
  initWebServer(server, ws);
  WiFi.setSleep(false);

  Serial.println("*********************");
  Serial.println("*                   *");
  Serial.println("*  Setup complete!  *");
  Serial.println("*                   *");
  Serial.println("*********************");
  Serial.println();
  Serial.println();



}


void loop() {

  if (CONFIGV2.mqttConfig.enabled) {
    mqttLoop();
  }
  
  

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

  // 0d (Config)
    if (g_applyConfigPending) {
      g_applyConfigPending = false;

      // 1) alles ruhig stellen
      buzzer_stop();
      gpiohw_reset_click_state();      // verhindert Phantom-Clicks
      DisplayAnim::stop();

      // 2) LEDs aus (damit kein alter Effekt reinfunkt)
      LEDCTRL_NFC::allOff();
      LEDCTRL_FILAMENT::allOff();

      // 3) jetzt erst re-init (sicher im loop-Kontext!)
      applyConfigV2();

      // 4) optional: Idle sauber neu starten
      isActive = false;
      DisplayAnim::startIdleTextFirst(millis());
    }

  // ---------------------------------------------------------------------------
  // 1) NFC-Polling + Guards + LED-Trigger
  // ---------------------------------------------------------------------------
  bool tagPresent = false;
  NFC::tick(now, isActive, lastTagTime, tagPresent);
  
// WebIF kann Idle auslösen (aber NICHT wenn Tag wirklich präsent ist)
  if (!rebootPending && !tagPresent && webifIdleDue(now)) {
    webifCancelIdleTimeout();
    DisplayAnim::startIdleTextFirst(now);
    isActive = false;
  }

  // ---------------------------------------------------------------------------
  // 1b) Präsenz auch an den FILAMENT-Controller geben
  // ---------------------------------------------------------------------------
  LEDCTRL_FILAMENT::tagPresenceTick(tagPresent);

  // ---------------------------------------------------------------------------
  // 2) Reboot (falls angefordert) + Countdown-UI
  // ---------------------------------------------------------------------------
  // 
  if (rebootPending && now > rebootAt) {
    
    MYDISPLAY::showThreeLinesCentered(
      F("-----------"),
      F("Reboot now!"),
      F("-----------")
    );
  
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
    // NICHT auf Idle, wenn WebIF gerade aktiv ist
    if (!webifIsArmed()) {
      DisplayAnim::startIdleTextFirst(now);
      isActive = false;
      }
  }
  prevIdle = idleNow;

  // ---------------------------------------------------------------------------
  // 7) WebSocket hearbeat
  // ---------------------------------------------------------------------------
  sendHeartbeat(ws);

  // ---------------------------------------------------------------------------
  // 8) Update Check
  // ---------------------------------------------------------------------------
  updateLoop();
  if (updateHasChanged()) {
    publishUpdateStatus();
    clearUpdateChanged();
  }
  

  // ---------------------------------------------------------------------------
  // 9) (Optional) yield()
  // ---------------------------------------------------------------------------
  yield();



}
