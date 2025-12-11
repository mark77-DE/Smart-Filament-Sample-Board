#include "my_webserver.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "ledctrl.h"

extern int LED_COUNT;
extern int LED_BRIGHTNESS;
extern int targetLed;
extern void setLedBrightness(int led, int brightness);
extern String lastScannedUID;

String lastScannedUID = "";

// ----------------- WebSocket Event -----------------
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if(type != WS_EVT_DATA) return;

    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if(info->opcode != WS_TEXT) return;

    String msg;
    msg.reserve(len);
    for(size_t i=0; i<len; i++) msg += (char)data[i];

    JsonDocument doc;
    if(deserializeJson(doc, msg)) return;

    // --------- HIGHLIGHT LED (Klick im UI) ----------
    if(doc["action"].is<String>() && doc["action"] == "highlightLED")
{
    String uid = doc["uid"];
    handleUID(uid); // <-- NICHT mehr notifyUID(), sondern zentrale handleUID()
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

        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        for(auto &e : list){
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

    // Export DB
   #include <ArduinoJson.h>
#include <LittleFS.h>

// --- Export ALL (filaments + config) ---
server.on("/api/exportAll", HTTP_GET, [](AsyncWebServerRequest *req){
    // filaments.json
    File f1 = LittleFS.open("/filaments.json", "r");
    if(!f1){
        req->send(404, "text/plain", "filaments.json not found");
        return;
    }
    String filamentsStr = f1.readString();
    f1.close();

    // config.json
    File f2 = LittleFS.open("/config.json", "r");
    if(!f2){
        req->send(404, "text/plain", "config.json not found");
        return;
    }
    String configStr = f2.readString();
    f2.close();

    // alles in ein JSON packen
    DynamicJsonDocument doc(128*1024); // 32kB sollten reichen
    JsonDocument filamentsDoc;
    deserializeJson(filamentsDoc, filamentsStr);
    doc["filaments"] = filamentsDoc.as<JsonArray>();

    JsonDocument configDoc;
    deserializeJson(configDoc, configStr);
    doc["config"] = configDoc.as<JsonObject>();

    String out;
    serializeJson(doc, out);

    req->send(200, "application/json", out);
});

// --- Import ALL ---
server.on("/api/importAll", HTTP_POST,
    [](AsyncWebServerRequest *req){
        req->send(200, "text/plain", "Upload started");
    },
    nullptr,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
        static String body;
        if(index == 0) body = "";
        for(size_t i = 0; i < len; i++) body += (char)data[i];
        if(index + len != total) return;

        DynamicJsonDocument doc(128*1024);
        if(deserializeJson(doc, body)){
            req->send(400, "text/plain", "JSON parse failed");
            return;
        }
        req->send(200, "text/plain", "Import OK");

        // Filaments speichern
        if(doc.containsKey("filaments")){
            File f = LittleFS.open("/filaments.json", "w");
            if(f){
                String out;
                serializeJson(doc["filaments"], out);
                f.print(out);
                f.close();
                FilamentDB::loadFromFile(); // falls du direkt DB neu laden willst
                Serial.println("Filaments imported");
            }
        }

        // Config speichern
        if(doc.containsKey("config")){
            File f = LittleFS.open("/config.json", "w");
            if(f){
                String out;
                serializeJson(doc["config"], out); // die gesamte JSON-Struktur speichern
                f.print(out);
                f.close();
                loadConfig();
                LEDCTRL::init(LED_COUNT, LED_PIN);
                Serial.println("Config imported");
            }
        }
    }
);


    // Update eines Eintrags
server.on("/api/update", HTTP_POST,
    [](AsyncWebServerRequest *req){
        req->send(200, "text/plain", "Processing");
    },
    nullptr, // kein Upload-Handler
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
        static String body;
        if(index == 0) body = "";
        for(size_t i = 0; i < len; i++) body += (char)data[i];
        if(index + len != total) return;

        Serial.println("Received body: " + body);

        DynamicJsonDocument doc(512);
        DeserializationError err = deserializeJson(doc, body);
        if(err){
            Serial.println("JSON parse failed");
            return;
        }

        FilamentEntry entry;
        entry.uid      = doc["uid"].as<String>();
        entry.vendor   = doc["vendor"].as<String>();
        entry.type     = doc["type"].as<String>();
        entry.color    = doc["color"].as<String>();
        entry.ledIndex = doc["ledIndex"].as<int>();

        if(FilamentDB::update(entry)){
            FilamentDB::saveToFile();
            Serial.println("DB updated and saved");
        } else {
            Serial.println("DB update failed: UID not found");
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
        if (index == 0) body = "";
        for (size_t i = 0; i < len; i++) body += (char)data[i];
        if (index + len != total) return;

        DynamicJsonDocument doc(512);
        if (deserializeJson(doc, body)) {
            Serial.println("ADD: JSON parse failed");
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


server.on("/api/updateLedConfig", HTTP_POST, 
    [](AsyncWebServerRequest *req){},
    nullptr,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
        static String body;
        if(index == 0) body = "";
        body += String((char*)data).substring(0, len);

        if(index + len != total) return;

        StaticJsonDocument<256> doc;
        if(deserializeJson(doc, body)){
            req->send(400, "text/plain", "JSON Error");
            return;
        }

        int newCount      = doc["ledCount"] | 8;
        int newPin        = doc["ledPin"]   | 5;
        int newBrightness = doc["ledBrightness"] | 50;

        // Farbe aus dem Request
        JsonArray colorArr = doc["ledColor"];

        // Config.json laden
        StaticJsonDocument<1024> configDoc;
        File f = LittleFS.open("/config.json", "r");
        if(f){
            deserializeJson(configDoc, f);
            f.close();
        }

        configDoc["options"]["ledCount"] = newCount;
        configDoc["options"]["ledPin"]   = newPin;
        configDoc["options"]["ledBrightness"] = newBrightness;

        // Farbe korrekt kopieren
        JsonArray col = configDoc["options"]["ledColor"].to<JsonArray>();
        col.clear();
        for (int i = 0; i < 3; i++) col.add(colorArr[i].as<int>());

        // zurückschreiben
        f = LittleFS.open("/config.json", "w");
        if(f){
            serializeJson(configDoc, f);
            f.close();
        }

        req->send(200, "text/plain", "REBOOTING");
        delay(200);
        ESP.restart();
    }
);






    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.begin();
}

