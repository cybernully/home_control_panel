#include "config_service.h"

#include <ArduinoJson.h>
#include <string.h>

bool config_service_parse_climate_devices(const String &json, PanelConfig &config,
                                          String &error) {
    JsonDocument doc;
    if (deserializeJson(doc, json) || !doc.is<JsonArray>() ||
        doc.size() > PANEL_MAX_CLIMATE_DEVICES) {
        error = "Climate devices must be an array of at most four items.";
        return false;
    }

    PanelClimateDevice parsed[PANEL_MAX_CLIMATE_DEVICES] = {};
    size_t count = 0;
    for (JsonObject item : doc.as<JsonArray>()) {
        const char *entity_id = item["entity_id"] | "";
        const char *label = item["label"] | "";
        if (strncmp(entity_id, "climate.", 8) != 0 ||
            strlen(entity_id) >= sizeof(parsed[0].entity_id)) {
            error = "Each Climate device needs a valid climate.* entity ID.";
            return false;
        }
        if (!label[0] || strlen(label) >= sizeof(parsed[0].label)) {
            error = "Each Climate device needs a display name of at most 47 characters.";
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            if (strcmp(parsed[i].entity_id, entity_id) == 0) {
                error = "Each Climate entity may be added only once.";
                return false;
            }
        }
        snprintf(parsed[count].entity_id, sizeof(parsed[count].entity_id), "%s", entity_id);
        snprintf(parsed[count].label, sizeof(parsed[count].label), "%s", label);
        ++count;
    }

    memset(config.climate_devices, 0, sizeof(config.climate_devices));
    memcpy(config.climate_devices, parsed, sizeof(parsed));
    config.climate_device_count = static_cast<uint8_t>(count);
    return true;
}
