#include "config_service.h"

#include <cassert>
#include <cstring>
#include <iostream>

int main() {
    PanelConfig config = {};
    String error;
    assert(config_service_parse_climate_devices(
        R"([{"entity_id":"climate.office","label":"Office"},{"entity_id":"climate.bedroom","label":"Bedroom heat pump"}])",
        config, error));
    assert(config.climate_device_count == 2);
    assert(strcmp(config.climate_devices[0].entity_id, "climate.office") == 0);
    assert(strcmp(config.climate_devices[1].label, "Bedroom heat pump") == 0);

    const PanelConfig before = config;
    for (const char *bad : {
             R"([{"entity_id":"sensor.office","label":"Office"}])",
             R"([{"entity_id":"climate.office","label":""}])",
             R"([{"entity_id":"climate.office","label":"Office"},{"entity_id":"climate.office","label":"Duplicate"}])",
             R"([{"entity_id":"climate.one","label":"One"},{"entity_id":"climate.two","label":"Two"},{"entity_id":"climate.three","label":"Three"},{"entity_id":"climate.four","label":"Four"},{"entity_id":"climate.five","label":"Five"}])"}) {
        assert(!config_service_parse_climate_devices(bad, config, error));
        assert(memcmp(&config, &before, sizeof(config)) == 0);
    }
    assert(config_service_parse_climate_devices("[]", config, error));
    assert(config.climate_device_count == 0);
    std::cout << "Climate configuration parser tests passed.\n";
}
