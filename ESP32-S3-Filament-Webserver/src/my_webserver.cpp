#include "my_webserver.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "ledctrl_filament.h"
#include "ledctrl_nfc.h"
#include <vector>
#include "display.h"
#include "globals.h"
#include "filament_db.h"
#include "filehandling.h"
#include "gpio_hardware.h"
#include "version_info.h"
#include "reboot_handler.h"
#include "nfc.h"


extern void webifArmIdleTimeout(uint32_t ms);


//Vorwärtsdeklaration
extern void renderRebootCountdown(unsigned long nowMs);

extern void handleUID(const String &uid, UidSource source);


SysInfo getSysInfo() {
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

    if (type != WS_EVT_DATA) return;

    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->opcode != WS_TEXT) return;

    // --- FIX: WS-Fragmente zusammensetzen ---
    static String wsBuf;
    if (info->index == 0) {
        wsBuf = "";
        wsBuf.reserve(info->len);
    }

    wsBuf.concat((const char*)data, len);

    // Noch nicht komplett?
    if (!(info->final && (info->index + len == info->len))) {
        return;
    }

    // Jetzt ist wsBuf vollständig
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, wsBuf);
    if (err) {
        if (CONFIG.debugMode) {
            Serial.print("WS JSON parse error: ");
            Serial.println(err.c_str());
            Serial.print("WS raw: ");
            Serial.println(wsBuf);
        }
        return;
    }

    const char* action = doc["action"] | "";
    if (strcmp(action, "highlightLED") == 0) {
        // --- ACK sofort zurück an genau diesen Client ---
        // (damit JS nicht retry-spamt)
        uint32_t seq = doc["seq"] | 0;
        if (seq != 0) {
            JsonDocument ack;
            ack["action"] = "ack";
            ack["seq"]    = seq;

            String out;
            serializeJson(ack, out);
            client->text(out);
        }

        // ab hier "heavy work"
        String uid = doc["uid"].as<String>();
        uint32_t t = (CONFIG.webLEDTimeout > 0) ? CONFIG.webLEDTimeout : (uint32_t)CONFIG.led.timeout;

        LEDCTRL_FILAMENT::webifHoldFor((uint16_t)min<uint32_t>(t, 65535));
        webifArmIdleTimeout(t);

        handleUID(uid, UidSource::WEBIF);
    }

}


