#include "config_service.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

namespace {
void copy_text(char *out, size_t out_len, const char *value) {
    if (out && out_len) snprintf(out, out_len, "%s", value ? value : "");
}

bool valid_entity_id(const char *id) {
    if (!id || !id[0] || strlen(id) >= 96) return false;
    const char *dot = strchr(id, '.');
    if (!dot || dot == id || !dot[1] || strchr(dot + 1, '.')) return false;
    for (const char *p = id; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '.'))
            return false;
    return true;
}

bool one_of(const char *value, const char *const *allowed, size_t count) {
    for (size_t i = 0; i < count; ++i) if (strcmp(value, allowed[i]) == 0) return true;
    return false;
}

void set_status_slot(PanelRoomStatusSlot &slot, const char *type, const char *entity_id,
                     const char *label, const char *icon, const char *active_states,
                     const char *active_label, const char *inactive_label, const char *color) {
    memset(&slot, 0, sizeof(slot));
    copy_text(slot.type, sizeof(slot.type), type);
    copy_text(slot.entity_id, sizeof(slot.entity_id), entity_id);
    copy_text(slot.label, sizeof(slot.label), label);
    copy_text(slot.icon, sizeof(slot.icon), icon);
    copy_text(slot.active_states, sizeof(slot.active_states), active_states);
    copy_text(slot.active_label, sizeof(slot.active_label), active_label);
    copy_text(slot.inactive_label, sizeof(slot.inactive_label), inactive_label);
    copy_text(slot.color, sizeof(slot.color), color);
}

bool parse_status_slot(JsonObjectConst item, PanelRoomStatusSlot &out, String &error, size_t slot_index) {
    static const char *const TYPES[] = {"entity", "devices", "controls"};
    static const char *const ICONS[] = {"auto", "garage", "door", "lock", "motion", "light", "fan",
                                        "cover", "window", "camera", "shield", "temperature", "humidity",
                                        "power", "alert", "weather", "timer", "devices"};
    static const char *const COLORS[] = {"cyan", "green", "yellow", "red", "purple", "blue"};
    const char *type = item["type"] | "";
    const char *entity_id = item["entity_id"] | "";
    const char *label = item["label"] | "";
    const char *icon = item["icon"] | "auto";
    const char *active_states = item["active_states"] | "";
    const char *active_label = item["active_label"] | "";
    const char *inactive_label = item["inactive_label"] | "";
    const char *color = item["color"] | "cyan";
    const unsigned shown_slot = static_cast<unsigned>(slot_index + 1);
    if (!item["type"].is<const char *>() || !item["label"].is<const char *>() ||
        (!item["entity_id"].isNull() && !item["entity_id"].is<const char *>()) ||
        (!item["icon"].isNull() && !item["icon"].is<const char *>()) ||
        (!item["active_states"].isNull() && !item["active_states"].is<const char *>()) ||
        (!item["active_label"].isNull() && !item["active_label"].is<const char *>()) ||
        (!item["inactive_label"].isNull() && !item["inactive_label"].is<const char *>()) ||
        (!item["color"].isNull() && !item["color"].is<const char *>())) {
        char message[96]; snprintf(message, sizeof(message), "Room status slot %u has invalid field types.", shown_slot);
        error = message;
        return false;
    }
    if (!one_of(type, TYPES, sizeof(TYPES) / sizeof(TYPES[0]))) {
        char message[96]; snprintf(message, sizeof(message), "Room status slot %u has an invalid source.", shown_slot);
        error = message; return false;
    }
    if (!label[0]) {
        char message[96]; snprintf(message, sizeof(message), "Room status slot %u caption is required.", shown_slot);
        error = message; return false;
    }
    if (strlen(label) >= sizeof(out.label)) {
        char message[112]; snprintf(message, sizeof(message), "Room status slot %u caption must be at most 23 UTF-8 bytes.", shown_slot);
        error = message; return false;
    }
    if (strlen(entity_id) >= sizeof(out.entity_id) || strlen(icon) >= sizeof(out.icon) ||
        strlen(active_states) >= sizeof(out.active_states) || strlen(active_label) >= sizeof(out.active_label) ||
        strlen(inactive_label) >= sizeof(out.inactive_label) || strlen(color) >= sizeof(out.color)) {
        char message[96]; snprintf(message, sizeof(message), "Room status slot %u contains text that is too long.", shown_slot);
        error = message; return false;
    }
    if (!one_of(icon, ICONS, sizeof(ICONS) / sizeof(ICONS[0]))) {
        char message[96]; snprintf(message, sizeof(message), "Room status slot %u has an invalid icon.", shown_slot);
        error = message; return false;
    }
    if (!one_of(color, COLORS, sizeof(COLORS) / sizeof(COLORS[0]))) {
        char message[96]; snprintf(message, sizeof(message), "Room status slot %u has an invalid highlight color.", shown_slot);
        error = message; return false;
    }
    if (strcmp(type, "entity") == 0 && entity_id[0] && !valid_entity_id(entity_id)) {
        char message[112]; snprintf(message, sizeof(message), "Room status slot %u must use a valid Home Assistant entity ID.", shown_slot);
        error = message;
        return false;
    }
    set_status_slot(out, type, strcmp(type, "entity") == 0 ? entity_id : "", label, icon,
                    active_states, active_label, inactive_label, color);
    return true;
}
}  // namespace

void config_service_set_room_status_defaults(PanelRoom &room,
                                             const char *temperature_entity_id,
                                             const char *humidity_entity_id) {
    set_status_slot(room.status_slots[0], "entity", temperature_entity_id, "Temperature", "temperature",
                    "", "", "", "red");
    set_status_slot(room.status_slots[1], "entity", humidity_entity_id, "Humidity", "humidity",
                    "", "", "", "blue");
    set_status_slot(room.status_slots[2], "devices", "", "Devices Online", "devices",
                    "", "", "", "cyan");
    set_status_slot(room.status_slots[3], "controls", "", "Room Controls", "shield",
                    "", "", "", "green");
}

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
        config_service_set_room_status_defaults(parsed[i], temperature, humidity);
        JsonArrayConst slots = doc[i]["status_slots"].as<JsonArrayConst>();
        if (!doc[i]["status_slots"].isNull()) {
            if (slots.isNull() || slots.size() != PANEL_ROOM_STATUS_SLOTS) {
                error = "Every room must define exactly four status slots."; return false;
            }
            for (size_t slot = 0; slot < PANEL_ROOM_STATUS_SLOTS; ++slot) {
                JsonObjectConst item = slots[slot].as<JsonObjectConst>();
                if (item.isNull()) {
                    char message[80]; snprintf(message, sizeof(message), "Room status slot %u must be an object.", static_cast<unsigned>(slot + 1));
                    error = message; return false;
                }
                if (!parse_status_slot(item, parsed[i].status_slots[slot], error, slot)) return false;
            }
        }
    }
    config.room_count = static_cast<uint8_t>(doc.size()); memset(config.rooms, 0, sizeof(config.rooms)); memcpy(config.rooms, parsed, sizeof(parsed)); return true;
}
