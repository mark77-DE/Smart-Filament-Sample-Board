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
#include "pins.h"
#include "Arduino.h"
#include "esp_ota_ops.h"



constexpr uint32_t SPLASH_CHAR_MS = 35;
constexpr uint32_t SPLASH_LINE_MS = 200;
constexpr uint32_t SPLASH_HOLD_MS = 2000;

constexpr uint32_t FIRMWARE_HOLD_MS = 5000;

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


//Debug
bool DEBUG_MODE = false;

// ----------------- Server & WS -----------------
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ----------------- OLED ---------------
DisplayType display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET_PIN);

// ----------------- PN532 SPI Settings -----------------

Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_CS);

// ----------------- LED & Display Timing -----------------
int targetLed = -1;
unsigned long ledStartTime = 0;
unsigned long lastTagTime = 0;
unsigned long now = 0;
bool isActive = false;

// ----------------- Globale Variablen -----------------
String activeUID = "";       // aktuell aktive UID

volatile bool g_applyConfigPending = false;
volatile bool g_reloadFilamentsPending = false;


//globale WebIF-Timer-Variablen + Setter

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
  MYDISPLAY::showThreeCentered(
    F("WLAN-SETUP AP"),
    F("SSID: NFC-Setup-AP"),
    apIp.toString()
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



void printChipInfo() {
    
    Serial.printf("ESP-Typ: %s\n", g_sysInfo.chipName);
    Serial.printf("Cores: %d\n", g_sysInfo.cores);
    Serial.printf("Rev: %d\n", g_sysInfo.revision);
    Serial.printf("Flash Size: %lu bytes\n", g_sysInfo.flashSize);
    Serial.println();
    Serial.println();
}



// ----------------------------- Setup -----------------------------
// -----------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(50);

  printOtaInfo();
  markOtaImageValidIfNeeded();
  


  Serial.printf("Firmware Version Info: %s\n", FIRMWARE_VERSION);
  Serial.printf("Build Date: %s\n", BUILD_DATE_SHORT);


  g_sysInfo = getSysInfo();
  
  printChipInfo();

  // 1) Konfiguration laden
  loadConfig();
  applyConfig(); 
  LEDCTRL_FILAMENT::allOff();
  LEDCTRL_NFC::allOff();

  // 2) I2C + DISPLAY FRÜH initialisieren (alles, was malen will, braucht das)
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!initDisplay(display)) {
    Serial.println("OLED init failed!");
    while (1) { delay(100); }
  }
  MYDISPLAY::init(&display);

  // 3) WLAN verbinden
  //    Gewünschtes Verhalten:
  //    - Wenn er NICHT verbunden ist und WiFiManager das AP-Config-Portal startet:
  //      -> AP-IP sofort anzeigen (Callback), damit der User weiß, wo er verbinden muss.
  //    - Erst WENN er mit dem Router verbunden ist:
  //      -> "VERBINDUNG..." zeigen und anschließend die Router-IP.
  WiFiManager wifiManager;
  wifiManager.setAPCallback(onWiFiManagerConfigPortalStarted);

  if (CONFIG.hostname.length() > 0) {
    WiFi.setHostname(CONFIG.hostname.c_str());  // <- hier
    Serial.printf("Hostname gesetzt: %s\n", CONFIG.hostname.c_str());
  }

  // Optional: neutrale Anzeige während autoConnect() entscheidet (Router vs. AP-Portal).
  // Wenn AP startet, überschreibt der Callback diese Anzeige automatisch.
  MYDISPLAY::showCentered("WLAN...");

  if (!wifiManager.autoConnect("NFC-Setup-AP")) {
    ESP.restart();
  }

  // Ab hier: Router verbunden
  MYDISPLAY::showCentered("VERBINDUNG...");

  Serial.printf("IP-Address: %s\n", WiFi.localIP().toString().c_str());
  String mac = WiFi.macAddress();
  Serial.printf("MAC-Address: %s\n", mac.c_str());

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


  // 6) FIRMWARE-BOOTSCREEN x s ANZEIGEN (WebIF ist bereits online)
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
    Serial.print("PN532 FW "); Serial.print((version>>24)&0xFF);
    Serial.print('.');        Serial.print((version>>16)&0xFF);
    Serial.print(" chip=0x"); Serial.println(version & 0xFFFF, HEX);
  }

  // 8) Idle-Animation vorbereiten
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
      applyConfig();

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
    
    MYDISPLAY::showThreeCentered(
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
  // 7) (Optional) yield()
  // ---------------------------------------------------------------------------
  yield();
}