// ------------------ Webserver Init -------------------
void initWebServer(AsyncWebServer &server, AsyncWebSocket &ws)
{
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

    // ALT: Direkte Handler für statische Files entfernt (durch serveStatic ersetzt)
    // server.on("/", HTTP_GET, ...);
    // server.on("/style.css", HTTP_GET, ...);
    // server.on("/script.js", HTTP_GET, ...);
    // server.on("/settings.css", HTTP_GET, ...);
    // server.on("/settings.js", HTTP_GET, ...);
    // server.on("/settings", HTTP_GET, ...);
    // server.on("/logo.png", HTTP_GET, ...);
    // server.on("/favicon.ico", HTTP_GET, ...);

    server.on("/api/version", HTTP_GET, [](AsyncWebServerRequest *request) {

        SysInfo info = getSysInfo();

        // FIX: Netzlast-Hinweis – JSON bauen/senden
        LEDCTRL_FILAMENT::netBusyHint(250);
        LEDCTRL_NFC::netBusyHint(250);

        JsonDocument doc;
        doc["firmware"]                 = FIRMWARE_VERSION;
        doc["git_hash"]                 = GIT_HASH;
        doc["build_date"]               = BUILD_DATE;
        doc["build_date_short"]         = BUILD_DATE_SHORT;
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
        doc["wifi_rssi"]                = WiFi.RSSI();
        doc["wifi_bssid"]               = WiFi.BSSIDstr();
        doc["wifi_channel"]             = WiFi.channel();
        doc["wifi_dns1"]                = WiFi.dnsIP(0).toString();
        doc["wifi_dns2"]                = WiFi.dnsIP(1).toString();
        doc["uptime_ms"]                = millis();
        doc["heap_size"]                = ESP.getHeapSize();
        doc["free_heap"]                = ESP.getFreeHeap();
        doc["sketch_size"]              = ESP.getSketchSize();
        doc["free_sketch"]              = ESP.getFreeSketchSpace();
        doc["spiffs_size"]              = LittleFS.totalBytes();
        doc["free_spiffs"]              = LittleFS.usedBytes();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Filament-Liste als JSON
    server.on("/filaments.json", HTTP_GET, [](AsyncWebServerRequest *request){
        // FIX: Netzlast-Hinweis – Dateizugriff + JSON
        LEDCTRL_FILAMENT::netBusyHint(350);
        LEDCTRL_NFC::netBusyHint(350);

        std::vector<FilamentEntry> list;
        FilamentDB::getAll(list);

        // Dokument-Größe je nach Anzahl der Einträge anpassen
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        for (auto &e : list) {
            JsonObject o = arr.add<JsonObject>();
            o["uid"] = e.uid;
            o["vendor"] = e.vendor;
            o["type"] = e.type;
            o["color"] = e.color;
            o["ledIndex"] = e.ledIndex;
        }

        String json;
        serializeJson(arr, json);
        request->send(200, "application/json", json);
    });

    // --- api to export ALL (filaments + config) ---
    server.on("/api/exportAll", HTTP_GET, [](AsyncWebServerRequest *req) {
        // FIX: Netzlast-Hinweis – relativ große JSON-Antwort
        LEDCTRL_FILAMENT::netBusyHint(500);
        LEDCTRL_NFC::netBusyHint(500);

        JsonDocument outDoc;

        // config
        JsonObject cfg = outDoc["config"].to<JsonObject>();
        loadConfigAsJson(cfg);

        // filaments
        JsonArray fils = outDoc["filaments"].to<JsonArray>();
        loadFilamentsAsJson(fils);

        String out;
        serializeJsonPretty(outDoc, out);
        req->send(200, "application/json", out);
    });

    // --- api to import ALL ---
    server.on("/api/importAll", HTTP_POST,
        [](AsyncWebServerRequest *req){ 
            // FIX: Netzlast-Hinweis – Upload startet
            LEDCTRL_FILAMENT::netBusyHint(500);
            LEDCTRL_NFC::netBusyHint(500);
            req->send(200, "text/plain", "Upload started"); 
        },
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
            // FIX: Netzlast-Hinweis – bei jedem Chunk
            LEDCTRL_FILAMENT::netBusyHint(500);
            LEDCTRL_NFC::netBusyHint(500);

            static String body;
            if (index == 0) { body = ""; if (total > 0) body.reserve(total); }
            body.concat((const char*)data, len);
            if (index + len != total) return;

            DynamicJsonDocument doc(8192);
            DeserializationError err = deserializeJson(doc, body);
            if (err) {
                req->send(400, "text/plain", "JSON parse failed");
                if(CONFIG.debugMode) {
                    Serial.println("importAll JSON parse failed");
                }
                return;
            }

            if (doc.containsKey("config") && doc["config"].is<JsonObject>())
                importConfigJson(doc["config"].as<JsonObject>());

            if (doc.containsKey("filaments") && doc["filaments"].is<JsonArray>())
                importFilamentsJson(doc["filaments"].as<JsonArray>());

            req->send(200, "text/plain", "Import OK");
        }
    );

    // Update single filament
    server.on("/api/update", HTTP_POST,
        [](AsyncWebServerRequest *req){
            // FIX: Netzlast-Hinweis – kurzer Upload/JSON
            LEDCTRL_FILAMENT::netBusyHint(350);
            LEDCTRL_NFC::netBusyHint(350);
            req->send(200, "text/plain", "Processing");
        },
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
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

            if(CONFIG.debugMode) {
                Serial.println("Update received: " + body);
            }

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, body);
            if(err){
                if(CONFIG.debugMode) {
                    Serial.print("update JSON parse failed: ");
                    Serial.println(err.c_str());
                }
                return;
            }

            // Index aus JSON auslesen
            int idx = doc["idx"] | -1;
            if(idx < 0){
                if(CONFIG.debugMode) {
                    Serial.println("Update failed: missing index");
                }
                return;
            }

            FilamentEntry entry;
            entry.uid      = doc["uid"].as<String>();
            entry.vendor   = doc["vendor"].as<String>();
            entry.type     = doc["type"].as<String>();
            entry.color    = doc["color"].as<String>();
            entry.ledIndex = doc["ledIndex"].as<int>();

            // Update über Index
            if(FilamentDB::updateAtIndex(idx, entry)){
                saveFilamentsToFile();
                if(CONFIG.debugMode) {
                    Serial.println("DB updated and saved");
                }
            } else {
                if(CONFIG.debugMode) {
                    Serial.println("DB update failed");
                }
            }
        }
    );

    // Neuen Eintrag anlegen
    server.on("/api/add", HTTP_POST,
        [](AsyncWebServerRequest *req){
            // FIX: Netzlast-Hinweis – kurzer Upload/JSON
            LEDCTRL_FILAMENT::netBusyHint(350);
            LEDCTRL_NFC::netBusyHint(350);
            req->send(200, "text/plain", "Processing");
        },
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
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
                if(CONFIG.debugMode) {
                    Serial.print("ADD: JSON parse failed: ");
                    Serial.println(err.c_str());
                }
                return;
            }

            FilamentEntry entry;
            entry.uid      = doc["uid"].as<String>();
            entry.vendor   = doc["vendor"].as<String>();
            entry.type     = doc["type"].as<String>();
            entry.color    = doc["color"].as<String>();
            entry.ledIndex = doc["ledIndex"].as<int>();

            if (FilamentDB::add(entry)) {
                if(CONFIG.debugMode) {
                    Serial.println("ADD filament: OK");
                }
                saveFilamentsToFile();
            } else {
                if(CONFIG.debugMode) {
                    Serial.println("ADD filament: FAILED");
                }
            }
        }
    );

    server.on("/api/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
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

        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // Config als JSON ausliefern
    server.on("/config.json", HTTP_GET, [](AsyncWebServerRequest *request){
        // FIX: Netzlast-Hinweis – FS-Read + JSON
        LEDCTRL_FILAMENT::netBusyHint(250);
        LEDCTRL_NFC::netBusyHint(250);

        if (!LittleFS.exists("/config.json")) {
            request->send(404, "application/json", "{\"error\":\"config.json missing\"}");
            return;
        }
        request->send(LittleFS, "/config.json", "application/json");
    });

    // FIX: /logo.png und /favicon.ico laufen nun über serveStatic (oben) mit Cache
    // server.on("/logo.png", HTTP_GET, ...);    // entfernt
    // server.on("/favicon.ico", HTTP_GET, ...); // entfernt

    // Update LED Config (sicherer Upload-Handler)
    server.on("/api/updateConfig", HTTP_POST, 
        [](AsyncWebServerRequest *req){ 
            // FIX: Netzlast-Hinweis – Start der Config-Übertragung
            LEDCTRL_FILAMENT::netBusyHint(400);
            LEDCTRL_NFC::netBusyHint(400);
        },  // keine GET-Handler nötig
        nullptr,                            // kein Body-Upload-Handler für Chunked POST
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
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

            // --- Debug Ausgabe optional ---
            if (CONFIG.debugMode) {
                Serial.println("Updated CONFIG:");
                Serial.printf("LED: count=%d, pin=%d, brightness=%d, timeout=%d, color=0x%06X, colorError=0x%06X, colorPulse=0x%06X\n", 
                              CONFIG.led.count, CONFIG.led.pin, CONFIG.led.brightness, CONFIG.led.timeout, CONFIG.led.color, CONFIG.led.colorError, CONFIG.led.colorPulse);
                Serial.printf("NFC: count=%d, pin=%d, brightness=%d, timeout=%d, success=0x%06X, error=0x%06X, pulse=0x%06X\n",
                              CONFIG.nfc.count, CONFIG.nfc.pin, CONFIG.nfc.brightness, CONFIG.nfc.timeout,
                              CONFIG.nfc.colorSuccess, CONFIG.nfc.colorError, CONFIG.nfc.colorPulse);
                Serial.printf("NFC Blink: enabled=%d, count=%d, ms=%d\n",
                              CONFIG.nfc.successBlinkEnabled, CONFIG.nfc.successBlinkCount, CONFIG.nfc.successBlinkMs);
                Serial.printf("Debug Mode: %s\n", CONFIG.debugMode ? "ON" : "OFF");
            }
            
            if (err) {
                req->send(400, "text/plain", "JSON Error");
                if(CONFIG.debugMode) {
                    Serial.print("updateConfig JSON error: ");
                    Serial.println(err.c_str());
                }
                return;
            }

            // --- Update CONFIG ---
            if (!updateConfigFromJson(doc)) {
                req->send(400, "text/plain", "Invalid JSON structure");
                return;
            }

            req->send(200, "application/json", "{\"status\":\"ok\"}");
        }
    );

    server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request){
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
        request->send(200, "application/json", "{\"status\":\"ok\",\"pending\":true}");
    });


    

    // [ORDER-FIX]: Catch-all (ROOT) *zuletzt*, damit nichts Wichtiges davor abgefangen wird
    server.serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html")
        .setCacheControl("no-cache"); // Startseite immer revalidieren

    server.begin();
}
