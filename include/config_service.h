#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "app_config.h"

struct PanelMediaShortcut {
    char label[PANEL_MEDIA_SHORTCUT_LABEL_LEN];
    char entity_id[PANEL_MEDIA_ENTITY_ID_LEN];
    char media_content_id[HA_MEDIA_CONTENT_ID_LEN];
    char media_content_type[HA_MEDIA_CONTENT_TYPE_LEN];
};

// Placement: 0 = grouped, 1 = favorite, 2 = hidden. Array order is display order.
struct PanelRoomControl {
    char entity_id[96];
    char label[64];
    uint8_t placement;
};

// span is the number of columns in Overview's four-column widget grid: 1, 2,
// or 4. The widget type selects a built-in renderer and HA data source.
struct PanelOverviewWidget {
    char type[PANEL_OVERVIEW_WIDGET_ID_LEN];
    uint8_t span;
    uint8_t height;
};

// type: all_lights, toggle, or scene. Toggle/scene actions require entity_id.
struct PanelOverviewQuickAction {
    char label[PANEL_OVERVIEW_QUICK_ACTION_LABEL_LEN];
    char type[16];
    char entity_id[96];
};

struct PanelConfig {
    char device_id[32];
    char display_name[48];
    char profile[20];
    char area_id[64];
    char modules[PANEL_MAX_MODULES][PANEL_MODULE_ID_LEN];
    uint8_t module_count;
    PanelMediaShortcut media_shortcuts[PANEL_MAX_MEDIA_SHORTCUTS];
    uint8_t media_shortcut_count;
    PanelRoomControl room_controls[HA_MAX_AREA_ENTITIES];
    uint8_t room_control_count;
    // Once a layout is saved in the 1.5 web manager, only these configured
    // entities (plus configured media players/shortcuts) are subscribed.
    bool explicit_layout;
    char media_players[PANEL_MAX_MEDIA_PLAYERS][PANEL_MEDIA_ENTITY_ID_LEN];
    uint8_t media_player_count;
    PanelOverviewWidget overview_widgets[PANEL_MAX_OVERVIEW_WIDGETS];
    uint8_t overview_widget_count;
    PanelOverviewQuickAction overview_quick_actions[PANEL_MAX_OVERVIEW_QUICK_ACTIONS];
    uint8_t overview_quick_action_count;
    uint8_t backlight;
    bool dark_mode;
    uint32_t screen_timeout_seconds;
};

bool config_service_begin();
const PanelConfig &config_service_get();
bool config_service_save(const PanelConfig &config);
void config_service_set_profile_defaults(PanelConfig &config);
bool config_service_module_enabled(const char *module_id);
bool config_service_parse_modules_csv(const String &csv, PanelConfig &config);
String config_service_modules_csv();

// Shared strict parser for persisted and web room preferences.
bool config_service_parse_room_controls(const String &json, PanelConfig &config, String &error);
bool config_service_parse_overview_widgets(const String &json, PanelConfig &config, String &error);
bool config_service_parse_overview_quick_actions(const String &json, PanelConfig &config, String &error);
void config_service_set_overview_defaults(PanelConfig &config);
void config_service_set_overview_quick_action_defaults(PanelConfig &config);
