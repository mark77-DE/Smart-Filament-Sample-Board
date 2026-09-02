#include "my_webserver.h"
#include <ArduinoJson.h>
#include <Update.h>
#include <LittleFS.h>
#include "ledctrl_filament.h"
#include "ledctrl_nfc.h"
#include "globals.h"
#include "filament_db.h"
#include "filehandling.h"
#include "version_info.h"
#include "reboot_handler.h"
#include "nfc.h"
#include "gpio_hardware.h"
#include "display/display.h"
#include "display/display_anim.h"
#include "esp_image_format.h"
#include "config.h"
#include "pins.h"
#include "esp_system.h"
#include "update_manager.h"

File fsFile; // global oder in cpp außerhalb des Lambdas

extern void webifArmIdleTimeout(uint32_t ms);

unsigned long lastHeartbeatMs = 0;
const unsigned long HEARTBEAT_INTERVAL_MS = 1000; // 1 Sekunde

// Vorwärtsdeklaration
extern void renderRebootCountdown(unsigned long nowMs);

extern void handleUID(const String &uid, UidSource source);

const char *boardVariant = BOARD_VARIANT;

SysInfo getSysInfo()
{
    SysInfo info;

    // Enum -> String
    info.chipName = ESP.getChipModel();
    info.cores = ESP.getChipCores();
    info.revision = ESP.getChipRevision();
    info.flashSize = ESP.getFlashChipSize();
    info.fwVersion = FIRMWARE_VERSION;
    info.buildDate = BUILD_DATE_SHORT;

    return info;
}

