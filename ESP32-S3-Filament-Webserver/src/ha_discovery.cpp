#include "globals.h"
#include "ha_discovery.h"
#include "config.h"

void publishHADiscovery(
    PubSubClient &client,
    const String &discoveryPrefix)
{
    String base = CONFIGV2.mqttConfig.baseTopic;
    String node = CONFIGV2.mqttConfig.clientId;

    if (CONFIGV2.system.debugMode)
    {
        Serial.println("Publishing Home Assistant Discovery:");
        Serial.println("  Base topic: " + base);
        Serial.println("  Discovery prefix: " + discoveryPrefix);
        Serial.println("  Node ID: " + node);
        Serial.println();
    }

    // -------------------- Device Block --------------------
    String deviceBlock = "{";
    deviceBlock += "\"identifiers\":[\"" + node + "\"],";
    deviceBlock += "\"name\":\"Spot My Filament\",";
    deviceBlock += "\"manufacturer\":\"DIY\",";
    deviceBlock += "\"model\":\"ESP32-S3\"";
    deviceBlock += "}";

    // -------------------- Animation Switch --------------------
    String animationPayload = "{";
    animationPayload += "\"name\":\"Animation\",";
    animationPayload += "\"command_topic\":\"" + base + "/animation/set\",";
    animationPayload += "\"state_topic\":\"" + base + "/animation/state\",";
    animationPayload += "\"payload_on\":\"ON\",";
    animationPayload += "\"payload_off\":\"OFF\",";
    animationPayload += "\"unique_id\":\"" + node + "_animation\",";
    animationPayload += "\"device\":" + deviceBlock;
    animationPayload += "}";

    String topic = discoveryPrefix + "/light/" + node + "/animation/config";
    bool ok = client.publish(topic.c_str(), animationPayload.c_str(), true);
    if (CONFIGV2.system.debugMode)
    {
        Serial.println("Animation discovery published: " + String(ok ? "SUCCESS" : "FAILED"));
        Serial.println();
    }

    // -------------------- Filament Sensors --------------------
    struct SensorDef
    {
        const char *name;
        const char *key;
        const char *id;
    };
    SensorDef sensors[] = {
        {"Filament UID", "uid", "filament_uid"},
        {"Filament Vendor", "vendor", "filament_vendor"},
        {"Filament Type", "type", "filament_type"},
        {"Filament Color", "color", "filament_color"},
        {"Filament LED Index", "led_index", "filament_led_index"}};

    for (auto s : sensors)
    {
        String sensorPayload = "{";
        sensorPayload += "\"name\":\"" + String(s.name) + "\",";
        sensorPayload += "\"state_topic\":\"" + base + "/filament/state\",";
        sensorPayload += "\"value_template\":\"{{ value_json." + String(s.key) + " }}\",";
        sensorPayload += "\"unique_id\":\"" + node + "_" + String(s.id) + "\",";
        sensorPayload += "\"device\":" + deviceBlock;
        sensorPayload += "}";

        topic = discoveryPrefix + "/sensor/" + node + "/" + String(s.id) + "/config";
        ok = client.publish(topic.c_str(), sensorPayload.c_str(), true);

        if (CONFIGV2.system.debugMode)
        {
            Serial.println(String(s.name) + " discovery published: " + String(ok ? "SUCCESS" : "FAILED"));
            Serial.println(sensorPayload);
            Serial.println();
        }
    }
}
