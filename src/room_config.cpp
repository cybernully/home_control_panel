#include "config_service.h"
#include <ArduinoJson.h>
#include <string.h>

bool config_service_parse_room_controls(const String &json, PanelConfig &config, String &error) {
    JsonDocument doc;
    if (deserializeJson(doc, json) || !doc.is<JsonArray>() || doc.size() > HA_MAX_AREA_ENTITIES) {
        error = "Room controls must be an array of at most 48 entries.";
        return false;
    }
    unsigned favorites = 0;
    for (size_t i = 0; i < doc.size(); ++i) {
        JsonVariant item = doc[i];
        const char *id = item["entity_id"] | "";
        const char *label = item["label"] | "";
        if (!item["entity_id"].is<const char *>() || !id[0] || strlen(id) >= 96 ||
            !item["label"].is<const char *>() || strlen(label) >= 64 ||
            !item["placement"].is<unsigned>() || item["placement"].as<unsigned>() > 2) {
            error = "Invalid room entry: entity, label (max 63 UTF-8 bytes), and placement 0-2 required.";
            return false;
        }
        const char *dot = strchr(id, '.');
        if (!dot || !dot[1] || !(strncmp(id,"light.",6)==0 || strncmp(id,"switch.",7)==0 ||
            strncmp(id,"fan.",4)==0 || strncmp(id,"cover.",6)==0 || strncmp(id,"scene.",6)==0)) {
            error = "Room controls support light, switch, fan, cover and scene entities.";
            return false;
        }
        for (const char *p = id; *p; ++p) {
            if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || p == dot)) {
                error = "Invalid room entity ID."; return false;
            }
        }
        for (size_t j = 0; j < i; ++j) {
            if (strcmp(id, doc[j]["entity_id"].as<const char *>()) == 0) {
                error = "Room entity IDs must be unique."; return false;
            }
        }
        if (item["placement"].as<unsigned>() == 1 && ++favorites > 6) {
            error = "Choose at most six room favorites."; return false;
        }
    }
    config.room_control_count = 0;
    memset(config.room_controls, 0, sizeof(config.room_controls));
    for (JsonObject item : doc.as<JsonArray>()) {
        auto &out = config.room_controls[config.room_control_count++];
        snprintf(out.entity_id, sizeof(out.entity_id), "%s", item["entity_id"].as<const char *>());
        snprintf(out.label, sizeof(out.label), "%s", item["label"].as<const char *>());
        out.placement = item["placement"].as<uint8_t>();
    }
    return true;
}
