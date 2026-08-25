#include "globals.h"
#include "mqtt_manager.h"
#include "display/display_anim.h"
#include "ledctrl_filament.h"
#include "ledctrl_nfc.h"
#include "filehandling.h"
#include "config.h"
#include "ha_discovery.h"
#include "filament_db.h"
#include "version_info.h"
#include "update_manager.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// --------------------------------------------------
// OTA Dummy
// --------------------------------------------------
void startOTAUpdate() {
    Serial.println("[OTA] Update requested -> NOT IMPLEMENTED YET");
}

// --------------------------------------------------
// MQTT Callback
// --------------------------------------------------
static void mqttCallback(char* topic, byte* payload, unsigned int length) {

    String msg;
    msg.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }

    String t = topic;

    if (CONFIGV2.system.debugMode) {
        Serial.println("MQTT message received:");
        Serial.println("  Topic: " + t);
        Serial.println("  Payload: " + msg);
        Serial.println();
    }

    String base = CONFIGV2.mqttConfig.baseTopic;

    // ---------------- Animation ----------------
    if (t == base + "/animation/set") {
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

    // ---------------- OTA Trigger ----------------
    if (t == base + "/device/update/install") {
        if (msg == "INSTALL") {
            startOTAUpdate();
        }
    }
}

// --------------------------------------------------
void mqttInit() {
    if (!CONFIGV2.mqttConfig.enabled) return;

    mqttClient.setServer(
        CONFIGV2.mqttConfig.server.c_str(),
        CONFIGV2.mqttConfig.port
    );

    mqttClient.setCallback(mqttCallback);
    mqttClient.setKeepAlive(30);
    mqttClient.setSocketTimeout(10);

    if(CONFIGV2.system.debugMode) {
        Serial.println("MQTT initialized:");
        Serial.println("  Server: " + CONFIGV2.mqttConfig.server);
        Serial.println("  Port: " + String(CONFIGV2.mqttConfig.port));
    }
}

// --------------------------------------------------
static void publishDeviceDiagnostics() {

    String base = CONFIGV2.mqttConfig.baseTopic;

    mqttClient.publish(
        (base + "/device/ip").c_str(),
        WiFi.localIP().toString().c_str(),
        true
    );

    mqttClient.publish(
        (base + "/device/fw").c_str(),
        FIRMWARE_VERSION,
        true
    );

    String build = String(FIRMWARE_VERSION) + " | " + GIT_HASH + " | " + BUILD_DATE;

    mqttClient.publish(
        (base + "/device/build").c_str(),
        build.c_str(),
        true
    );

    // OTA aktuell immer OFF
    mqttClient.publish(
        (base + "/device/update").c_str(),
        "OFF",
        true
    );
}

// --------------------------------------------------
static void mqttReconnect() {

    if (mqttClient.connected()) return;

    String base = CONFIGV2.mqttConfig.baseTopic;
    String lwTopic = base + "/status";

    if (mqttClient.connect(
        CONFIGV2.mqttConfig.clientId.c_str(),
        CONFIGV2.mqttConfig.user.c_str(),
        CONFIGV2.mqttConfig.password.c_str(),
        lwTopic.c_str(), 0, true, "offline"
    )) {

        mqttClient.publish(lwTopic.c_str(), "online", true);

        // Animation Status sync
        publishAnimationStatus(!LEDCTRL_FILAMENT::_standby);

        // Subscribe
        mqttClient.subscribe((base + "/animation/set").c_str());
        mqttClient.subscribe((base + "/device/update/install").c_str());

        if(CONFIGV2.system.debugMode) {
            Serial.println("MQTT connected");
        }

        // HA Discovery
        if (CONFIGV2.mqttConfig.haDiscovery) {
            publishHADiscovery(
                mqttClient,
                CONFIGV2.mqttConfig.haDiscoveryPrefix
            );
        }

        // Diagnosewerte senden
        publishDeviceDiagnostics();

        //Update yes/no
        publishUpdateStatus();

    } else {
        if(CONFIGV2.system.debugMode) {
            Serial.print("MQTT connect failed, rc=");
            Serial.println(mqttClient.state());
        }
    }
}

// --------------------------------------------------
void mqttLoop() {

    if (!CONFIGV2.mqttConfig.enabled) return;

    static uint32_t lastReconnectAttempt = 0;

    if (!mqttClient.connected()) {
        uint32_t now = millis();
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            mqttReconnect();
        }
        return;
    }

    mqttClient.loop();
}

// --------------------------------------------------
bool mqttIsConnected() {
    return mqttClient.connected();
}

// --------------------------------------------------
void publishAnimationStatus(bool on) {

    String base = CONFIGV2.mqttConfig.baseTopic;

    mqttClient.publish(
        (base + "/animation/state").c_str(),
        on ? "ON" : "OFF",
        true
    );
}

// --------------------------------------------------
void publishFilamentState(const FilamentEntry& entry) {

    if (!mqttClient.connected()) return;

    String base = CONFIGV2.mqttConfig.baseTopic;

    String payload = "{";
    payload += "\"uid\":\"" + entry.uid + "\",";
    payload += "\"vendor\":\"" + entry.vendor + "\",";
    payload += "\"type\":\"" + entry.type + "\",";
    payload += "\"color\":\"" + entry.color + "\",";
    payload += "\"storage\":\"" + entry.storage + "\",";
    payload += "\"led_index\":" + String(entry.ledIndex + 1);
    payload += "}";

    mqttClient.publish(
        (base + "/filament/state").c_str(),
        payload.c_str(),
        true
    );
}

void publishUpdateStatus() {

    if (!mqttClient.connected()) return;

    const UpdateInfo& info = getUpdateInfo();
    String base = CONFIGV2.mqttConfig.baseTopic;

    // ON/OFF für HA Update Entity
    mqttClient.publish(
        (base + "/device/update").c_str(),
        info.updateAvailable ? "ON" : "OFF",
        true
    );

    // Zusatzinfos (optional)
    mqttClient.publish(
        (base + "/device/update/latest").c_str(),
        info.latestVersion.c_str(),
        true
    );
}