#include "config_service.h"
#include <ArduinoJson.h>
#include <string.h>

namespace {
const char *VALID_WIDGETS[] = {"home_status", "lights", "area", "network",
                               "quick_actions", "weather", "calendar", "panel_tip"};
const char *VALID_ITEMS[] = {"entity", "home_status", "lights", "network",
                             "all_lights", "weather", "weather_current",
                             "weather_hourly", "weather_daily", "calendar", "panel_tip"};
const char *VALID_ICONS[] = {"auto", "garage", "door", "lock", "motion", "light",
                             "fan", "cover", "window", "camera", "shield",
                             "temperature", "humidity", "power", "alert", "weather", "timer"};
const char *VALID_COLORS[] = {"cyan", "green", "yellow", "red", "purple", "blue"};
bool valid_type(const char *type) {
    for (const char *candidate : VALID_WIDGETS) if (strcmp(candidate, type) == 0) return true;
    return false;
}
void add(PanelConfig &config, const char *type, uint8_t span, uint8_t height) {
    if (config.overview_widget_count >= PANEL_MAX_OVERVIEW_WIDGETS) return;
    auto &widget = config.overview_widgets[config.overview_widget_count++];
    snprintf(widget.type, sizeof(widget.type), "%s", type);
    widget.span = span;
    widget.height = height;
}
bool in_list(const char *value, const char *const *values, size_t count) {
    if (!value) return false;
    for (size_t i = 0; i < count; ++i) if (strcmp(value, values[i]) == 0) return true;
    return false;
}
void add_item(PanelConfig &config, const char *type, const char *label, uint8_t span,
              const char *entity_id = "", const char *icon = "auto",
              const char *action = "none", const char *active_states = "",
              const char *active_label = "", const char *inactive_label = "",
              const char *color = "cyan", bool confirm = false,
              const char *action_entity_id = "") {
    if (config.overview_item_count >= PANEL_MAX_OVERVIEW_ITEMS) return;
    auto &item = config.overview_items[config.overview_item_count++];
    snprintf(item.type, sizeof(item.type), "%s", type);
    snprintf(item.entity_id, sizeof(item.entity_id), "%s", entity_id);
    snprintf(item.action_entity_id, sizeof(item.action_entity_id), "%s", action_entity_id);
    snprintf(item.label, sizeof(item.label), "%s", label);
    snprintf(item.icon, sizeof(item.icon), "%s", icon);
    snprintf(item.action, sizeof(item.action), "%s", action);
    snprintf(item.active_states, sizeof(item.active_states), "%s", active_states);
    snprintf(item.active_label, sizeof(item.active_label), "%s", active_label);
    snprintf(item.inactive_label, sizeof(item.inactive_label), "%s", inactive_label);
    snprintf(item.color, sizeof(item.color), "%s", color);
    item.span = span;
    item.confirm = confirm;
}
}

void config_service_set_overview_defaults(PanelConfig &config) {
    config.overview_widget_count = 0;
    memset(config.overview_widgets, 0, sizeof(config.overview_widgets));
    add(config, "home_status", 2, 1); add(config, "lights", 1, 1); add(config, "network", 1, 1);
    add(config, "quick_actions", 4, 2); add(config, "weather", 2, 1); add(config, "calendar", 2, 1);
}

void config_service_set_overview_quick_action_defaults(PanelConfig &config) {
    config.overview_quick_action_count = 1;
    memset(config.overview_quick_actions, 0, sizeof(config.overview_quick_actions));
    auto &action = config.overview_quick_actions[0];
    snprintf(action.label, sizeof(action.label), "All Lights");
    snprintf(action.type, sizeof(action.type), "all_lights");
}

void config_service_set_overview_item_defaults(PanelConfig &config) {
    config.overview_item_count = 0;
    memset(config.overview_items, 0, sizeof(config.overview_items));
    add_item(config, "home_status", "Home", 2, "", "shield", "none", "", "All good", "Check system", "green");
    add_item(config, "lights", "Lights", 1, "", "light", "none", "", "On", "All off", "yellow");
    add_item(config, "network", "Network", 1, "", "power", "none", "", "Online", "Offline", "cyan");
    add_item(config, "all_lights", "All Lights", 2, "", "light", "all_lights", "", "Turn off", "Turn on", "yellow", true);
}

