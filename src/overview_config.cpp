#include "config_service.h"
#include <ArduinoJson.h>
#include <string.h>

namespace {
const char *VALID_WIDGETS[] = {"home_status", "lights", "area", "network",
                               "quick_actions", "weather", "calendar", "panel_tip"};
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
