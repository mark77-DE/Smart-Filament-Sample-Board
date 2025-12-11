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
   server.on("/api/export", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!req->hasParam("file")) {
        req->send(400, "text/plain", "Missing file parameter");
        return;
    }

    String which = req->getParam("file")->value();
    String path;

    if (which == "filaments") {
        path = "/filaments.json";
    } else if (which == "config") {
        path = "/config.json";
    } else {
        req->send(400, "text/plain", "Unknown file");
        return;
    }

    File f = LittleFS.open(path, "r");
    if(!f){
        req->send(404, "text/plain", "File not found");
        return;
    }

    req->send(f, path, "application/json", true);
});


    // Import DB
    server.on("/api/import", HTTP_POST,
        [](AsyncWebServerRequest *req){
            req->send(200, "text/plain", "Upload OK");
        },
        [](AsyncWebServerRequest *req, String filename, size_t index,
           uint8_t *data, size_t len, bool final)
        {
            static File uploadFile;

            if(index == 0){
                uploadFile = LittleFS.open("/filaments.json", "w");
                if(!uploadFile) return;
            }

            if(uploadFile){
                uploadFile.write(data, len);
            }

            if(final){
                uploadFile.close();
                FilamentDB::loadFromFile();
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
    DynamicJsonDocument doc(256);

    // Beispielwerte
    JsonObject layout = doc.createNestedObject("layout");
    layout["columns"] = 2;
    layout["rows"] = 5;

    JsonObject options = doc.createNestedObject("options");
    options["darkmode"] = true;
    options["mqtt"] = false;
    options["ledCount"] = LED_COUNT;

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
});

server.on("/logo.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/logo.png", "image/png");
});


    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.begin();
}