void config_service_migrate_overview_items(PanelConfig &config) {
    config.overview_item_count = 0;
    memset(config.overview_items, 0, sizeof(config.overview_items));
    for (uint8_t i = 0; i < config.overview_widget_count; ++i) {
        const PanelOverviewWidget &widget = config.overview_widgets[i];
        const uint8_t span = widget.span == 1 || widget.span == 2 || widget.span == 4 ? widget.span : 2;
        if (strcmp(widget.type, "home_status") == 0) add_item(config, "home_status", "Home", span, "", "shield", "none", "", "All good", "Check system", "green");
        else if (strcmp(widget.type, "lights") == 0) add_item(config, "lights", "Lights", span, "", "light", "none", "", "On", "All off", "yellow");
        else if (strcmp(widget.type, "network") == 0) add_item(config, "network", "Network", span, "", "power", "none", "", "Online", "Offline", "cyan");
        else if (strcmp(widget.type, "weather") == 0) add_item(config, "weather", "Weather", span, config.weather_entity_id, "weather");
        else if (strcmp(widget.type, "calendar") == 0) add_item(config, "calendar", "Calendar", span, config.calendar_entity_id, "auto");
        else if (strcmp(widget.type, "panel_tip") == 0) add_item(config, "panel_tip", "Panel tip", span, "", "auto");
        else if (strcmp(widget.type, "quick_actions") == 0) {
            for (uint8_t j = 0; j < config.overview_quick_action_count; ++j) {
                const PanelOverviewQuickAction &old = config.overview_quick_actions[j];
                if (strcmp(old.type, "all_lights") == 0) add_item(config, "all_lights", old.label, 2, "", "light", "all_lights", "", "Turn off", "Turn on", "yellow", true);
                else add_item(config, "entity", old.label, 2, old.entity_id, "auto", old.type,
                              strcmp(old.type, "scene") == 0 ? "" : "on,open,opening,unlocked",
                              strcmp(old.type, "scene") == 0 ? "Ready" : "On", "Off", "cyan", false);
            }
        }
    }
    if (config.overview_item_count == 0) config_service_set_overview_item_defaults(config);
}

bool config_service_parse_overview_widgets(const String &json, PanelConfig &config, String &error) {
    JsonDocument doc;
    if (deserializeJson(doc, json) || !doc.is<JsonArray>() || doc.size() == 0 ||
        doc.size() > PANEL_MAX_OVERVIEW_WIDGETS) {
        error = "Overview widgets must be an array of 1 to 8 items."; return false;
    }
    PanelOverviewWidget parsed[PANEL_MAX_OVERVIEW_WIDGETS] = {};
    size_t count = 0;
    unsigned grid_cells = 0;
    for (JsonVariant item : doc.as<JsonArray>()) {
        const char *type = item["type"] | "";
        const unsigned span = item["span"] | 0U;
        // 1.5.0 widget records did not include height; preserve their Quick
        // actions card as a two-row card during the schema-compatible load.
        const unsigned height = item["height"] | (strcmp(type, "quick_actions") == 0 ? 2U : 1U);
        if (!valid_type(type) || (span != 1 && span != 2 && span != 4) || height < 1 || height > 4) {
            error = "Each overview widget needs a supported type, span 1/2/4, and height 1-4."; return false;
        }
        if (strcmp(type, "quick_actions") == 0 && height < 2) {
            error = "Quick actions needs at least two rows for configured buttons."; return false;
        }
        if (strcmp(type, "quick_actions") == 0 && span != 4) {
            error = "Quick actions must use the full-width widget area."; return false;
        }
        grid_cells += span * height;
        if (grid_cells > 16) { error = "Overview widgets exceed the four-by-four display grid."; return false; }
        for (size_t i = 0; i < count; ++i) if (strcmp(parsed[i].type, type) == 0) {
            error = "Each overview widget type may be used once."; return false;
        }
        snprintf(parsed[count].type, sizeof(parsed[count].type), "%s", type);
        parsed[count++].span = static_cast<uint8_t>(span);
        parsed[count - 1].height = static_cast<uint8_t>(height);
    }
    config.overview_widget_count = static_cast<uint8_t>(count);
    memset(config.overview_widgets, 0, sizeof(config.overview_widgets));
    memcpy(config.overview_widgets, parsed, sizeof(parsed));
    return true;
}

