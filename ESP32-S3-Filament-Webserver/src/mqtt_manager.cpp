// mqtt_manager.cpp
#include "globals.h"
#include "mqtt_manager.h"
#include "display_anim.h"
#include "ledctrl_filament.h"
#include "ledctrl_nfc.h"
#include "filehandling.h"
#include "config.h"
#include "ha_discovery.h"
#include "filament_db.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Payload als String
    payload[length] = '\0';
    String msg = (char*)payload;

    // Topic als String
    String t = topic;

    // --- Debug Ausgabe ---
    if (CONFIGV2.system.debugMode) {
        Serial.print("MQTT message received: ");
        Serial.println(msg);
        Serial.print("Topic: ");
        Serial.println(t);
        Serial.println();
    }

    // --- Animation ---
    if (t.endsWith("/animation/set")) {
    if (msg == "ON") {
        DisplayAnim::startIdle(millis());
        LEDCTRL_FILAMENT::standBy(false);
        LEDCTRL_NFC::standBy(false);
        publishAnimationStatus(true);
    }
    else if (msg == "OFF") {
        DisplayAnim::stop();
        MYDISPLAY::clear();
        LEDCTRL_FILAMENT::standBy(true);
        LEDCTRL_NFC::standBy(true);
        publishAnimationStatus(false);
    }
}

    
}


void mqttInit() {
    if (!CONFIGV2.mqttConfig.enabled) return;

    mqttClient.setServer(CONFIGV2.mqttConfig.server.c_str(), CONFIGV2.mqttConfig.port);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setKeepAlive(30);       // Sekunden
    mqttClient.setSocketTimeout(10);


    if(CONFIGV2.system.debugMode) {
        Serial.print("MQTT initialized with server: ");
        Serial.print(CONFIGV2.mqttConfig.server);
        Serial.print(":");
        Serial.println(CONFIGV2.mqttConfig.port);
    }   
}

static void mqttReconnect() {
    if (mqttClient.connected()) return;

    if(CONFIGV2.system.debugMode) {
        Serial.print(F("HA-Discovery enabled: ")); Serial.println(CONFIGV2.mqttConfig.haDiscovery ? F("true") : F("false"));
    }

    String base = CONFIGV2.mqttConfig.baseTopic;
    String lwTopic = base + "/status";

    

    if (mqttClient.connect(
        CONFIGV2.mqttConfig.clientId.c_str(),
        CONFIGV2.mqttConfig.user.c_str(),
        CONFIGV2.mqttConfig.password.c_str(),
        lwTopic.c_str(), 0, true, "offline"
        )) {

            


           
            mqttClient.publish(lwTopic.c_str(), "online", true);


            if(LEDCTRL_FILAMENT::_standby) {
                publishAnimationStatus(false); // Standby = Animation OFF
            } else {
                publishAnimationStatus(true);  // Normalbetrieb = Animation ON
            }

        
        

            mqttClient.subscribe((base + "/animation/set").c_str());
        

            if(CONFIGV2.system.debugMode) {
                Serial.println("MQTT connected and subscribed to topics:");
                Serial.println("  " + base + "/animation/set");
            
            }

            if(CONFIGV2.system.debugMode) {
                Serial.println("Publishing initial LED status...");
            }
            // ---- Home Assistant Discovery (optional) ----
            if (CONFIGV2.mqttConfig.haDiscovery) {
                publishHADiscovery(
                    mqttClient,
                    CONFIGV2.mqttConfig.haDiscoveryPrefix
                );

                if(CONFIGV2.system.debugMode) { 
                    Serial.println("HA Discovery published");
                    Serial.println();
                }
            }
        } else {
            if(CONFIGV2.system.debugMode) {
                Serial.print("MQTT connection failed, rc=");
                Serial.print(mqttClient.state());
                Serial.println(" try again in 5 seconds");
                Serial.println();
            }
        }
}


void mqttLoop() {

    if (!mqttClient.connected()) {
        Serial.println("MQTT disconnected, attempting reconnect...");
    }

    if (!CONFIGV2.mqttConfig.enabled) return;

    static uint32_t lastReconnectAttempt = 0;

    if (!mqttClient.connected()) {
        uint32_t now = millis();
        if (now - lastReconnectAttempt > 5000) {   // 5 Sekunden
            lastReconnectAttempt = now;
            mqttReconnect();
        }
        return;
    }

    mqttClient.loop();
}

bool mqttIsConnected() {
    return mqttClient.connected();
}



void publishAnimationStatus(bool on) {
    String base = CONFIGV2.mqttConfig.baseTopic;
    const char* msg = on ? "ON" : "OFF";

    mqttClient.publish(
        (base + "/animation/state").c_str(),
        msg,
        true   // retain!
    );
}




void publishFilamentState(const FilamentEntry& entry) {

    if (!mqttClient.connected()) return;

    String base = CONFIGV2.mqttConfig.baseTopic;

    String payload = "{";
    payload += "\"uid\":\"" + entry.uid + "\",";
    payload += "\"vendor\":\"" + entry.vendor + "\",";
    payload += "\"type\":\"" + entry.type + "\",";
    payload += "\"color\":\"" + entry.color + "\",";
    payload += "\"led_index\":" + String(entry.ledIndex + 1) + "";  // +1, damit es in HA bei 1 beginnt
    payload += "}";

    mqttClient.publish(
        (base + "/filament/state").c_str(),
        payload.c_str(),
        true   // retain sinnvoll!
    );
}
