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

    const PanelConfig before = config;
    for (const char *bad : {
             R"([{"entity_id":"front_door","label":"Door","icon":"door","abnormal_states":"on","color":"red"}])",
             R"([{"entity_id":"binary_sensor.door","label":"Door","icon":"temperature","abnormal_states":"on","color":"red"}])",
             R"([{"entity_id":"binary_sensor.door","label":"Door","icon":"door","abnormal_states":"","color":"red"}])",
             R"([{"entity_id":"binary_sensor.door","label":"Door","icon":"door","abnormal_states":"on","color":"green"}])",
             R"([{"entity_id":"binary_sensor.door","label":"Door","icon":"door","abnormal_states":"on","color":"red"},{"entity_id":"binary_sensor.door","label":"Again","icon":"door","abnormal_states":"on","color":"red"}])"}) {
        assert(!config_service_parse_security_devices(bad, config, error));
        assert(memcmp(&config, &before, sizeof(config)) == 0);
    }
    assert(config_service_parse_security_devices("[]", config, error));
    assert(config.security_device_count == 0);
    std::cout << "Security configuration parser tests passed.\n";
}