bool config_service_parse_overview_quick_actions(const String &json, PanelConfig &config, String &error) {
    JsonDocument doc;
    if (deserializeJson(doc, json) || !doc.is<JsonArray>() || doc.size() > PANEL_MAX_OVERVIEW_QUICK_ACTIONS) {
        error = "Quick actions must be an array of at most six items."; return false;
    }
    PanelOverviewQuickAction parsed[PANEL_MAX_OVERVIEW_QUICK_ACTIONS] = {};
    size_t count = 0;
    for (JsonVariant item : doc.as<JsonArray>()) {
        const char *label = item["label"] | "";
        const char *type = item["type"] | "";
        const char *entity_id = item["entity_id"] | "";
        const bool all_lights = strcmp(type, "all_lights") == 0;
        const bool scene = strcmp(type, "scene") == 0;
        const bool toggle = strcmp(type, "toggle") == 0;
        if (!label[0] || strlen(label) >= PANEL_OVERVIEW_QUICK_ACTION_LABEL_LEN || (!all_lights && !scene && !toggle) ||
            (!all_lights && (!entity_id[0] || strlen(entity_id) >= sizeof(parsed[0].entity_id)))) {
            error = "Each quick action needs a label, supported action type, and target entity when required."; return false;
        }
        if (scene && strncmp(entity_id, "scene.", 6) != 0) { error = "Scene actions must target a scene entity."; return false; }
        if (toggle && !(strncmp(entity_id, "light.", 6) == 0 || strncmp(entity_id, "switch.", 7) == 0 ||
                         strncmp(entity_id, "fan.", 4) == 0 || strncmp(entity_id, "cover.", 6) == 0)) {
            error = "Toggle actions must target a light, switch, fan, or cover entity."; return false;
        }
        snprintf(parsed[count].label, sizeof(parsed[count].label), "%s", label);
        snprintf(parsed[count].type, sizeof(parsed[count].type), "%s", type);
        snprintf(parsed[count].entity_id, sizeof(parsed[count].entity_id), "%s", entity_id);
        ++count;
    }
    config.overview_quick_action_count = static_cast<uint8_t>(count);
    memset(config.overview_quick_actions, 0, sizeof(config.overview_quick_actions));
    memcpy(config.overview_quick_actions, parsed, sizeof(parsed));
    return true;
}

