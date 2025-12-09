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
        File f = LittleFS.open("/filaments.json", "r");
        if(!f){
            req->send(404, "text/plain", "File not found");
            return;
        }
        req->send(f, "/filaments.json", "application/json", true);
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

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.begin();
}