// ----------------- WebSocket Event -----------------
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    LEDCTRL_FILAMENT::netBusyHint(350);
    LEDCTRL_NFC::netBusyHint(350);

    if (type != WS_EVT_DATA)
        return;

    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->opcode != WS_TEXT)
        return;

    // --- FIX: WS-Fragmente zusammensetzen ---
    static String wsBuf;
    if (info->index == 0)
    {
        wsBuf = "";
        wsBuf.reserve(info->len);
    }

    wsBuf.concat((const char *)data, len);

    // Noch nicht komplett?
    if (!(info->final && (info->index + len == info->len)))
    {
        return;
    }

    // Jetzt ist wsBuf vollständig
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, wsBuf);
    if (err)
    {
        if (CONFIGV2.system.debugMode)
        {
            Serial.print("WS JSON parse error: ");
            Serial.println(err.c_str());
            Serial.print("WS raw: ");
            Serial.println(wsBuf);
        }
        return;
    }

    const char *action = doc["action"] | "";
    if (CONFIGV2.system.debugMode)
    {
        Serial.print("WS action: ");
        Serial.println(action);
    }

    if (strcmp(action, "highlightUIDLED") == 0)
    {
        // --- ACK sofort zurück an genau diesen Client ---
        // (damit JS nicht retry-spamt)
        uint32_t seq = doc["seq"] | 0;
        if (seq != 0)
        {
            JsonDocument ack;
            ack["action"] = "ack";
            ack["seq"] = seq;

            String out;
            serializeJson(ack, out);
            client->text(out);
        }

        // ab hier "heavy work"
        String uid = doc["uid"].as<String>();
        uint32_t t = (CONFIGV2.system.webLEDTimeout > 0) ? CONFIGV2.system.webLEDTimeout : (uint32_t)CONFIGV2.led.timeout;

        LEDCTRL_FILAMENT::webifHoldFor((uint16_t)min<uint32_t>(t, 65535));
        webifArmIdleTimeout(t);

        handleUID(uid, UidSource::WEBIF);
    }
    else if (strcmp(action, "highlightMultiLED") == 0)
    {

        // --- ACK sofort zurück an genau diesen Client ---
        uint32_t seq = doc["seq"] | 0;
        if (seq != 0)
        {
            JsonDocument ack;
            ack["action"] = "ack";
            ack["seq"] = seq;

            String out;
            serializeJson(ack, out);
            client->text(out);
        }

        // --- Timeout bestimmen ---
        uint32_t t = (CONFIGV2.system.webLEDTimeout > 0)
                         ? CONFIGV2.system.webLEDTimeout
                         : (uint32_t)CONFIGV2.led.timeout;

        uint16_t holdMs = (uint16_t)min<uint32_t>(t, 65535);

        LEDCTRL_FILAMENT::webifHoldFor(holdMs);
        webifArmIdleTimeout(t);

        // --- UIDs verarbeiten ---
        JsonArray uids = doc["uids"].as<JsonArray>();

        FilamentEntry entry;
        int16_t index = 0;
        bool anyHit = false;

        LEDCTRL_FILAMENT::allOff();

        for (JsonVariant uidVar : uids)
        {
            String uid = uidVar.as<String>();

            if (CONFIGV2.system.debugMode)
            {
                Serial.print(index++);
                Serial.print(": WS highlightUID: ");
                Serial.println(uid);
            }

            if (FilamentDB::findByUID(uid, entry))
            {
                LEDCTRL_FILAMENT::setPixel(entry.ledIndex, CONFIGV2.led.color);
                anyHit = true;

                if (CONFIGV2.system.debugMode)
                {
                    Serial.printf(
                        "  -> LED %u (%s %s %s | %s | %s | %s)\n",
                        entry.ledIndex,
                        entry.vendor.c_str(),
                        entry.type.c_str(),
                        entry.color.c_str(),
                        entry.info1.c_str(),
                        entry.info2.c_str(),
                        entry.storage.c_str());
                }
            }
            else if (CONFIGV2.system.debugMode)
            {
                Serial.print("  !! UID not found: ");
                Serial.println(uid);
            }
        }

        if (!anyHit && CONFIGV2.system.debugMode)
        {
            Serial.println("WS highlightMultiLED: no matching UIDs");
        }
    }
    else if (strcmp(action, "highlightSingleLed") == 0)
    {

        if (CONFIGV2.system.debugMode)
        {
            Serial.println("WS highlightSingleLed received");
        }

        int ledIndex = doc["led"] | -1; // Default -1, falls key fehlt
        if (ledIndex < 0)
        {
            Serial.println("WS highlightSingleLed: LED index fehlt oder ungültig");
            return;
        }

        activateLed(ledIndex);
        uint32_t t = (CONFIGV2.system.webLEDTimeout > 0) ? CONFIGV2.system.webLEDTimeout : (uint32_t)CONFIGV2.led.timeout;

        LEDCTRL_FILAMENT::webifHoldFor((uint16_t)min<uint32_t>(t, 65535));
        webifArmIdleTimeout(t);
    }
}

