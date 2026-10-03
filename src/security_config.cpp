#include "config_service.h"

#include <ArduinoJson.h>
#include <string.h>

namespace {
const char *const VALID_ICONS[] = {"auto", "door", "window", "garage", "lock",
                                   "motion", "camera", "shield", "alert", "power"};
const char *const VALID_COLORS[] = {"red", "yellow", "purple", "cyan"};

bool in_list(const char *value, const char *const *values, size_t count) {
    if (!value) return false;
    for (size_t i = 0; i < count; ++i)
        if (strcmp(value, values[i]) == 0) return true;
    return false;
}

bool parse_collection(const String &json, PanelSecurityDevice *destination,
                      size_t capacity, uint8_t &destination_count,
                      const char *collection_name, String &error) {
    JsonDocument doc;
    if (deserializeJson(doc, json) || !doc.is<JsonArray>() || doc.size() > capacity) {
        char message[96];
        snprintf(message, sizeof(message), "%s must be an array of at most %u items.",
                 collection_name, static_cast<unsigned>(capacity));
        error = message;
        return false;
    }

    // The dynamic collection is intentionally larger than the persistent-card
    // collection. Keep the transactional scratch buffer sized for the larger
    // collection so validation never writes past the temporary array.
    PanelSecurityDevice parsed[PANEL_MAX_SECURITY_DYNAMIC_DEVICES] = {};
    size_t count = 0;
    for (JsonObject item : doc.as<JsonArray>()) {
        const unsigned position = static_cast<unsigned>(count + 1);
        const char *entity_id = item["entity_id"] | "";
        const char *label = item["label"] | "";
        const char *icon = item["icon"] | "auto";
        const char *abnormal_states = item["abnormal_states"] |
            "on,open,opening,unlocked,jammed,problem,unsafe";
        const char *normal_label = item["normal_label"] | "Normal";
        const char *abnormal_label = item["abnormal_label"] | "Attention";
        const char *color = item["color"] | "red";
        char message[144];
        if (!entity_id[0] || !strchr(entity_id, '.') ||
            strlen(entity_id) >= sizeof(parsed[0].entity_id)) {
            snprintf(message, sizeof(message), "%s item %u needs a valid Home Assistant entity ID.",
                     collection_name, position);
            error = message;
            return false;
        }
        if (!label[0] || strlen(label) >= sizeof(parsed[0].label)) {
            snprintf(message, sizeof(message), "%s item %u needs a display name of at most 47 characters.",
                     collection_name, position);
            error = message;
            return false;
        }
        if (!in_list(icon, VALID_ICONS, sizeof(VALID_ICONS) / sizeof(VALID_ICONS[0]))) {
            snprintf(message, sizeof(message), "%s item %u has an invalid icon.",
                     collection_name, position);
            error = message;
            return false;
        }
        if (!abnormal_states[0] || strlen(abnormal_states) >= sizeof(parsed[0].abnormal_states)) {
            snprintf(message, sizeof(message), "%s item %u needs abnormal states of at most 63 characters.",
                     collection_name, position);
            error = message;
            return false;
        }
        if (strlen(normal_label) >= sizeof(parsed[0].normal_label) ||
            strlen(abnormal_label) >= sizeof(parsed[0].abnormal_label)) {
            snprintf(message, sizeof(message), "%s item %u state labels must be at most 23 characters.",
                     collection_name, position);
            error = message;
            return false;
        }
        if (!in_list(color, VALID_COLORS, sizeof(VALID_COLORS) / sizeof(VALID_COLORS[0]))) {
            snprintf(message, sizeof(message), "%s item %u has an invalid attention color.",
                     collection_name, position);
            error = message;
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            if (strcmp(parsed[i].entity_id, entity_id) == 0) {
                snprintf(message, sizeof(message), "%s contains %s more than once.",
                         collection_name, entity_id);
                error = message;
                return false;
            }
        }
        PanelSecurityDevice &out = parsed[count++];
        snprintf(out.entity_id, sizeof(out.entity_id), "%s", entity_id);
        snprintf(out.label, sizeof(out.label), "%s", label);
        snprintf(out.icon, sizeof(out.icon), "%s", icon);
        snprintf(out.abnormal_states, sizeof(out.abnormal_states), "%s", abnormal_states);
        snprintf(out.normal_label, sizeof(out.normal_label), "%s", normal_label);
        snprintf(out.abnormal_label, sizeof(out.abnormal_label), "%s", abnormal_label);
        snprintf(out.color, sizeof(out.color), "%s", color);
        out.reverse_abnormal = item["reverse_abnormal"] | false;
    }

    destination_count = static_cast<uint8_t>(count);
    memset(destination, 0, sizeof(PanelSecurityDevice) * capacity);
    memcpy(destination, parsed, sizeof(PanelSecurityDevice) * capacity);
    return true;
}
}

bool config_service_parse_security_devices(const String &json, PanelConfig &config,
                                           String &error) {
    return parse_collection(json, config.security_devices, PANEL_MAX_SECURITY_DEVICES,
                            config.security_device_count, "Monitored devices", error);
}

bool config_service_parse_security_dynamic_devices(const String &json, PanelConfig &config,
                                                   String &error) {
    return parse_collection(json, config.security_dynamic_devices,
                            PANEL_MAX_SECURITY_DYNAMIC_DEVICES,
                            config.security_dynamic_device_count,
                            "Dynamic attention devices", error);
}

bool config_service_validate_security_device_uniqueness(const PanelConfig &config,
                                                        String &error) {
    for (uint8_t monitored = 0; monitored < config.security_device_count; ++monitored) {
        for (uint8_t dynamic = 0; dynamic < config.security_dynamic_device_count; ++dynamic) {
            if (strcmp(config.security_devices[monitored].entity_id,
                       config.security_dynamic_devices[dynamic].entity_id) == 0) {
                error = "A security entity cannot be both monitored and dynamic attention-only.";
                return false;
            }
        }
    }
    return true;
}