bool config_service_parse_overview_items(const String &json, PanelConfig &config, String &error) {
    JsonDocument doc;
    if (deserializeJson(doc, json) || !doc.is<JsonArray>() || doc.size() == 0 ||
        doc.size() > PANEL_MAX_OVERVIEW_ITEMS) {
        error = "Overview cards must be an array of 1 to 12 items."; return false;
    }
    PanelOverviewItem parsed[PANEL_MAX_OVERVIEW_ITEMS] = {};
    size_t count = 0;
    unsigned grid_cells = 0;
    for (JsonObject item : doc.as<JsonArray>()) {
        const char *type = item["type"] | "";
        const char *entity_id = item["entity_id"] | "";
        const char *action_entity_id = item["action_entity_id"] | "";
        const char *label = item["label"] | "";
        const char *icon = item["icon"] | "auto";
        const char *action = item["action"] | "none";
        const char *active_states = item["active_states"] | "";
        const char *active_label = item["active_label"] | "";
        const char *inactive_label = item["inactive_label"] | "";
        const char *color = item["color"] | "cyan";
        const unsigned span = item["span"] | 0U;
        const bool confirm = item["confirm"] | false;
        if (!in_list(type, VALID_ITEMS, sizeof(VALID_ITEMS) / sizeof(VALID_ITEMS[0])) ||
            !label[0] || strlen(label) >= sizeof(parsed[0].label) ||
            (span != 1 && span != 2 && span != 4) ||
            !in_list(icon, VALID_ICONS, sizeof(VALID_ICONS) / sizeof(VALID_ICONS[0])) ||
            !in_list(color, VALID_COLORS, sizeof(VALID_COLORS) / sizeof(VALID_COLORS[0])) ||
            strlen(active_states) >= sizeof(parsed[0].active_states) ||
            strlen(active_label) >= sizeof(parsed[0].active_label) ||
            strlen(inactive_label) >= sizeof(parsed[0].inactive_label)) {
            error = "Each Overview card needs a valid type, label, width, icon, color, and state text."; return false;
        }
        if (strcmp(action, "none") != 0 && strcmp(action, "toggle") != 0 &&
            strcmp(action, "scene") != 0 && strcmp(action, "all_lights") != 0) {
            error = "Overview card actions must be none, toggle, scene, or all_lights."; return false;
        }
        const bool entity = strcmp(type, "entity") == 0;
        if (entity && (!entity_id[0] || !strchr(entity_id, '.') || strlen(entity_id) >= sizeof(parsed[0].entity_id))) {
            error = "Entity cards need a valid Home Assistant entity ID."; return false;
        }
        if (!entity && entity_id[0] && strlen(entity_id) >= sizeof(parsed[0].entity_id)) {
            error = "Overview entity ID is too long."; return false;
        }
        if (action_entity_id[0] && (!strchr(action_entity_id, '.') ||
            strlen(action_entity_id) >= sizeof(parsed[0].action_entity_id))) {
            error = "Overview action target must be a valid Home Assistant entity ID."; return false;
        }
        const char *action_target = action_entity_id[0] ? action_entity_id : entity_id;
        if (strcmp(action, "scene") == 0 && strncmp(action_target, "scene.", 6) != 0) {
            error = "Scene cards must use a scene entity as the action target."; return false;
        }
        if (strcmp(action, "toggle") == 0 && !(strncmp(action_target, "light.", 6) == 0 ||
            strncmp(action_target, "switch.", 7) == 0 || strncmp(action_target, "fan.", 4) == 0 ||
            strncmp(action_target, "cover.", 6) == 0 || strncmp(action_target, "lock.", 5) == 0)) {
            error = "Toggle cards must use a light, switch, fan, cover, or lock as the action target."; return false;
        }
        grid_cells += span;
        if (grid_cells > 16) { error = "Overview cards exceed the four-by-four display grid."; return false; }
        for (size_t i = 0; i < count; ++i) {
            if (entity && strcmp(parsed[i].type, "entity") == 0 && strcmp(parsed[i].entity_id, entity_id) == 0) {
                error = "An entity can appear only once on Overview."; return false;
            }
            if (!entity && strcmp(parsed[i].type, type) == 0) {
                error = "Each built-in Overview card may be used once."; return false;
            }
        }
        PanelOverviewItem &out = parsed[count++];
        snprintf(out.type, sizeof(out.type), "%s", type);
        snprintf(out.entity_id, sizeof(out.entity_id), "%s", entity_id);
        snprintf(out.action_entity_id, sizeof(out.action_entity_id), "%s", action_entity_id);
        snprintf(out.label, sizeof(out.label), "%s", label);
        snprintf(out.icon, sizeof(out.icon), "%s", icon);
        snprintf(out.action, sizeof(out.action), "%s", action);
        snprintf(out.active_states, sizeof(out.active_states), "%s", active_states);
        snprintf(out.active_label, sizeof(out.active_label), "%s", active_label);
        snprintf(out.inactive_label, sizeof(out.inactive_label), "%s", inactive_label);
        snprintf(out.color, sizeof(out.color), "%s", color);
        out.span = static_cast<uint8_t>(span);
        out.confirm = confirm;
    }
    config.overview_item_count = static_cast<uint8_t>(count);
    memset(config.overview_items, 0, sizeof(config.overview_items));
    memcpy(config.overview_items, parsed, sizeof(parsed));
    return true;
}