// ------------------ Webserver Init -------------------
void initWebServer(AsyncWebServer &server, AsyncWebSocket &ws)
{

    // LittleFS mounten
    if (!LittleFS.begin(true))
    { // true = format if mount fails
        Serial.println("LittleFS mount failed!");
    }
    else
    {
        Serial.println("LittleFS mounted successfully!");
        Serial.print("Total Bytes: ");
        Serial.println(LittleFS.totalBytes());
        Serial.print("Used Bytes:  ");
        Serial.println(LittleFS.usedBytes());
    }

    // [ORDER-FIX]: WebSocket zuerst registrieren, damit /ws nicht vom Catch-all "/" abgefangen wird
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // FIX: Statische Dateien per serveStatic + Cache-Header ausliefern
    //      (schneller, weniger LittleFS-Lesezugriffe, Browser-Caching)
    // Spezifische Routen zuerst:
    server.serveStatic("/settings", LittleFS, "/settings.html")
        .setCacheControl("no-cache"); // HTML bewusst kurz cachen/prüfen

    server.serveStatic("/script.js", LittleFS, "/script.js")
        .setCacheControl("public, max-age=604800"); // 7 Tage

    server.serveStatic("/style.css", LittleFS, "/style.css")
        .setCacheControl("public, max-age=604800");

    server.serveStatic("/settings.js", LittleFS, "/settings.js")
        .setCacheControl("public, max-age=604800");

    server.serveStatic("/settings.css", LittleFS, "/settings.css")
        .setCacheControl("public, max-age=604800");

    server.serveStatic("/logo.png", LittleFS, "/logo.png")
        .setCacheControl("public, max-age=2592000"); // 30 Tage

    server.serveStatic("/favicon.ico", LittleFS, "/favicon.ico")
        .setCacheControl("public, max-age=2592000");

    server.serveStatic("/update.html", LittleFS, "/update.html")
        .setCacheControl("no-cache");

    server.serveStatic("/update.css", LittleFS, "/update.css")
        .setCacheControl("no-cache");

    server.serveStatic("/update.js", LittleFS, "/update.js")
        .setCacheControl("no-cache");

    server.on("/api/version", HTTP_GET, [](AsyncWebServerRequest *request)
              {

        SysInfo info = getSysInfo();

        // FIX: Netzlast-Hinweis – JSON bauen/senden
        LEDCTRL_FILAMENT::netBusyHint(250);
        LEDCTRL_NFC::netBusyHint(250);

        JsonDocument doc;
        doc["firmware"]                 = FIRMWARE_VERSION;
        doc["git_hash"]                 = GIT_HASH;
        doc["build_date"]               = BUILD_DATE;
        doc["build_date_short"]         = BUILD_DATE_SHORT;
        doc["boardVariant"]             = boardVariant;
        doc["chipName"]                 = info.chipName;
        doc["cores"]                    = info.cores;
        doc["revision"]                 = info.revision;
        doc["flashSize"]                = info.flashSize;
        doc["nfc_available"]            = g_nfcInfo.available;
        doc["nfc_fwVerMajor"]           = g_nfcInfo.fwVerMajor;
        doc["nfc_fwVerMinor"]           = g_nfcInfo.fwVerMinor;
        char chipHex[6]; // genug für 0xFFFF
        sprintf(chipHex, "0x%04X", g_nfcInfo.chipID);
        doc["nfc_chipID"]               = chipHex; // als String
        String mac                      = WiFi.macAddress();
        doc["wifi_mac"]                 = mac;
        doc["hostname"]                 = WiFi.getHostname();
        doc["wifi_ip"]                  = WiFi.localIP().toString();
        doc["wifi_gateway"]             = WiFi.gatewayIP().toString();
        doc["wifi_subnet"]              = WiFi.subnetMask().toString();
        doc["wifi_ssid"]                = WiFi.SSID();
        
        doc["wifi_bssid"]               = WiFi.BSSIDstr();
        doc["wifi_channel"]             = WiFi.channel();
        doc["wifi_dns1"]                = WiFi.dnsIP(0).toString();
        doc["wifi_dns2"]                = WiFi.dnsIP(1).toString();
        doc["uptime_ms"]                = millis();
        doc["heap_size"]                = ESP.getHeapSize();
        
        doc["sketch_size"]              = ESP.getSketchSize();
        doc["free_sketch"]              = ESP.getFreeSketchSpace();
        
        doc["spiffs_size"]              = LittleFS.totalBytes();
        doc["free_spiffs"]              = LittleFS.usedBytes();

        
    

        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response); });

    server.on("/api/pinout", HTTP_GET, [](AsyncWebServerRequest *request)
              {

    // Netzlast-Hinweis
    LEDCTRL_FILAMENT::netBusyHint(250);
    LEDCTRL_NFC::netBusyHint(250);

    JsonDocument doc;

    // ------------------------------------------------------------
    // Allgemeine Hardware
    // ------------------------------------------------------------

    doc["LED_PIN"]     = LED_PIN;
    doc["NFC_LED_PIN"] = NFC_LED_PIN;
    doc["BTN_PIN"]     = BTN_PIN;
    doc["BUZ_PIN"]     = BUZ_PIN;


    // ------------------------------------------------------------
    // I2C
    // ------------------------------------------------------------

    doc["I2C"]["SDA"] = SDA_PIN;
    doc["I2C"]["SCL"] = SCL_PIN;


    // ------------------------------------------------------------
    // PN532 SPI
    // ------------------------------------------------------------

    doc["PN532"]["SCK"]  = NFC_SPI_SCK;
    doc["PN532"]["MISO"] = NFC_SPI_MISO;
    doc["PN532"]["MOSI"] = NFC_SPI_MOSI;
    doc["PN532"]["CS"]   = NFC_SPI_CS;

        // ------------------------------------------------------------
        // Display
        // ------------------------------------------------------------

#if DISPLAY_TYPE == DISPLAY_TYPE_ST7789

    doc["display"]["type"] = "ST7789";

    doc["display"]["SPI"]["SCK"]  = TFT_SPI_SCK;
    doc["display"]["SPI"]["MOSI"] = TFT_SPI_MOSI;
    doc["display"]["SPI"]["CS"]   = TFT_SPI_CS;
    doc["display"]["SPI"]["DC"]   = TFT_SPI_DC;
    doc["display"]["SPI"]["RST"]  = TFT_SPI_RST;

#elif DISPLAY_TYPE == DISPLAY_TYPE_SH1106

    doc["display"]["type"] = "SH1106";


#else

    doc["display"]["type"] = "unknown";

#endif

        // ------------------------------------------------------------
        // Board
        // ------------------------------------------------------------

#ifdef BOARD_VARIANT
    doc["board"] = BOARD_VARIANT;
#else
    doc["board"] = "unknown";
#endif


    // ------------------------------------------------------------
    // JSON senden
    // ------------------------------------------------------------

    String response;
    serializeJson(doc, response);

    request->send(200, "application/json", response); });

    // Filament-Liste als JSON
    server.on("/filaments.json", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // FIX: Netzlast-Hinweis – Dateizugriff + JSON
        LEDCTRL_FILAMENT::netBusyHint(350);
        LEDCTRL_NFC::netBusyHint(350);

        std::vector<FilamentEntry> list;
        FilamentDB::getAll(list);

        // Dokument-Größe je nach Anzahl der Einträge anpassen
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        for (auto &e : list) {
            JsonObject      o = arr.add<JsonObject>();
            o["uid"]        = e.uid;
            o["vendor"]     = e.vendor;
            o["type"]       = e.type;
            o["color"]      = e.color;
            o["ledIndex"]   = e.ledIndex;
            o["info1"]      = e.info1;
            o["info2"]      = e.info2;
            o["storage"]    = e.storage;
        }

        String json;
        serializeJson(arr, json);
        request->send(200, "application/json", json); });

    // --- api to export ALL (filaments + config) ---
    server.on("/api/exportAll", HTTP_GET, [](AsyncWebServerRequest *req)
              {
        // FIX: Netzlast-Hinweis – relativ große JSON-Antwort
        LEDCTRL_FILAMENT::netBusyHint(500);
        LEDCTRL_NFC::netBusyHint(500);

        JsonDocument outDoc;

        // config
        JsonObject cfg = outDoc["config"].to<JsonObject>();
        loadConfigAsJsonV2(cfg);

        // filaments
        JsonArray fils = outDoc["filaments"].to<JsonArray>();
        loadFilamentsAsJson(fils);

        String out;
        serializeJsonPretty(outDoc, out);
        req->send(200, "application/json", out); });

    // --- api to import ALL ---
    server.on("/api/importAll", HTTP_POST, [](AsyncWebServerRequest *req)
              { 
            // FIX: Netzlast-Hinweis – Upload startet
            LEDCTRL_FILAMENT::netBusyHint(500);
            LEDCTRL_NFC::netBusyHint(500);
            req->send(200, "text/plain", "Upload started"); }, nullptr, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total)
              {
            // FIX: Netzlast-Hinweis – bei jedem Chunk
            LEDCTRL_FILAMENT::netBusyHint(500);
            LEDCTRL_NFC::netBusyHint(500);

            static String body;
            if (index == 0) { body = ""; if (total > 0) body.reserve(total); }
            body.concat((const char*)data, len);
            if (index + len != total) return;

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, body);
            if (err) {
                req->send(400, "text/plain", "JSON parse failed");
                if(CONFIGV2.system.debugMode) {
                    Serial.println("importAll JSON parse failed");
                }
                return;
            }

            if (doc["config"].is<JsonObject>())
                importConfigJsonV2(doc["config"].as<JsonObject>());

            if (doc["filaments"].is<JsonArray>())
                importFilamentsJson(doc["filaments"].as<JsonArray>());

            req->send(200, "text/plain", "Import OK"); });

    // Update single filament
    server.on("/api/update", HTTP_POST, [](AsyncWebServerRequest *req)
              {
            // FIX: Netzlast-Hinweis – kurzer Upload/JSON
            LEDCTRL_FILAMENT::netBusyHint(350);
            LEDCTRL_NFC::netBusyHint(350);
            req->send(200, "text/plain", "Processing"); }, nullptr, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total)
              {
            // FIX: Netzlast-Hinweis – pro Chunk
            LEDCTRL_FILAMENT::netBusyHint(350);
            LEDCTRL_NFC::netBusyHint(350);

            static String body;
            if(index == 0){
                body = "";
                if(total > 0) body.reserve(total);
            }
            body.concat((const char*)data, len);
            if(index + len != total) return;

            if(CONFIGV2.system.debugMode) {
                Serial.println("Update received: " + body);
            }

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, body);
            if(err){
                if(CONFIGV2.system.debugMode) {
                    Serial.print("update JSON parse failed: ");
                    Serial.println(err.c_str());
                }
                return;
            }

            // Index aus JSON auslesen
            int idx = doc["idx"] | -1;
            if(idx < 0){
                if(CONFIGV2.system.debugMode) {
                    Serial.println("Update failed: missing index");
                }
                return;
            }

            FilamentEntry entry;
            entry.uid       = doc["uid"].as<String>();
            entry.vendor    = doc["vendor"].as<String>();
            entry.type      = doc["type"].as<String>();
            entry.color     = doc["color"].as<String>();
            entry.ledIndex  = doc["ledIndex"].as<int>();
            entry.info1     = doc["info1"].as<String>();
            entry.info2     = doc["info2"].as<String>();
            entry.storage   = doc["storage"].as<String>();

            // Update über Index
            if(FilamentDB::updateAtIndex(idx, entry)){
                saveFilamentsToFile();
                if(CONFIGV2.system.debugMode) {
                    Serial.println("DB updated and saved");
                }
            } else {
                if(CONFIGV2.system.debugMode) {
                    Serial.println("DB update failed");
                }
            } });

    // Neuen Eintrag anlegen
    server.on("/api/add", HTTP_POST, [](AsyncWebServerRequest *req)
              {
            // FIX: Netzlast-Hinweis – kurzer Upload/JSON
            LEDCTRL_FILAMENT::netBusyHint(350);
            LEDCTRL_NFC::netBusyHint(350);
            req->send(200, "text/plain", "Processing"); }, nullptr, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total)
              {
            // FIX: Netzlast-Hinweis – pro Chunk
            LEDCTRL_FILAMENT::netBusyHint(350);
            LEDCTRL_NFC::netBusyHint(350);

            static String body;
            if (index == 0) {
                body = "";
                if (total > 0) body.reserve(total);
            }
            body.concat((const char*)data, len);
            if (index + len != total) return;

            JsonDocument doc;

            DeserializationError err = deserializeJson(doc, body);
            if (err) {
                if(CONFIGV2.system.debugMode) {
                    Serial.print("ADD: JSON parse failed: ");
                    Serial.println(err.c_str());
                }
                return;
            }

            FilamentEntry entry;
            entry.uid       = doc["uid"].as<String>();
            entry.vendor    = doc["vendor"].as<String>();
            entry.type      = doc["type"].as<String>();
            entry.color     = doc["color"].as<String>();
            entry.ledIndex  = doc["ledIndex"].as<int>();
            entry.info1     = doc["info1"].as<String>();
            entry.info2     = doc["info2"].as<String>();
            entry.storage   = doc["storage"].as<String>();

            if (FilamentDB::add(entry)) {
                if(CONFIGV2.system.debugMode) {
                    Serial.println("ADD filament: OK");
                }
                saveFilamentsToFile();
            } else {
                if(CONFIGV2.system.debugMode) {
                    Serial.println("ADD filament: FAILED");
                }
            } });

    server.on("/api/delete", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        // FIX: Netzlast-Hinweis – kleiner JSON-Response + DB-Zugriff
        LEDCTRL_FILAMENT::netBusyHint(250);
        LEDCTRL_NFC::netBusyHint(250);

        if (!request->hasParam("uid", true)) {
            request->send(400, "application/json",
                "{\"status\":\"error\",\"msg\":\"missing uid\"}");
            return;
        }

        String uid = request->getParam("uid", true)->value();

        if (!FilamentDB::remove(uid)) {
            request->send(404, "application/json",
                "{\"status\":\"error\",\"msg\":\"uid not found\"}");
            return;
        } else {
            saveFilamentsToFile();
        }

        request->send(200, "application/json", "{\"status\":\"ok\"}"); });

    // Config als JSON ausliefern
    server.on("/config.json", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // FIX: Netzlast-Hinweis – FS-Read + JSON
        LEDCTRL_FILAMENT::netBusyHint(250);
        LEDCTRL_NFC::netBusyHint(250);

        if (!LittleFS.exists("/config.json")) {
            request->send(404, "application/json", "{\"error\":\"config.json missing\"}");
            return;
        }
        request->send(LittleFS, "/config.json", "application/json"); });

    // Config als JSON ausliefern
    server.on("/config_v2.json", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // FIX: Netzlast-Hinweis – FS-Read + JSON
        LEDCTRL_FILAMENT::netBusyHint(250);
        LEDCTRL_NFC::netBusyHint(250);

        if (!LittleFS.exists("/config_v2.json")) {
            request->send(404, "application/json", "{\"error\":\"config_v2.json missing\"}");
            return;
        }
        request->send(LittleFS, "/config_v2.json", "application/json"); });

    // Help/Texte als JSON ausliefern
    server.on("/help_de.json", HTTP_GET, [](AsyncWebServerRequest *request)
              {

        // Optional: Netzlast-Hinweis wie bei Config
        LEDCTRL_FILAMENT::netBusyHint(150);
        LEDCTRL_NFC::netBusyHint(150);

        if (!LittleFS.exists("/help_de.json")) {
            request->send(404, "application/json", "{\"error\":\"help_de.json missing\"}");
            return;
        }

        request->send(LittleFS, "/help_de.json", "application/json"); });

    server.on("/help_en.json", HTTP_GET, [](AsyncWebServerRequest *request)
              {

        // Optional: Netzlast-Hinweis wie bei Config
        LEDCTRL_FILAMENT::netBusyHint(150);
        LEDCTRL_NFC::netBusyHint(150);

        if (!LittleFS.exists("/help_en.json")) {
            request->send(404, "application/json", "{\"error\":\"help_en.json missing\"}");
            return;
        }

        request->send(LittleFS, "/help_en.json", "application/json"); });

    server.on("/i18n_help.js", HTTP_GET, [](AsyncWebServerRequest *request)
              {

        // Optional: Netzlast-Hinweis wie bei Config
        LEDCTRL_FILAMENT::netBusyHint(150);
        LEDCTRL_NFC::netBusyHint(150);

        if (!LittleFS.exists("/i18n_help.js")) {
            request->send(404, "application/json", "{\"error\":\"i18n_help.js missing\"}");
            return;
        }

        request->send(LittleFS, "/i18n_help.js", "application/javascript"); });

    // FIX: /logo.png und /favicon.ico laufen nun über serveStatic (oben) mit Cache
    // server.on("/logo.png", HTTP_GET, ...);    // entfernt
    // server.on("/favicon.ico", HTTP_GET, ...); // entfernt

    // Update LED Config (sicherer Upload-Handler)
    server.on("/api/updateConfig", HTTP_POST, [](AsyncWebServerRequest *req)
              { 
            // FIX: Netzlast-Hinweis – Start der Config-Übertragung
            LEDCTRL_FILAMENT::netBusyHint(400);
            LEDCTRL_NFC::netBusyHint(400); },     // keine GET-Handler nötig
              nullptr, // kein Body-Upload-Handler für Chunked POST
              [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total)
              {
            // FIX: Netzlast-Hinweis – pro Chunk
            LEDCTRL_FILAMENT::netBusyHint(400);
            LEDCTRL_NFC::netBusyHint(400);

            static String body;
            if (index == 0) {
                body = "";
                if (total > 0) body.reserve(total);
            }
            body.concat((const char*)data, len);

            if (index + len != total) return; // noch nicht alles empfangen

            // --- JSON parsen ---
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, body);

            
            
            if (err) {
                req->send(400, "text/plain", "JSON Error");
                if(CONFIGV2.system.debugMode) {
                    Serial.print("updateConfig JSON error: ");
                    Serial.println(err.c_str());
                }
                return;
            }       

            // --- Update CONFIG ---
            if (!updateConfigFromJsonV2(doc)) {
                req->send(400, "text/plain", "Invalid JSON structure");
                return;
            }

            req->send(200, "application/json", "{\"status\":\"ok\"}"); });

    server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        // FIX: Netzlast-Hinweis – Request bearbeiten
        LEDCTRL_FILAMENT::netBusyHint(250);
        LEDCTRL_NFC::netBusyHint(250);

        const unsigned long nowMs = millis();

        if (!rebootPending) {
            // Akustik + Countdown starten
            buzzer_double_beep();
            rebootPending = true;
            rebootAt      = nowMs + REBOOT_DELAY_WEBIF_MS;

            // Button-States flushen, damit kein sofortiges Cancel aus altem Zustand kommt
            gpiohw_reset_click_state();

            // WICHTIG: sofort LEDs + UI „armen“ (idempotent)
            // FIX: compile-sicher dank Vorwärtsdeklaration oben
            renderRebootCountdown(nowMs);
        }
        // optional: Status-JSON zurückgeben
        request->send(200, "application/json", "{\"status\":\"ok\",\"pending\":true}"); });

    server.on("/api/otaUpdate", HTTP_POST, [](AsyncWebServerRequest *req)
              { req->send(200, "text/plain", "Upload started"); }, nullptr, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total)
              {

        SysInfo info = getSysInfo();

        if (req->hasParam("filename", true)) { // true = von FormData
            String fname = req->getParam("filename", true)->value();

            if (!fname.startsWith("firmware") || !fname.endsWith(".bin")) {
                req->send(400, "text/plain", "Wrong filename! Must start with 'firmware' and end with '.bin'");
            return;
            }
        }


        if (index == 0) {


            // =====================================================
            // 1) OTA START
            // =====================================================
            Serial.printf("Starting FW OTA update: %u bytes\n", total);

            DisplayAnim::stop();
            MYDISPLAY::showThreeLinesCentered(
                F("started"),
                F("FW OTA"),
                F("update")
            );

            if (!Update.begin(total)) {
                Serial.println("OTA begin failed");

                MYDISPLAY::showThreeLinesCentered(
                    F("FW OTA"),
                    F("update"),
                    F("failed")
                );
                return;
            }
        }

        // =====================================================
        // 2) CHUNK SCHREIBEN
        // =====================================================
        if (Update.write(data, len) != len) {
            Serial.printf("FW update write failed! Error: %d\n", Update.getError());

            MYDISPLAY::showThreeLinesCentered(
                F("FW OTA"),
                F("update"),
                F("failed")
            );
            return;
        }

        // =====================================================
        // 3) OTA FINALISIEREN
        // =====================================================
        if (index + len == total) {

            if (Update.end(false)) {   // kein Auto-Reboot
                Serial.println("FW OTA applied successfully");

                MYDISPLAY::showThreeLinesCentered(
                    F("FW OTA"),
                    F("update"),
                    F("success")
                );

                req->send(200, "application/json",
                    "{\"status\":\"ok\",\"msg\":\"FW update successful, rebooting\"}");

                rebootPending = true;
                rebootAt = millis() + 3000;

            } else {
                req->send(500, "application/json",
                    "{\"status\":\"error\",\"msg\":\"FW update failed\"}");
            }
        } });

    server.on("/api/uploadFS", HTTP_POST, [](AsyncWebServerRequest *req)
              { req->send(200, "text/plain", "FS Upload started"); }, nullptr, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total)
              {

        if (req->hasParam("filename", true)) { // falls dein JS FormData verwendet
            String fname = req->getParam("filename", true)->value();
            if(fname != "littlefs.bin"){
                req->send(400, "text/plain", "Wrong filename! Expected: littlefs.bin");
                return;
            }
        }


        if(index == 0){
            Serial.printf("Starting FS OTA update: %u bytes\n", total);
            DisplayAnim::stop();
            MYDISPLAY::showThreeLinesCentered(
                F("started"),
                F("FS OTA"),
                F("update")
            );
            if(!Update.begin(total, U_SPIFFS)) {  // <-- U_SPIFFS für FS-OTA
                Serial.printf("Update begin failed! Error: %d\n", Update.getError());
                MYDISPLAY::showThreeLinesCentered(
                    F("FS OTA"),
                    F("update"),
                    F("failed")
                );
                return;
            }
        }

        // Chunk schreiben
        if(Update.write(data, len) != len){
            Serial.printf("FS update write failed! Error: %d\n", Update.getError());
            MYDISPLAY::showThreeLinesCentered(
                    F("FS OTA"),
                    F("update"),
                    F("failed")
                );
            return;
        }

        // Letzter Chunk
        if (index + len == total) {

            if (Update.end(false)) {   // WICHTIG: false = KEIN automatischer Reboot
                Serial.println("FS OTA applied successfully");

                req->send(200, "application/json",
                    "{\"status\":\"ok\",\"msg\":\"FS update successful, rebooting\"}");

                // Reboot verzögert auslösen
        
                rebootPending = true;
                rebootAt = millis() + 3000;
            } else {
                req->send(500, "application/json",
                    "{\"status\":\"error\",\"msg\":\"FS update failed\"}");
            }
        } });

    // [ORDER-FIX]: Catch-all (ROOT) *zuletzt*, damit nichts Wichtiges davor abgefangen wird
    server.serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html")
        .setCacheControl("no-cache"); // Startseite immer revalidieren

    server.begin();
}

void sendHeartbeat(AsyncWebSocket &ws)
{
    unsigned long now = millis();
    if (now - lastHeartbeatMs < HEARTBEAT_INTERVAL_MS)
        return;
    lastHeartbeatMs = now;

    JsonDocument doc;
    doc["action"] = "heartbeat";
    doc["uptime_ms"] = millis();
    doc["heap_free"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["cpu_temp_c"] = temperatureRead();

    // Update-Info
    UpdateInfo &update = getUpdateInfo();

    if (update.updateAvailable)
    {
        doc["updateAvailable"] = update.updateAvailable;
        doc["currentVersion"] = update.currentVersion;
        doc["latestVersion"] = update.latestVersion;
        doc["lastCheck"] = update.lastCheck;
    }
    else
    {
        doc["updateAvailable"] = false;
    }

    String out;
    serializeJson(doc, out);
    ws.textAll(out); // an alle Clients senden
}