#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "app_config.h"

struct PanelMediaShortcut {
    char label[PANEL_MEDIA_SHORTCUT_LABEL_LEN];
    char icon[20];
    char entity_id[PANEL_MEDIA_ENTITY_ID_LEN];
    char media_content_id[HA_MEDIA_CONTENT_ID_LEN];
    char media_content_type[HA_MEDIA_CONTENT_TYPE_LEN];
};

// Placement: 0 = grouped, 1 = favorite, 2 = hidden. Array order is display order.
struct PanelRoomControl {
    char entity_id[96];
    char label[64];
    uint8_t placement;
    uint8_t room_index;
    // "auto" uses the entity's Home Assistant domain; an explicit value lets
    // the owner render a compatible device as light, switch, fan, cover, or scene.
    char device_type[12];
};

// Four compact, configurable status positions are shown beside the room picker.
// type is entity, devices, or controls. Entity slots are presentation-only and
// never send a Home Assistant command.
struct PanelRoomStatusSlot {
    char type[12];
    char entity_id[96];
    char label[24];
    char icon[20];
    char active_states[PANEL_OVERVIEW_STATE_LEN];
    char active_label[24];
    char inactive_label[24];
    char color[12];
};

struct PanelRoom {
    char tab_label[PANEL_ROOM_NAME_LEN];
    char header[PANEL_ROOM_NAME_LEN];
    // Retained so pre-1.6.6 configurations can be migrated without losing
    // their sensor selections. New UI code uses status_slots exclusively.
    char temperature_entity_id[96];
    char humidity_entity_id[96];
    PanelRoomStatusSlot status_slots[PANEL_ROOM_STATUS_SLOTS];
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

// Overview 1.6.4 uses one ordered, four-column collection for built-in status
// summaries and Home Assistant entities. Entity items can be status-only or
// actionable without coupling their presentation to HA transport details.
struct PanelOverviewItem {
    char type[16];             // entity or a supported built-in summary
    char entity_id[96];        // status source shown by the card
    char action_entity_id[96]; // optional tap target; empty uses entity_id
    char label[PANEL_OVERVIEW_LABEL_LEN];
    char icon[20];             // auto, garage, door, lock, motion, light, ...
    char action[16];           // none, toggle, scene, or all_lights
    char active_states[PANEL_OVERVIEW_STATE_LEN]; // comma-separated HA states
    char active_label[24];
    char inactive_label[24];
    char color[12];            // cyan, green, yellow, red, purple, or blue
    uint8_t span;              // 1, 2, or 4 columns
    bool confirm;
};

struct PanelCalendarSource {
    char entity_id[96];
    char label[PANEL_CALENDAR_LABEL_LEN];
    char color[12];
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
    // Popup favorites are separate from the six main-page quick-play cards.
    // They are shown first in the Media > Favorites bubble.
    PanelMediaShortcut media_favorites[PANEL_MAX_MEDIA_FAVORITES];
    uint8_t media_favorite_count;
    PanelRoomControl room_controls[PANEL_MAX_ROOM_CONTROLS];
    uint8_t room_control_count;
    PanelRoom rooms[PANEL_MAX_ROOMS];
    uint8_t room_count;
    // Once a layout is saved in the 1.5 web manager, only these configured
    // entities (plus configured media players/shortcuts) are subscribed.
    bool explicit_layout;
    char media_players[PANEL_MAX_MEDIA_PLAYERS][PANEL_MEDIA_ENTITY_ID_LEN];
    uint8_t media_player_count;
    char weather_entity_id[96];
    // Weather presentation remains configuration-only. The Home Assistant
    // transport exposes a UI-neutral current/hourly/daily snapshot.
    char weather_layout[20]; // balanced, current_focus, or forecast_focus
    bool weather_show_current;
    bool weather_show_hourly;
    bool weather_show_daily;
    bool weather_header_enabled;
    // calendar_entity_id is retained as a migration alias and mirrors the
    // first selected source for legacy Overview cards.
    char calendar_entity_id[96];
    PanelCalendarSource calendars[PANEL_MAX_CALENDARS];
    uint8_t calendar_count;
    bool calendar_week_starts_monday;
    PanelOverviewWidget overview_widgets[PANEL_MAX_OVERVIEW_WIDGETS];
    uint8_t overview_widget_count;
    PanelOverviewQuickAction overview_quick_actions[PANEL_MAX_OVERVIEW_QUICK_ACTIONS];
    uint8_t overview_quick_action_count;
    PanelOverviewItem overview_items[PANEL_MAX_OVERVIEW_ITEMS];
    uint8_t overview_item_count;
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
bool config_service_parse_rooms(const String &json, PanelConfig &config, String &error);
void config_service_set_room_status_defaults(PanelRoom &room,
                                             const char *temperature_entity_id = "",
                                             const char *humidity_entity_id = "");
bool config_service_parse_overview_widgets(const String &json, PanelConfig &config, String &error);
bool config_service_parse_overview_quick_actions(const String &json, PanelConfig &config, String &error);
bool config_service_parse_overview_items(const String &json, PanelConfig &config, String &error);
void config_service_set_overview_defaults(PanelConfig &config);
void config_service_set_overview_quick_action_defaults(PanelConfig &config);
void config_service_set_overview_item_defaults(PanelConfig &config);
void config_service_migrate_overview_items(PanelConfig &config);
