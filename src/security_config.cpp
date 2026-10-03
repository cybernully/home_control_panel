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
}

bool config_service_parse_security_devices(const String &json, PanelConfig &config,
                                           String &error) {
    JsonDocument doc;
    if (deserializeJson(doc, json) || !doc.is<JsonArray>() ||
        doc.size() > PANEL_MAX_SECURITY_DEVICES) {
        error = "Security devices must be an array of at most eight items.";
        return false;
    }

    PanelSecurityDevice parsed[PANEL_MAX_SECURITY_DEVICES] = {};
    size_t count = 0;
    for (JsonObject item : doc.as<JsonArray>()) {
        const char *entity_id = item["entity_id"] | "";
        const char *label = item["label"] | "";
        const char *icon = item["icon"] | "auto";
        const char *abnormal_states = item["abnormal_states"] | "on,open,opening,unlocked,detected,problem,unsafe";
        const char *normal_label = item["normal_label"] | "Normal";
        const char *abnormal_label = item["abnormal_label"] | "Attention";
        const char *color = item["color"] | "red";
        if (!entity_id[0] || !strchr(entity_id, '.') ||
            strlen(entity_id) >= sizeof(parsed[0].entity_id) ||
            !label[0] || strlen(label) >= sizeof(parsed[0].label) ||
            !in_list(icon, VALID_ICONS, sizeof(VALID_ICONS) / sizeof(VALID_ICONS[0])) ||
            !abnormal_states[0] || strlen(abnormal_states) >= sizeof(parsed[0].abnormal_states) ||
            strlen(normal_label) >= sizeof(parsed[0].normal_label) ||
            strlen(abnormal_label) >= sizeof(parsed[0].abnormal_label) ||
            !in_list(color, VALID_COLORS, sizeof(VALID_COLORS) / sizeof(VALID_COLORS[0]))) {
            error = "Each security device needs a valid entity, label, icon, abnormal states, labels, and color.";
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            if (strcmp(parsed[i].entity_id, entity_id) == 0) {
                error = "Each security device may be added only once.";
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

    config.security_device_count = static_cast<uint8_t>(count);
    memset(config.security_devices, 0, sizeof(config.security_devices));
    memcpy(config.security_devices, parsed, sizeof(parsed));
    return true;
}
