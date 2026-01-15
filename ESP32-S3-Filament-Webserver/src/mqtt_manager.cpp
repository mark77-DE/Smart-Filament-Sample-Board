// mqtt_manager.cpp
#include "mqtt_manager.h"
#include "display_anim.h"
#include "ledctrl_filament.h"
#include "ledctrl_nfc.h"
#include "filehandling.h"
#include "config.h"
#include "ha_discovery.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    payload[length] = '\0';
    String msg = (char*)payload;

    String t = topic;

    if (t.endsWith("/animation/set")) {
        if (msg == "ON") {
            DisplayAnim::startIdle();
        } else if (msg == "OFF") {
            DisplayAnim::stop();
        }
    }

    if (t.endsWith("/leds/set")) {
        if (msg == "OFF") {
            LEDCTRL_FILAMENT::allOff();
            LEDCTRL_NFC::allOff();
        }
    }
}

void mqttInit() {
    if (!CONFIGV2.mqttConfig.enabled) return;

    mqttClient.setServer(CONFIGV2.mqttConfig.server.c_str(), CONFIGV2.mqttConfig.port);
    mqttClient.setCallback(mqttCallback);
}

static void mqttReconnect() {
    if (mqttClient.connected()) return;

    if (mqttClient.connect(
        CONFIGV2.mqttConfig.clientId.c_str(),
        CONFIGV2.mqttConfig.user.c_str(),
        CONFIGV2.mqttConfig.password.c_str()
    )) {
        String base = CONFIGV2.mqttConfig.baseTopic;

        mqttClient.subscribe((base + "/animation/set").c_str());
        mqttClient.subscribe((base + "/leds/set").c_str());

        // Home Assistant Discovery
        publishHADiscovery();
    }
}

void mqttLoop() {
    if (!mqttCfg.enabled) return;

    if (!mqttClient.connected()) {
        mqttReconnect();
    }
    mqttClient.loop();
}

bool mqttIsConnected() {
    return mqttClient.connected();
}
