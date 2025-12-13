#include "my_webserver.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "ledctrl.h"
#include "ledctrl_nfc.h"
#include <vector>
#include "display.h"

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
    DynamicJsonDocument doc(1024); // oder StaticJsonDocument<N> / DynamicJsonDocument<N> je nach UseCase

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
            handleUID(uid); // zentrale handleUID()
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
        DynamicJsonDocument doc(4096);
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
    server.on("/api/exportAll", HTTP_GET, [](AsyncWebServerRequest *req){
        // filaments.json
        File f1 = LittleFS.open("/filaments.json", "r");
        if (!f1) {
            req->send(404, "text/plain", "filaments.json not found");
            return;
        }
        String filamentsStr = f1.readString();
        f1.close();

        // config.json
        File f2 = LittleFS.open("/config.json", "r");
        if (!f2) {
            req->send(404, "text/plain", "config.json not found");
            return;
        }
        String configStr = f2.readString();
        f2.close();

        // alles in ein JSON packen
        // großer Puffer, weil beide Dateien enthalten werden
        DynamicJsonDocument outDoc(128 * 1024);
        
        // config (als Object)
        DynamicJsonDocument configDoc(8 * 1024);
        DeserializationError cerr = deserializeJson(configDoc, configStr);
        if (!cerr) {
            outDoc["config"] = configDoc.as<JsonObject>();
        } else {
            outDoc.createNestedObject("config");
        }

        // filaments (als Array)
        DynamicJsonDocument filamentsDoc(32 * 1024);
        DeserializationError ferr = deserializeJson(filamentsDoc, filamentsStr);
        if (!ferr) {
            outDoc["filaments"] = filamentsDoc.as<JsonArray>();
        } else {
            // falls filaments.json kein korrektes JSON ist, lege leeres Array an
            outDoc.createNestedArray("filaments");
        }

        String out;
        serializeJsonPretty(outDoc, out);
        req->send(200, "application/json", out);
    });

    // --- Import ALL ---
    server.on("/api/importAll", HTTP_POST,
        [](AsyncWebServerRequest *req){
            // ACK sofort (Upload beginnt)
            req->send(200, "text/plain", "Upload started");
        },
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
            static String body;
            if (index == 0) {
                body = "";
                if (total > 0) body.reserve(total);
            }
            // sichere Concatenation
            body.concat((const char*)data, len);

            if (index + len != total) return; // noch nicht komplett

            DynamicJsonDocument doc(128 * 1024);
            DeserializationError err = deserializeJson(doc, body);
            if (err) {
                req->send(400, "text/plain", "JSON parse failed");
                Serial.print("importAll JSON parse failed: ");
                Serial.println(err.c_str());
                return;
            }
            req->send(200, "text/plain", "Import OK");

            // Filaments speichern
            if (doc.containsKey("filaments")) {
                File f = LittleFS.open("/filaments.json", "w");
                if (f) {
                    String out;
                    serializeJson(doc["filaments"], out);
                    f.print(out);
                    f.close();
                    FilamentDB::loadFromFile(); // DB neu laden
                    Serial.println("Filaments imported");
                } else {
                    Serial.println("Failed to open /filaments.json for writing");
                }
            }

            // Config speichern
            if (doc.containsKey("config")) {
                File f = LittleFS.open("/config.json", "w");
                if (f) {
                    String out;
                    serializeJson(doc["config"], out); // gesamte Struktur speichern
                    f.print(out);
                    f.close();
                    loadLedConfig();
                    loadNfcLedConfig();
                    // LEDCTRL::init erwartet evtl. LED_PIN extern definiert
                    
                    //Serial.println("Config imported, ESP will reboot... Please refresh browser in 5s...");
                    //delay(500);
                    //ESP.restart();

                } else {
                    Serial.println("Failed to open /config.json for writing");
                }
            }
        }
    );

    // Update eines Eintrags
    // Update eines Eintrags
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

        DynamicJsonDocument doc(1024);
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
            FilamentDB::saveToFile();
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

            DynamicJsonDocument doc(1024); // oder StaticJsonDocument<N> / DynamicJsonDocument<N> je nach UseCase

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
                FilamentDB::saveToFile();
            } else {
                Serial.println("ADD: FAILED");
            }
        }
    );

    server.on("/api/delete", HTTP_POST, [] (AsyncWebServerRequest *request) {
        if (!request->hasParam("index", true)) {
            request->send(400, "application/json", "{\"status\":\"error\",\"msg\":\"missing index\"}");
            return;
        }

        int index = request->getParam("index", true)->value().toInt();

        if (FilamentDB::deleteEntry(index)) {
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(500, "application/json", "{\"status\":\"error\",\"msg\":\"delete failed\"}");
        }
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
    server.on("/api/updateLedConfig", HTTP_POST,
        [](AsyncWebServerRequest *req){},
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
            static String body;
            if (index == 0) {
                body = "";
                if (total > 0) body.reserve(total);
            }
            // previous buggy code used String((char*)data).substring(0,len) -> unsafe
            body.concat((const char*)data, len);

            if (index + len != total) return;

            StaticJsonDocument<512> doc;
            DeserializationError err = deserializeJson(doc, body);
            if (err) {
                req->send(400, "text/plain", "JSON Error");
                Serial.print("updateLedConfig JSON error: ");
                Serial.println(err.c_str());
                return;
            }

            int newCount            = doc["ledCount"] | 8;
            int newPin              = doc["ledPin"]   | 4;
            int newBrightness       = doc["ledBrightness"] | 50;
            int newNfcCount         = doc["nfcLedCount"] | 8;
            int newNfcPin           = doc["nfcLedPin"]   | 16;
            int newNfcBrightness    = doc["nfcLedBrightness"] | 50;

            Serial.println("Updating LED Config:");
            Serial.printf(" LED Count: %d\n", newCount);
            Serial.printf(" LED Pin: %d\n", newPin);
            Serial.printf(" LED Brightness: %d\n", newBrightness); 
            Serial.printf(" NFC LED Count: %d\n", newNfcCount);
            Serial.printf(" NFC LED Pin: %d\n", newNfcPin);
            Serial.printf(" NFC LED Brightness: %d\n", newNfcBrightness);



            // Farbe aus dem Request
            JsonArray colorArr      = doc["ledColor"].as<JsonArray>();
            JsonArray colorNfcArr   = doc["ledNfcColor"].as<JsonArray>();

            // Config.json laden
            StaticJsonDocument<1024> configDoc;
            File f = LittleFS.open("/config.json", "r");
            if (f) {
                DeserializationError rerr = deserializeJson(configDoc, f);
                if (rerr) {
                    Serial.print("Failed to parse existing config.json: ");
                    Serial.println(rerr.c_str());
                }
                f.close();
            }

            configDoc["options"]["ledCount"] = newCount;
            configDoc["options"]["ledPin"]   = newPin;
            configDoc["options"]["ledBrightness"] = newBrightness;
            configDoc["options"]["ledNfcCount"] = newNfcCount;
            configDoc["options"]["nfcLedPin"]   = newNfcPin;
            configDoc["options"]["ledNfcBrightness"] = newNfcBrightness;

            // Farbe korrekt kopieren (sichere Prüfung)
            if (colorArr.size() >= 3) {
                JsonArray col = configDoc["options"]["ledColor"].to<JsonArray>();
                col.clear();
                for (int i = 0; i < 3; i++) col.add(colorArr[i].as<int>());
            }

            if (colorNfcArr.size() >= 3) {
                JsonArray col = configDoc["options"]["ledNfcColor"].to<JsonArray>();
                col.clear();
                for (int i = 0; i < 3; i++) col.add(colorNfcArr[i].as<int>());
            }

            // zurückschreiben
            f = LittleFS.open("/config.json", "w");
            if (f) {
                serializeJson(configDoc, f);
                f.close();
            } else {
                Serial.println("Failed to open /config.json for writing (updateLedConfig)");
            }

            loadLedConfig();
            loadNfcLedConfig();

            
            req->send(200, "application/json", "{\"status\":\"ok\"}");

            //req->send(200, "text/plain", "REBOOTING");
            //delay(500);
            //ESP.restart();
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
