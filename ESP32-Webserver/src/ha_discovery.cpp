#include "globals.h"
#include "ha_discovery.h"
#include "config.h"
#include "version_info.h"

void publishHADiscovery(
    PubSubClient &client,
    const String &discoveryPrefix)
{
    String base = CONFIGV2.mqttConfig.baseTopic;
    String node = CONFIGV2.mqttConfig.clientId;

    // ---------------- Device Block ----------------
    String deviceBlock = "{";
    deviceBlock += "\"identifiers\":[\"" + node + "\"],";
    deviceBlock += "\"name\":\"Spot My Filament\",";
    deviceBlock += "\"manufacturer\":\"DIY\",";
    deviceBlock += "\"model\":\"ESP32-S3\",";
    deviceBlock += "\"sw_version\":\"";
    deviceBlock += FIRMWARE_VERSION;
    deviceBlock += " (";
    deviceBlock += GIT_HASH;
    deviceBlock += ")\",";
    deviceBlock += "\"configuration_url\":\"http://";
    deviceBlock += WiFi.localIP().toString();
    deviceBlock += "\"";
    deviceBlock += "}";

    // ---------------- Animation ----------------
    String animationPayload = "{";
    animationPayload += "\"name\":\"Animation\",";
    animationPayload += "\"command_topic\":\"" + base + "/animation/set\",";
    animationPayload += "\"state_topic\":\"" + base + "/animation/state\",";
    animationPayload += "\"payload_on\":\"ON\",";
    animationPayload += "\"payload_off\":\"OFF\",";
    animationPayload += "\"unique_id\":\"" + node + "_animation\",";
    animationPayload += "\"device\":" + deviceBlock;
    animationPayload += "}";

    client.publish(
        (discoveryPrefix + "/light/" + node + "/animation/config").c_str(),
        animationPayload.c_str(),
        true
    );

    // ---------------- Filament Sensoren ----------------
    struct SensorDef {
        const char *name;
        const char *key;
        const char *id;
    };

    SensorDef sensors[] = {
        {"Filament UID", "uid", "filament_uid"},
        {"Filament Vendor", "vendor", "filament_vendor"},
        {"Filament Type", "type", "filament_type"},
        {"Filament Color", "color", "filament_color"},
        {"Filament Storage", "storage", "filament_storage"},
        {"Filament LED Index", "led_index", "filament_led_index"}
    };

    for (auto s : sensors) {

        String payload = "{";
        payload += "\"name\":\"" + String(s.name) + "\",";
        payload += "\"state_topic\":\"" + base + "/filament/state\",";
        payload += "\"value_template\":\"{{ value_json." + String(s.key) + " }}\",";
        payload += "\"unique_id\":\"" + node + "_" + String(s.id) + "\",";
        payload += "\"device\":" + deviceBlock;
        payload += "}";

        client.publish(
            (discoveryPrefix + "/sensor/" + node + "/" + String(s.id) + "/config").c_str(),
            payload.c_str(),
            true
        );
    }

    // ---------------- IP Sensor ----------------
    String ipPayload = "{";
    ipPayload += "\"name\":\"IP Address\",";
    ipPayload += "\"state_topic\":\"" + base + "/device/ip\",";
    ipPayload += "\"unique_id\":\"" + node + "_ip\",";
    ipPayload += "\"entity_category\":\"diagnostic\",";
    ipPayload += "\"device\":" + deviceBlock;
    ipPayload += "}";

    client.publish(
        (discoveryPrefix + "/sensor/" + node + "/ip/config").c_str(),
        ipPayload.c_str(),
        true
    );

    // ---------------- Firmware Sensor ----------------
    String fwPayload = "{";
    fwPayload += "\"name\":\"Firmware Version\",";
    fwPayload += "\"state_topic\":\"" + base + "/device/fw\",";
    fwPayload += "\"unique_id\":\"" + node + "_fw\",";
    fwPayload += "\"entity_category\":\"diagnostic\",";
    fwPayload += "\"device\":" + deviceBlock;
    fwPayload += "}";

    client.publish(
        (discoveryPrefix + "/sensor/" + node + "/fw/config").c_str(),
        fwPayload.c_str(),
        true
    );

    // ---------------- Build Info ----------------
    String buildPayload = "{";
    buildPayload += "\"name\":\"Build Info\",";
    buildPayload += "\"state_topic\":\"" + base + "/device/build\",";
    buildPayload += "\"unique_id\":\"" + node + "_build\",";
    buildPayload += "\"entity_category\":\"diagnostic\",";
    buildPayload += "\"device\":" + deviceBlock;
    buildPayload += "}";

    client.publish(
        (discoveryPrefix + "/sensor/" + node + "/build/config").c_str(),
        buildPayload.c_str(),
        true
    );

    // ---------------- Update Entity ----------------
    String updatePayload = "{";
    updatePayload += "\"name\":\"Firmware Update\",";
    updatePayload += "\"state_topic\":\"" + base + "/device/update\",";
    updatePayload += "\"command_topic\":\"" + base + "/device/update/install\",";
    updatePayload += "\"payload_install\":\"INSTALL\",";
    updatePayload += "\"unique_id\":\"" + node + "_update\",";
    updatePayload += "\"entity_category\":\"diagnostic\",";
    updatePayload += "\"device\":" + deviceBlock;
    updatePayload += "}";

    client.publish(
        (discoveryPrefix + "/update/" + node + "/firmware/config").c_str(),
        updatePayload.c_str(),
        true
    );
}