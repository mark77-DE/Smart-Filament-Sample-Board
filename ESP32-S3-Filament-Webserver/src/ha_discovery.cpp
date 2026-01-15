#include <PubSubClient.h>
#include "config.h"

extern PubSubClient mqttClient;

void publishHADiscovery() {
    String base = CONFIGV2.mqttConfig.baseTopic;
    String node = CONFIGV2.mqttConfig.clientId;

    // --- Animation Switch ---
    String animationPayload = "{"
        "\"name\": \"Filament Animation\","
        "\"command_topic\": \"" + base + "/animation/set\","
        "\"payload_on\": \"ON\","
        "\"payload_off\": \"OFF\","
        "\"unique_id\": \"" + node + "_animation\","
        "\"device\": {"
            "\"identifiers\": [\"" + node + "\"],"
            "\"name\": \"Spot My Filament\","
            "\"manufacturer\": \"DIY\","
            "\"model\": \"ESP32-S3\""
        "}"
    "}";

    mqttClient.publish(
        ("homeassistant/switch/" + node + "/animation/config").c_str(),
        animationPayload.c_str(),
        true
    );

    // --- LED Switch ---
    String ledPayload = "{"
        "\"name\": \"Filament LEDs\","
        "\"command_topic\": \"" + base + "/leds/set\","
        "\"payload_off\": \"OFF\","
        "\"unique_id\": \"" + node + "_leds\""
    "}";

    mqttClient.publish(
        ("homeassistant/switch/" + node + "/leds/config").c_str(),
        ledPayload.c_str(),
        true
    );
}
