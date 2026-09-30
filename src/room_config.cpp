#include "config_service.h"
#include <ArduinoJson.h>
#include <string.h>

bool config_service_parse_room_controls(const String &json, PanelConfig &config, String &error) {
    JsonDocument doc;
    if (deserializeJson(doc, json) || !doc.is<JsonArray>() || doc.size() > PANEL_MAX_ROOM_CONTROLS) {
        error = "Room controls must be an array of at most 48 entries.";
        return false;
    }
    unsigned favorites = 0;
    for (size_t i = 0; i < doc.size(); ++i) {
        JsonVariant item = doc[i];
        const char *id = item["entity_id"] | "";
        const char *label = item["label"] | "";
        const char *device_type = item["device_type"] | "auto";
        if (!item["entity_id"].is<const char *>() || !id[0] || strlen(id) >= 96 ||
            !item["label"].is<const char *>() || strlen(label) >= 64 ||
            (!item["device_type"].isNull() && (!item["device_type"].is<const char *>() || strlen(device_type) >= 12)) ||
            !item["placement"].is<unsigned>() || item["placement"].as<unsigned>() > 2 ||
            (!item["room_index"].isNull() && (!item["room_index"].is<unsigned>() || item["room_index"].as<unsigned>() >= PANEL_MAX_ROOMS))) {
            error = "Invalid room entry: entity, label (max 63 UTF-8 bytes), and placement 0-2 required.";
            return false;
        }
        if (strcmp(device_type,"auto") && strcmp(device_type,"light") && strcmp(device_type,"switch") &&
            strcmp(device_type,"fan") && strcmp(device_type,"cover") && strcmp(device_type,"scene")) {
            error = "Device type must be auto, light, switch, fan, cover, or scene."; return false;
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
        out.room_index = item["room_index"] | 0;
        snprintf(out.device_type, sizeof(out.device_type), "%s", item["device_type"] | "auto");
    }
    return true;
}

bool config_service_parse_rooms(const String &json, PanelConfig &config, String &error) {
    JsonDocument doc;
    if (deserializeJson(doc, json) || !doc.is<JsonArray>() || doc.size() == 0 || doc.size() > PANEL_MAX_ROOMS) {
        error = "Rooms must be an array of 1 to 4 named rooms."; return false;
    }
    PanelRoom parsed[PANEL_MAX_ROOMS] = {};
    for (size_t i = 0; i < doc.size(); ++i) {
        const char *tab = doc[i]["tab_label"] | ""; const char *header = doc[i]["header"] | "";
        const char *temperature = doc[i]["temperature_entity_id"] | "";
        const char *humidity = doc[i]["humidity_entity_id"] | "";
        if (!tab[0] || !header[0] || strlen(tab) >= PANEL_ROOM_NAME_LEN || strlen(header) >= PANEL_ROOM_NAME_LEN) {
            error = "Every room needs a tab label and header of at most 31 characters."; return false;
        }
        if ((temperature[0] && (strncmp(temperature, "sensor.", 7) != 0 || strlen(temperature) >= 96)) ||
            (humidity[0] && (strncmp(humidity, "sensor.", 7) != 0 || strlen(humidity) >= 96))) {
            error = "Room temperature and humidity entities must be sensor entity IDs."; return false;
        }
        snprintf(parsed[i].tab_label, sizeof(parsed[i].tab_label), "%s", tab);
        snprintf(parsed[i].header, sizeof(parsed[i].header), "%s", header);
        snprintf(parsed[i].temperature_entity_id, sizeof(parsed[i].temperature_entity_id), "%s", temperature);
        snprintf(parsed[i].humidity_entity_id, sizeof(parsed[i].humidity_entity_id), "%s", humidity);
    }
    config.room_count = static_cast<uint8_t>(doc.size()); memset(config.rooms, 0, sizeof(config.rooms)); memcpy(config.rooms, parsed, sizeof(parsed)); return true;
}
