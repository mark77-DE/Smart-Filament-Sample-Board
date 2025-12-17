#include "my_webserver.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "ledctrl_filament.h"
#include "ledctrl_nfc.h"
#include <vector>
#include "display.h"
#include "globals.h"
#include "filament_db.h"
#include "config.h"

extern volatile bool rebootPending;
extern unsigned long rebootAt;



void showRebootScreen(){
        // Zeige Reboot-Nachricht an
        Serial.println("Rebooting...");
    }

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

    server.on("/admin.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/admin.css", "text/css");
    });

    server.on("/admin.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/admin.js", "application/javascript");
    });

    // Admin page
    server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/admin.html", "text/html");
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


    // --- Export ALL (filaments + config) ---
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

    // --- Import ALL ---
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
            Serial.println("importAll JSON parse failed");
            return;
        }

        if (doc.containsKey("config") && doc["config"].is<JsonObject>())
            importConfigJson(doc["config"].as<JsonObject>());

        //if (doc.containsKey("filaments") && doc["filaments"].is<JsonArray>())
        //    importFilamentsJson(doc["filaments"].as<JsonArray>());

        req->send(200, "text/plain", "Import OK");
    }
);


    
    // Update eines Filament Eintrags
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

        Serial.println("Received body: " + body);

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if(err){
            Serial.print("update JSON parse failed: ");
            Serial.println(err.c_str());
            return;
        }

        // Index aus JSON auslesen
        int idx = doc["idx"] | -1;
        if(idx < 0){
            Serial.println("Update failed: missing index");
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
            Serial.println("DB updated and saved");
        } else {
            Serial.println("DB update failed: invalid index");
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
                Serial.print("ADD: JSON parse failed: ");
                Serial.println(err.c_str());
                return;
            }

            FilamentEntry entry;
            entry.uid      = doc["uid"].as<String>();
            entry.vendor   = doc["vendor"].as<String>();
            entry.type     = doc["type"].as<String>();
            entry.color    = doc["color"].as<String>();
            entry.ledIndex = doc["ledIndex"].as<int>();

            if (FilamentDB::add(entry)) {
                Serial.println("ADD: OK");
                saveFilamentsToFile();
            } else {
                Serial.println("ADD: FAILED");
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
                        Serial.printf("LED: count=%d, pin=%d, brightness=%d, timeout=%d, color=0x%06X\n",
                                    CONFIG.led.count, CONFIG.led.pin, CONFIG.led.brightness, CONFIG.led.timeout, CONFIG.led.color);
                        Serial.printf("NFC: count=%d, pin=%d, brightness=%d, timeout=%d, success=0x%06X, error=0x%06X, pulse=0x%06X\n",
                                    CONFIG.nfc.count, CONFIG.nfc.pin, CONFIG.nfc.brightness, CONFIG.nfc.timeout,
                                    CONFIG.nfc.colorSuccess, CONFIG.nfc.colorError, CONFIG.nfc.colorPulse);
                        Serial.printf("NFC Blink: enabled=%d, count=%d, ms=%d\n",
                                    CONFIG.nfc.successBlinkEnabled, CONFIG.nfc.successBlinkCount, CONFIG.nfc.successBlinkMs);
                        Serial.printf("Debug Mode: %s\n", CONFIG.debugMode ? "ON" : "OFF");
                    }
                    
                    if (err) {
                        req->send(400, "text/plain", "JSON Error");
                        Serial.print("updateConfig JSON error: ");
                        Serial.println(err.c_str());
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
                showRebootScreen();
                rebootPending = true;
                rebootAt = millis() + 1000;   // 1 Sekunde Zeit fürs Display

                request->send(200, "text/plain", "Rebooting");

            });


    


  



    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.serveStatic("/admin.js", LittleFS, "/admin.js")
          .setCacheControl("max-age=86400");

    server.serveStatic("/admin.css", LittleFS, "/admin.css")
          .setCacheControl("max-age=86400");

    server.begin();
}
