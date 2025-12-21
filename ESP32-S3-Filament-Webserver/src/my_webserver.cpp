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



// ----------------- WebSocket Event -----------------
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type != WS_EVT_DATA) return;

    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->opcode != WS_TEXT) return;

    String msg;
    msg.reserve(len);
    for (size_t i = 0; i < len; i++) msg += (char)data[i];

    // ausreichend Platz für WS-Nachrichten
    JsonDocument doc;

    DeserializationError err = deserializeJson(doc, msg);
    if (err) {
        Serial.print("WS JSON parse error: ");
        Serial.println(err.c_str());
        return;
    }

    // --------- HIGHLIGHT LED (Klick im UI) ----------
    if (doc.containsKey("action") && doc["action"].is<const char*>()) {
        const char *action = doc["action"];
        if (strcmp(action, "highlightLED") == 0) {
            String uid = doc["uid"].as<String>();
            handleUID(uid, UidSource::WEBIF); // zentrale handleUID()
        }
    }
}

// ------------------ Webserver Init -------------------
void initWebServer(AsyncWebServer &server, AsyncWebSocket &ws)
{
    // ROOT
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });

    // STATIC FILES
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/style.css", "text/css");
    });

    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/script.js", "application/javascript");
    });

    server.on("/settings.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/settings.css", "text/css");
    });

    server.on("/settings.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/settings.js", "application/javascript");
    });

    // Admin page
    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/settings.html", "text/html");
    });

    server.on("/api/version", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<256> doc;
        doc["firmware"] = FIRMWARE_VERSION;
        doc["git_hash"] = GIT_HASH;
        doc["build_date"] = BUILD_DATE;
        doc["build_date_short"] = BUILD_DATE_SHORT;


        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Filament-Liste als JSON
    server.on("/filaments.json", HTTP_GET, [](AsyncWebServerRequest *request){
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
    [](AsyncWebServerRequest *req){ req->send(200, "text/plain", "Upload started"); },
    nullptr,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
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
            req->send(200, "text/plain", "Processing"); 
        },
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
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
            req->send(200, "text/plain", "Processing");
        },
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
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
                    Serial.println("ADD: OK");
                }
                
                saveFilamentsToFile();
            } else {
                if(CONFIG.debugMode) {
                    Serial.println("ADD: FAILED");
                }
                
            }
        }
    );

    server.on("/api/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
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
        if (!LittleFS.exists("/config.json")) {
            request->send(404, "application/json", "{\"error\":\"config.json missing\"}");
            return;
        }
        request->send(LittleFS, "/config.json", "application/json");
    });

    server.on("/logo.png", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/logo.png", "image/png");
    });

    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/favicon.ico", "image/x-icon");
    });


    // Update LED Config (sicherer Upload-Handler)
    server.on("/api/updateConfig", HTTP_POST, 
        [](AsyncWebServerRequest *req){},  // keine GET-Handler nötig
        nullptr,                            // kein Body-Upload-Handler für Chunked POST
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
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
            const unsigned long nowMs = millis();

            if (!rebootPending) {
                // Akustik + Countdown starten
                buzzer_double_beep();
                rebootPending = true;
                rebootAt      = nowMs + REBOOT_DELAY_WEBIF_MS;

                // Button-States flushen, damit kein sofortiges Cancel aus altem Zustand kommt
                gpiohw_reset_click_state();

                // WICHTIG: sofort LEDs + UI „armen“ (idempotent)
                renderRebootCountdown(nowMs);
            }
            // optional: Status-JSON zurückgeben
            request->send(200, "application/json", "{\"status\":\"ok\",\"pending\":true}");
    });




    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.serveStatic("/settings.js", LittleFS, "/settings.js")
          .setCacheControl("max-age=86400");

    server.serveStatic("/settings.css", LittleFS, "/settings.css")
          .setCacheControl("max-age=86400");

    server.begin();
}
