#include "config_service.h"

#include <cassert>
#include <cstring>
#include <iostream>

int main() {
    PanelConfig config = {};
    String error;
    assert(config_service_parse_security_devices(
        R"([{"entity_id":"binary_sensor.front_door","label":"Front door","icon":"door","abnormal_states":"on,open","normal_label":"Closed","abnormal_label":"Open","color":"red","reverse_abnormal":false},{"entity_id":"binary_sensor.alarm_network","label":"Alarm network","icon":"power","abnormal_states":"on","normal_label":"Online","abnormal_label":"Offline","color":"yellow","reverse_abnormal":true}])",
        config, error));
    assert(config.security_device_count == 2);
    assert(strcmp(config.security_devices[0].entity_id, "binary_sensor.front_door") == 0);
    assert(config.security_devices[1].reverse_abnormal);
    assert(config_service_parse_security_dynamic_devices(
        R"([{"entity_id":"binary_sensor.back_gate","label":"Back gate","icon":"door","abnormal_states":"on","normal_label":"Closed","abnormal_label":"Open","color":"yellow","reverse_abnormal":false}])",
        config, error));
    assert(config.security_dynamic_device_count == 1);
    assert(strcmp(config.security_dynamic_devices[0].entity_id,
                  "binary_sensor.back_gate") == 0);
    assert(config_service_validate_security_device_uniqueness(config, error));

    auto dynamic_json = [](unsigned count) {
        String json = "[";
        for (unsigned i = 0; i < count; ++i) {
            if (i) json += ",";
            json += "{\"entity_id\":\"binary_sensor.dynamic_" + std::to_string(i) +
                    "\",\"label\":\"Dynamic " + std::to_string(i) +
                    "\",\"icon\":\"alert\",\"abnormal_states\":\"on\","
                    "\"normal_label\":\"Normal\",\"abnormal_label\":\"Attention\","
                    "\"color\":\"red\"}";
        }
        json += "]";
        return json;
    };
    assert(config_service_parse_security_dynamic_devices(
        dynamic_json(PANEL_MAX_SECURITY_DYNAMIC_DEVICES), config, error));
    assert(config.security_dynamic_device_count == PANEL_MAX_SECURITY_DYNAMIC_DEVICES);
    assert(strcmp(config.security_dynamic_devices[15].entity_id,
                  "binary_sensor.dynamic_15") == 0);

    const PanelConfig before = config;
    assert(!config_service_parse_security_dynamic_devices(
        dynamic_json(PANEL_MAX_SECURITY_DYNAMIC_DEVICES + 1), config, error));
    assert(error.find("at most 16") != String::npos);
    assert(memcmp(&config, &before, sizeof(config)) == 0);
    for (const char *bad : {
             R"([{"entity_id":"front_door","label":"Door","icon":"door","abnormal_states":"on","color":"red"}])",
             R"([{"entity_id":"binary_sensor.door","label":"Door","icon":"temperature","abnormal_states":"on","color":"red"}])",
             R"([{"entity_id":"binary_sensor.door","label":"Door","icon":"door","abnormal_states":"","color":"red"}])",
             R"([{"entity_id":"binary_sensor.door","label":"Door","icon":"door","abnormal_states":"on,open,opening,unlocked,jammed,detected,problem,unsafe,smoke,heat,wet,not_home","color":"red"}])",
             R"([{"entity_id":"binary_sensor.door","label":"Door","icon":"door","abnormal_states":"on","color":"green"}])",
             R"([{"entity_id":"binary_sensor.door","label":"Door","icon":"door","abnormal_states":"on","color":"red"},{"entity_id":"binary_sensor.door","label":"Again","icon":"door","abnormal_states":"on","color":"red"}])"}) {
        assert(!config_service_parse_security_devices(bad, config, error));
        assert(memcmp(&config, &before, sizeof(config)) == 0);
    }
    assert(config_service_parse_security_dynamic_devices(
        R"([{"entity_id":"binary_sensor.front_door","label":"Duplicate","icon":"door","abnormal_states":"on","color":"red"}])",
        config, error));
    assert(!config_service_validate_security_device_uniqueness(config, error));
    config = before;
    assert(config_service_parse_security_devices("[]", config, error));
    assert(config.security_device_count == 0);
    std::cout << "Security monitored/dynamic configuration parser tests passed.\n";
}
