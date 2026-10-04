#include <memory>
#include <new>
#include "config_service.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <string.h>

namespace {
PanelConfig g_config = {};
bool g_mounted = false;

void copy_text(char *dst, size_t len, const char *src) {
    if (!dst || len == 0) return;
    snprintf(dst, len, "%s", src ? src : "");
}

bool valid_module_id(const char *id) {
    if (!id || !id[0]) return false;
    for (const char *p = id; *p; ++p) {
        const bool ok = (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-';
        if (!ok) return false;
    }
    return true;
}

void add_module(PanelConfig &cfg, const char *id) {
    // v1.5.3 retires the separate All Rooms placeholder. Existing layouts
    // keep their room controls by migrating it to the real Room module.
    if (id && strcmp(id, "rooms") == 0) id = "room";
    if (!valid_module_id(id) || cfg.module_count >= PANEL_MAX_MODULES) return;
    for (uint8_t i = 0; i < cfg.module_count; ++i) if (strcmp(cfg.modules[i], id) == 0) return;
    copy_text(cfg.modules[cfg.module_count], PANEL_MODULE_ID_LEN, id);
    ++cfg.module_count;
}

void set_base_defaults(PanelConfig &cfg) {
    memset(&cfg, 0, sizeof(cfg));
    copy_text(cfg.device_id, sizeof(cfg.device_id), "panel-01");
    copy_text(cfg.display_name, sizeof(cfg.display_name), "Home Panel");
    copy_text(cfg.profile, sizeof(cfg.profile), "room");
    // Kept for migration compatibility only. Device bindings no longer use a
    // profile-wide Home Assistant area.
    cfg.area_id[0] = '\0';
    cfg.backlight = APP_DEFAULT_BACKLIGHT;
    cfg.dark_mode = true;
    cfg.screen_timeout_seconds = APP_DEFAULT_SCREEN_TIMEOUT_SECONDS;
    copy_text(cfg.weather_layout, sizeof(cfg.weather_layout), "balanced");
    cfg.weather_show_current = true;
    cfg.weather_show_hourly = true;
    cfg.weather_show_daily = true;
    cfg.weather_header_enabled = false;
    cfg.calendar_week_starts_monday = true;
    cfg.calendar_days = 7;
    cfg.climate_show_humidity = true;
    cfg.climate_show_fan = true;
    cfg.climate_show_presets = true;
    cfg.security_show_abnormal_summary = true;
    cfg.security_confirm_arming = true;
    cfg.security_code_to_arm = false;
    cfg.security_arm_home = true;
    cfg.security_arm_away = true;
    cfg.security_arm_night = true;
    cfg.security_arm_vacation = false;
    config_service_set_profile_defaults(cfg);
    cfg.room_count = 1;
    copy_text(cfg.rooms[0].tab_label, PANEL_ROOM_NAME_LEN, "Room");
    copy_text(cfg.rooms[0].header, PANEL_ROOM_NAME_LEN, "Your room");
    config_service_set_room_status_defaults(cfg.rooms[0]);
    config_service_set_overview_defaults(cfg);
    config_service_set_overview_quick_action_defaults(cfg);
    config_service_set_overview_item_defaults(cfg);
}

bool save_internal(const PanelConfig &cfg) {
    if (!g_mounted) return false;
    SPIFFS.remove(PANEL_CONFIG_TEMP_PATH);
    File f = SPIFFS.open(PANEL_CONFIG_TEMP_PATH, FILE_WRITE);
    if (!f) return false;
    JsonDocument doc;
    doc["schema"] = PANEL_CONFIG_SCHEMA;
    doc["device_id"] = cfg.device_id;
    doc["display_name"] = cfg.display_name;
    doc["profile"] = cfg.profile;
    doc["backlight"] = cfg.backlight;
    doc["dark_mode"] = cfg.dark_mode;
    doc["screen_timeout_seconds"] = cfg.screen_timeout_seconds;
    doc["explicit_layout"] = cfg.explicit_layout;
    doc["weather_entity_id"] = cfg.weather_entity_id;
    doc["weather_hourly_entity_id"] = cfg.weather_hourly_entity_id;
    doc["weather_daily_entity_id"] = cfg.weather_daily_entity_id;
    doc["weather_layout"] = cfg.weather_layout;
    doc["weather_show_current"] = cfg.weather_show_current;
    doc["weather_show_hourly"] = cfg.weather_show_hourly;
    doc["weather_show_daily"] = cfg.weather_show_daily;
    doc["weather_header_enabled"] = cfg.weather_header_enabled;
    doc["calendar_entity_id"] = cfg.calendar_entity_id;
    doc["calendar_week_starts_monday"] = cfg.calendar_week_starts_monday;
    doc["calendar_days"] = cfg.calendar_days;
    JsonArray calendars = doc["calendars"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.calendar_count; ++i) {
        JsonObject item = calendars.add<JsonObject>();
        item["entity_id"] = cfg.calendars[i].entity_id;
        item["label"] = cfg.calendars[i].label;
        item["color"] = cfg.calendars[i].color;
    }
    doc["climate_show_humidity"] = cfg.climate_show_humidity;
    doc["climate_show_fan"] = cfg.climate_show_fan;
    doc["climate_show_presets"] = cfg.climate_show_presets;
    JsonArray climate_devices = doc["climate_devices"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.climate_device_count; ++i) {
        JsonObject item = climate_devices.add<JsonObject>();
        item["entity_id"] = cfg.climate_devices[i].entity_id;
        item["label"] = cfg.climate_devices[i].label;
    }
    doc["alarm_entity_id"] = cfg.alarm_entity_id;
    doc["security_show_abnormal_summary"] = cfg.security_show_abnormal_summary;
    doc["security_confirm_arming"] = cfg.security_confirm_arming;
    doc["security_code_to_arm"] = cfg.security_code_to_arm;
    doc["security_arm_home"] = cfg.security_arm_home;
    doc["security_arm_away"] = cfg.security_arm_away;
    doc["security_arm_night"] = cfg.security_arm_night;
    doc["security_arm_vacation"] = cfg.security_arm_vacation;
    JsonArray security_devices = doc["security_devices"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.security_device_count; ++i) {
        const PanelSecurityDevice &source = cfg.security_devices[i];
        JsonObject item = security_devices.add<JsonObject>();
        item["entity_id"] = source.entity_id;
        item["label"] = source.label;
        item["icon"] = source.icon;
        item["abnormal_states"] = source.abnormal_states;
        item["normal_label"] = source.normal_label;
        item["abnormal_label"] = source.abnormal_label;
        item["color"] = source.color;
        item["reverse_abnormal"] = source.reverse_abnormal;
    }
    JsonArray security_dynamic_devices = doc["security_dynamic_devices"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.security_dynamic_device_count; ++i) {
        const PanelSecurityDevice &source = cfg.security_dynamic_devices[i];
        JsonObject item = security_dynamic_devices.add<JsonObject>();
        item["entity_id"] = source.entity_id;
        item["label"] = source.label;
        item["icon"] = source.icon;
        item["abnormal_states"] = source.abnormal_states;
        item["normal_label"] = source.normal_label;
        item["abnormal_label"] = source.abnormal_label;
        item["color"] = source.color;
        item["reverse_abnormal"] = source.reverse_abnormal;
    }
    JsonArray modules = doc["modules"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.module_count; ++i) modules.add(cfg.modules[i]);
    JsonArray shortcuts = doc["media_shortcuts"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.media_shortcut_count; ++i) {
        JsonObject item = shortcuts.add<JsonObject>();
        item["label"] = cfg.media_shortcuts[i].label;
        item["icon"] = cfg.media_shortcuts[i].icon;
        item["entity_id"] = cfg.media_shortcuts[i].entity_id;
        item["media_content_id"] = cfg.media_shortcuts[i].media_content_id;
        item["media_content_type"] = cfg.media_shortcuts[i].media_content_type;
    }
    JsonArray favorites = doc["media_favorites"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.media_favorite_count; ++i) {
        JsonObject item = favorites.add<JsonObject>();
        item["label"] = cfg.media_favorites[i].label;
        item["icon"] = cfg.media_favorites[i].icon;
        item["entity_id"] = cfg.media_favorites[i].entity_id;
        item["media_content_id"] = cfg.media_favorites[i].media_content_id;
        item["media_content_type"] = cfg.media_favorites[i].media_content_type;
    }
    JsonArray room = doc["room_controls"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.room_control_count; ++i) {
        JsonObject item = room.add<JsonObject>();
        item["entity_id"] = cfg.room_controls[i].entity_id;
        item["label"] = cfg.room_controls[i].label;
        item["placement"] = cfg.room_controls[i].placement;
        item["room_index"] = cfg.room_controls[i].room_index;
        item["device_type"] = cfg.room_controls[i].device_type;
    }
    JsonArray rooms = doc["rooms"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.room_count; ++i) {
        JsonObject item = rooms.add<JsonObject>();
        item["tab_label"] = cfg.rooms[i].tab_label;
        item["header"] = cfg.rooms[i].header;
        item["temperature_entity_id"] = cfg.rooms[i].temperature_entity_id;
        item["humidity_entity_id"] = cfg.rooms[i].humidity_entity_id;
        JsonArray status_slots = item["status_slots"].to<JsonArray>();
        for (uint8_t slot = 0; slot < PANEL_ROOM_STATUS_SLOTS; ++slot) {
            const PanelRoomStatusSlot &source = cfg.rooms[i].status_slots[slot];
            JsonObject status = status_slots.add<JsonObject>();
            status["type"] = source.type;
            status["entity_id"] = source.entity_id;
            status["label"] = source.label;
            status["icon"] = source.icon;
            status["active_states"] = source.active_states;
            status["active_label"] = source.active_label;
            status["inactive_label"] = source.inactive_label;
            status["color"] = source.color;
        }
    }
    JsonArray players = doc["media_players"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.media_player_count; ++i) players.add(cfg.media_players[i]);
    JsonArray widgets = doc["overview_widgets"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.overview_widget_count; ++i) {
        JsonObject item = widgets.add<JsonObject>();
        item["type"] = cfg.overview_widgets[i].type;
        item["span"] = cfg.overview_widgets[i].span;
        item["height"] = cfg.overview_widgets[i].height;
    }
    JsonArray quick_actions = doc["overview_quick_actions"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.overview_quick_action_count; ++i) {
        JsonObject item = quick_actions.add<JsonObject>();
        item["label"] = cfg.overview_quick_actions[i].label;
        item["type"] = cfg.overview_quick_actions[i].type;
        item["entity_id"] = cfg.overview_quick_actions[i].entity_id;
    }
    JsonArray overview_items = doc["overview_items"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.overview_item_count; ++i) {
        const PanelOverviewItem &source = cfg.overview_items[i];
        JsonObject item = overview_items.add<JsonObject>();
        item["type"] = source.type;
        item["entity_id"] = source.entity_id;
        item["action_entity_id"] = source.action_entity_id;
        item["label"] = source.label;
        item["icon"] = source.icon;
        item["action"] = source.action;
        item["active_states"] = source.active_states;
        item["active_label"] = source.active_label;
        item["inactive_label"] = source.inactive_label;
        item["color"] = source.color;
        item["span"] = source.span;
        item["confirm"] = source.confirm;
    }
    const size_t expected = measureJsonPretty(doc);
    const size_t written = serializeJsonPretty(doc, f);
    f.close();
    if (!written || written != expected) {
        SPIFFS.remove(PANEL_CONFIG_TEMP_PATH);
        return false;
    }

    // Keep the last complete file until the new one has been written and
    // renamed. A power loss can therefore leave either panel.json or panel.bak,
    // never only a partially serialized configuration.
    const bool had_current = SPIFFS.exists(PANEL_CONFIG_PATH);
    SPIFFS.remove(PANEL_CONFIG_ROLLBACK_PATH);
    if (had_current &&
        !SPIFFS.rename(PANEL_CONFIG_PATH, PANEL_CONFIG_ROLLBACK_PATH)) {
        SPIFFS.remove(PANEL_CONFIG_TEMP_PATH);
        return false;
    }
    if (!SPIFFS.rename(PANEL_CONFIG_TEMP_PATH, PANEL_CONFIG_PATH)) {
        if (had_current) SPIFFS.rename(PANEL_CONFIG_ROLLBACK_PATH, PANEL_CONFIG_PATH);
        SPIFFS.remove(PANEL_CONFIG_TEMP_PATH);
        return false;
    }
    SPIFFS.remove(PANEL_CONFIG_ROLLBACK_PATH);
    return true;
}
}

void config_service_set_profile_defaults(PanelConfig &cfg) {
    cfg.module_count = 0;
    memset(cfg.modules, 0, sizeof(cfg.modules));
    if (strcmp(cfg.profile, "calendar") == 0) {
        add_module(cfg, "overview"); add_module(cfg, "calendar"); add_module(cfg, "weather"); add_module(cfg, "security"); add_module(cfg, "settings");
    } else if (strcmp(cfg.profile, "whole_home") == 0) {
        add_module(cfg, "overview"); add_module(cfg, "room"); add_module(cfg, "media"); add_module(cfg, "climate"); add_module(cfg, "security"); add_module(cfg, "settings");
    } else if (strcmp(cfg.profile, "custom") == 0) {
        add_module(cfg, "overview"); add_module(cfg, "settings");
    } else {
        copy_text(cfg.profile, sizeof(cfg.profile), "room");
        add_module(cfg, "overview"); add_module(cfg, "room"); add_module(cfg, "media"); add_module(cfg, "climate"); add_module(cfg, "security"); add_module(cfg, "settings");
    }
}

bool config_service_begin() {
    set_base_defaults(g_config);
    g_mounted = SPIFFS.begin(true);
    if (!g_mounted) { Serial0.println("[Config] SPIFFS mount failed"); return false; }
    if (!SPIFFS.exists(PANEL_CONFIG_PATH) &&
        SPIFFS.exists(PANEL_CONFIG_ROLLBACK_PATH)) {
        Serial0.println("[Config] Recovering the last complete configuration");
        SPIFFS.rename(PANEL_CONFIG_ROLLBACK_PATH, PANEL_CONFIG_PATH);
    }
    SPIFFS.remove(PANEL_CONFIG_TEMP_PATH);
    if (!SPIFFS.exists(PANEL_CONFIG_PATH)) {
        Serial0.println("[Config] panel.json not found; creating defaults");
        return save_internal(g_config);
    }
    File f = SPIFFS.open(PANEL_CONFIG_PATH, FILE_READ);
    if (!f) return false;
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) { Serial0.printf("[Config] panel.json parse failed: %s\n", err.c_str()); return false; }

    std::unique_ptr<PanelConfig> loaded_storage(new (std::nothrow) PanelConfig{});
    if (!loaded_storage) { return false; }
    PanelConfig &loaded = *loaded_storage;
    copy_text(loaded.device_id, sizeof(loaded.device_id), doc["device_id"] | g_config.device_id);
    copy_text(loaded.display_name, sizeof(loaded.display_name), doc["display_name"] | g_config.display_name);
    copy_text(loaded.profile, sizeof(loaded.profile), doc["profile"] | g_config.profile);
    // Ignore the retired profile-wide area setting in existing panel.json
    // files. Entity selection is now explicit and whole-home.
    loaded.area_id[0] = '\0';
    loaded.backlight = constrain(static_cast<int>(doc["backlight"] | APP_DEFAULT_BACKLIGHT), 10, 100);
    loaded.dark_mode = doc["dark_mode"] | true;
    loaded.screen_timeout_seconds = doc["screen_timeout_seconds"] | APP_DEFAULT_SCREEN_TIMEOUT_SECONDS;
    // Schema 1 did not have a layout editor. Preserve its automatic discovery
    // until the owner saves a layout from the 1.5 web manager.
    loaded.explicit_layout = doc["explicit_layout"] | false;
    copy_text(loaded.weather_entity_id, sizeof(loaded.weather_entity_id), doc["weather_entity_id"] | "");
    copy_text(loaded.weather_hourly_entity_id, sizeof(loaded.weather_hourly_entity_id),
              doc["weather_hourly_entity_id"] | "");
    copy_text(loaded.weather_daily_entity_id, sizeof(loaded.weather_daily_entity_id),
              doc["weather_daily_entity_id"] | "");
    copy_text(loaded.weather_layout, sizeof(loaded.weather_layout), doc["weather_layout"] | "balanced");
    if (strcmp(loaded.weather_layout, "balanced") != 0 &&
        strcmp(loaded.weather_layout, "current_focus") != 0 &&
        strcmp(loaded.weather_layout, "forecast_focus") != 0) {
        copy_text(loaded.weather_layout, sizeof(loaded.weather_layout), "balanced");
    }
    loaded.weather_show_current = doc["weather_show_current"] | true;
    loaded.weather_show_hourly = doc["weather_show_hourly"] | true;
    loaded.weather_show_daily = doc["weather_show_daily"] | true;
    loaded.weather_header_enabled = doc["weather_header_enabled"] | false;
    if (!loaded.weather_show_current && !loaded.weather_show_hourly && !loaded.weather_show_daily)
        loaded.weather_show_current = true;
    copy_text(loaded.calendar_entity_id, sizeof(loaded.calendar_entity_id), doc["calendar_entity_id"] | "");
    loaded.calendar_week_starts_monday = doc["calendar_week_starts_monday"] | true;
    loaded.calendar_days = doc["calendar_days"] | 7;
    if (loaded.calendar_days != 1 && loaded.calendar_days != 3 && loaded.calendar_days != 7)
        loaded.calendar_days = 7;
    JsonArray calendar_sources = doc["calendars"].as<JsonArray>();
    if (!calendar_sources.isNull()) {
        for (JsonObject item : calendar_sources) {
            if (loaded.calendar_count >= PANEL_MAX_CALENDARS) break;
            const char *entity_id = item["entity_id"] | "";
            if (strncmp(entity_id, "calendar.", 9) != 0) continue;
            PanelCalendarSource &source = loaded.calendars[loaded.calendar_count++];
            copy_text(source.entity_id, sizeof(source.entity_id), entity_id);
            copy_text(source.label, sizeof(source.label), item["label"] | entity_id);
            copy_text(source.color, sizeof(source.color), item["color"] | "cyan");
        }
    }
    // Schema 4 exposed a single calendar entity. Promote it into the first
    // source without losing existing Overview configuration.
    if (loaded.calendar_count == 0 && loaded.calendar_entity_id[0]) {
        loaded.calendar_count = 1;
        copy_text(loaded.calendars[0].entity_id, sizeof(loaded.calendars[0].entity_id), loaded.calendar_entity_id);
        copy_text(loaded.calendars[0].label, sizeof(loaded.calendars[0].label), "Calendar");
        copy_text(loaded.calendars[0].color, sizeof(loaded.calendars[0].color), "cyan");
    }
    if (loaded.calendar_count) copy_text(loaded.calendar_entity_id, sizeof(loaded.calendar_entity_id), loaded.calendars[0].entity_id);
    loaded.climate_show_humidity = doc["climate_show_humidity"] | true;
    loaded.climate_show_fan = doc["climate_show_fan"] | true;
    loaded.climate_show_presets = doc["climate_show_presets"] | true;
    if (!doc["climate_devices"].isNull()) {
        String json, error;
        serializeJson(doc["climate_devices"], json);
        if (!config_service_parse_climate_devices(json, loaded, error))
            Serial0.printf("[Config] Invalid Climate devices: %s\n", error.c_str());
    }
    const char *alarm_entity = doc["alarm_entity_id"] | "";
    if (strncmp(alarm_entity, "alarm_control_panel.", 20) == 0)
        copy_text(loaded.alarm_entity_id, sizeof(loaded.alarm_entity_id), alarm_entity);
    loaded.security_show_abnormal_summary = doc["security_show_abnormal_summary"] | true;
    loaded.security_confirm_arming = doc["security_confirm_arming"] | true;
    loaded.security_code_to_arm = doc["security_code_to_arm"] | false;
    loaded.security_arm_home = doc["security_arm_home"] | true;
    loaded.security_arm_away = doc["security_arm_away"] | true;
    loaded.security_arm_night = doc["security_arm_night"] | true;
    loaded.security_arm_vacation = doc["security_arm_vacation"] | false;
    if (!loaded.security_arm_home && !loaded.security_arm_away &&
        !loaded.security_arm_night && !loaded.security_arm_vacation)
        loaded.security_arm_away = true;
    if (!doc["security_devices"].isNull()) {
        String json, error;
        serializeJson(doc["security_devices"], json);
        if (!config_service_parse_security_devices(json, loaded, error))
            Serial0.printf("[Config] Invalid security devices: %s\n", error.c_str());
    }
    if (!doc["security_dynamic_devices"].isNull()) {
        String json, error;
        serializeJson(doc["security_dynamic_devices"], json);
        if (!config_service_parse_security_dynamic_devices(json, loaded, error))
            Serial0.printf("[Config] Invalid dynamic security devices: %s\n", error.c_str());
    }
    {
        String security_error;
        if (!config_service_validate_security_device_uniqueness(loaded, security_error)) {
            Serial0.printf("[Config] Invalid Security device overlap: %s\n", security_error.c_str());
            loaded.security_dynamic_device_count = 0;
            memset(loaded.security_dynamic_devices, 0, sizeof(loaded.security_dynamic_devices));
        }
    }
    if (loaded.screen_timeout_seconds > 3600U) loaded.screen_timeout_seconds = APP_DEFAULT_SCREEN_TIMEOUT_SECONDS;

    JsonArray modules = doc["modules"].as<JsonArray>();
    if (!modules.isNull()) for (JsonVariant item : modules) add_module(loaded, item.as<const char *>());
    if (loaded.module_count == 0) config_service_set_profile_defaults(loaded);

    JsonArray shortcuts = doc["media_shortcuts"].as<JsonArray>();
    if (!shortcuts.isNull()) {
        for (JsonObject item : shortcuts) {
            if (loaded.media_shortcut_count >= PANEL_MAX_MEDIA_SHORTCUTS) break;
            const char *label = item["label"] | "";
            const char *icon = item["icon"] | "music";
            const char *entity_id = item["entity_id"] | "";
            const char *content_id = item["media_content_id"] | "";
            const char *content_type = item["media_content_type"] | "";
            if (!label[0] || !entity_id[0] || !content_id[0] || !content_type[0]) continue;
            PanelMediaShortcut &shortcut =
                loaded.media_shortcuts[loaded.media_shortcut_count++];
            copy_text(shortcut.label, sizeof(shortcut.label), label);
            copy_text(shortcut.icon, sizeof(shortcut.icon), icon);
            copy_text(shortcut.entity_id, sizeof(shortcut.entity_id), entity_id);
            copy_text(shortcut.media_content_id, sizeof(shortcut.media_content_id), content_id);
            copy_text(shortcut.media_content_type, sizeof(shortcut.media_content_type), content_type);
        }
    }
    JsonArray favorites = doc["media_favorites"].as<JsonArray>();
    if (!favorites.isNull()) {
        for (JsonObject item : favorites) {
            if (loaded.media_favorite_count >= PANEL_MAX_MEDIA_FAVORITES) break;
            const char *label = item["label"] | "";
            const char *icon = item["icon"] | "star";
            const char *entity_id = item["entity_id"] | "";
            const char *content_id = item["media_content_id"] | "";
            const char *content_type = item["media_content_type"] | "";
            if (!label[0] || !entity_id[0] || !content_id[0] || !content_type[0]) continue;
            PanelMediaShortcut &favorite =
                loaded.media_favorites[loaded.media_favorite_count++];
            copy_text(favorite.label, sizeof(favorite.label), label);
            copy_text(favorite.icon, sizeof(favorite.icon), icon);
            copy_text(favorite.entity_id, sizeof(favorite.entity_id), entity_id);
            copy_text(favorite.media_content_id, sizeof(favorite.media_content_id), content_id);
            copy_text(favorite.media_content_type, sizeof(favorite.media_content_type), content_type);
        }
    }
    if (!doc["room_controls"].isNull()) {
        String json, error;
        serializeJson(doc["room_controls"], json);
        if (!config_service_parse_room_controls(json, loaded, error)) {
            Serial0.printf("[Config] Invalid room preferences: %s\n", error.c_str());
        }
    }
    if (!doc["rooms"].isNull()) { String json, error; serializeJson(doc["rooms"], json); if (!config_service_parse_rooms(json, loaded, error)) Serial0.printf("[Config] Invalid rooms: %s\n", error.c_str()); }
    if (loaded.room_count == 0) { loaded.room_count = 1; copy_text(loaded.rooms[0].tab_label, PANEL_ROOM_NAME_LEN, "Room"); copy_text(loaded.rooms[0].header, PANEL_ROOM_NAME_LEN, "Your room"); config_service_set_room_status_defaults(loaded.rooms[0]); }
    if (!doc["overview_widgets"].isNull()) {
        String json, error;
        serializeJson(doc["overview_widgets"], json);
        if (!config_service_parse_overview_widgets(json, loaded, error))
            Serial0.printf("[Config] Invalid overview widgets: %s\n", error.c_str());
    }
    if (loaded.overview_widget_count == 0) config_service_set_overview_defaults(loaded);
    if (!doc["overview_quick_actions"].isNull()) {
        String json, error;
        serializeJson(doc["overview_quick_actions"], json);
        if (!config_service_parse_overview_quick_actions(json, loaded, error))
            Serial0.printf("[Config] Invalid overview quick actions: %s\n", error.c_str());
    } else config_service_set_overview_quick_action_defaults(loaded);
    if (!doc["overview_items"].isNull()) {
        String json, error;
        serializeJson(doc["overview_items"], json);
        if (!config_service_parse_overview_items(json, loaded, error)) {
            Serial0.printf("[Config] Invalid Overview cards: %s\n", error.c_str());
            config_service_migrate_overview_items(loaded);
        }
    } else {
        config_service_migrate_overview_items(loaded);
    }
    JsonArray players = doc["media_players"].as<JsonArray>();
    if (!players.isNull()) for (JsonVariant item : players) {
        const char *id = item.as<const char *>();
        if (!id || strncmp(id, "media_player.", 13) != 0 ||
            loaded.media_player_count >= PANEL_MAX_MEDIA_PLAYERS) continue;
        copy_text(loaded.media_players[loaded.media_player_count++],
                  PANEL_MEDIA_ENTITY_ID_LEN, id);
    }
    g_config = loaded;
    SPIFFS.remove(PANEL_CONFIG_ROLLBACK_PATH);
    Serial0.printf("[Config] %s profile=%s modules=%u media_shortcuts=%u media_favorites=%u\n",
                   g_config.device_id, g_config.profile,
                   static_cast<unsigned>(g_config.module_count),
                   static_cast<unsigned>(g_config.media_shortcut_count),
                   static_cast<unsigned>(g_config.media_favorite_count));
    return true;
}

const PanelConfig &config_service_get() { return g_config; }

bool config_service_export_json(String &output) {
    output = "";
    if (!g_mounted || !SPIFFS.exists(PANEL_CONFIG_PATH)) return false;
    File file = SPIFFS.open(PANEL_CONFIG_PATH, FILE_READ);
    if (!file || file.size() == 0 || file.size() > WEB_CONFIG_BACKUP_MAX_BYTES) {
        if (file) file.close();
        return false;
    }
    output.reserve(file.size() + 1);
    output = file.readString();
    const size_t expected = file.size();
    file.close();
    return output.length() == expected;
}

bool config_service_restore_json(const String &json, String &error) {
    error = "";
    if (json.isEmpty() || json.length() > WEB_CONFIG_BACKUP_MAX_BYTES) {
        error = "Configuration backup is empty or exceeds 64 KB.";
        return false;
    }
    JsonDocument doc;
    const DeserializationError parse_error = deserializeJson(doc, json);
    if (parse_error || !doc.is<JsonObject>()) {
        error = String("Configuration backup is not valid JSON: ") + parse_error.c_str();
        return false;
    }
    if (!doc["schema"].is<int>() || doc["schema"].as<int>() != PANEL_CONFIG_SCHEMA) {
        error = "Configuration backup uses an unsupported schema.";
        return false;
    }

    std::unique_ptr<PanelConfig> restored_storage(new (std::nothrow) PanelConfig{});
    if (!restored_storage) {
        error = "Insufficient memory to validate the configuration backup.";
        return false;
    }
    PanelConfig &restored = *restored_storage;
    set_base_defaults(restored);

    const char *device_id = doc["device_id"] | "";
    const char *display_name = doc["display_name"] | "";
    const char *profile = doc["profile"] | "";
    if (!device_id[0] || strlen(device_id) >= sizeof(restored.device_id) ||
        !display_name[0] || strlen(display_name) >= sizeof(restored.display_name)) {
        error = "Backup device ID or display name is missing or too long.";
        return false;
    }
    if (strcmp(profile, "calendar") != 0 && strcmp(profile, "room") != 0 &&
        strcmp(profile, "whole_home") != 0 && strcmp(profile, "custom") != 0) {
        error = "Backup contains an invalid panel profile.";
        return false;
    }
    copy_text(restored.device_id, sizeof(restored.device_id), device_id);
    copy_text(restored.display_name, sizeof(restored.display_name), display_name);
    copy_text(restored.profile, sizeof(restored.profile), profile);
    restored.area_id[0] = '\0';
    restored.backlight = static_cast<uint8_t>(constrain(doc["backlight"] | APP_DEFAULT_BACKLIGHT, 10, 100));
    restored.dark_mode = doc["dark_mode"] | true;
    restored.screen_timeout_seconds = doc["screen_timeout_seconds"] | APP_DEFAULT_SCREEN_TIMEOUT_SECONDS;
    if (restored.screen_timeout_seconds > 3600U) {
        error = "Backup screen timeout must be between 0 and 3600 seconds.";
        return false;
    }
    restored.explicit_layout = doc["explicit_layout"] | false;

    JsonArray modules = doc["modules"].as<JsonArray>();
    if (modules.isNull() || modules.size() == 0 || modules.size() > PANEL_MAX_MODULES) {
        error = "Backup must contain 1 to 8 panel modules.";
        return false;
    }
    restored.module_count = 0;
    memset(restored.modules, 0, sizeof(restored.modules));
    for (JsonVariant item : modules) {
        if (!item.is<const char *>() || !valid_module_id(item.as<const char *>())) {
            error = "Backup contains an invalid panel module.";
            return false;
        }
        const uint8_t before = restored.module_count;
        add_module(restored, item.as<const char *>());
        if (restored.module_count != before + 1) {
            error = "Backup contains a duplicate panel module.";
            return false;
        }
    }

    auto restore_entity = [&](const char *key, const char *prefix,
                              char *target, size_t target_size) -> bool {
        const char *value = doc[key] | "";
        if (strlen(value) >= target_size ||
            (value[0] && strncmp(value, prefix, strlen(prefix)) != 0)) {
            error = String("Backup contains an invalid ") + key + ".";
            return false;
        }
        copy_text(target, target_size, value);
        return true;
    };
    if (!restore_entity("weather_entity_id", "weather.", restored.weather_entity_id,
                        sizeof(restored.weather_entity_id)) ||
        !restore_entity("weather_hourly_entity_id", "weather.", restored.weather_hourly_entity_id,
                        sizeof(restored.weather_hourly_entity_id)) ||
        !restore_entity("weather_daily_entity_id", "weather.", restored.weather_daily_entity_id,
                        sizeof(restored.weather_daily_entity_id))) return false;
    const char *weather_layout = doc["weather_layout"] | "balanced";
    if (strcmp(weather_layout, "balanced") != 0 &&
        strcmp(weather_layout, "current_focus") != 0 &&
        strcmp(weather_layout, "forecast_focus") != 0) {
        error = "Backup contains an invalid Weather layout.";
        return false;
    }
    copy_text(restored.weather_layout, sizeof(restored.weather_layout), weather_layout);
    restored.weather_show_current = doc["weather_show_current"] | true;
    restored.weather_show_hourly = doc["weather_show_hourly"] | true;
    restored.weather_show_daily = doc["weather_show_daily"] | true;
    restored.weather_header_enabled = doc["weather_header_enabled"] | false;
    if (!restored.weather_show_current && !restored.weather_show_hourly &&
        !restored.weather_show_daily) {
        error = "Backup must enable at least one Weather section.";
        return false;
    }

    JsonArray calendars = doc["calendars"].as<JsonArray>();
    if (calendars.isNull() || calendars.size() > PANEL_MAX_CALENDARS) {
        error = "Backup calendars must be an array of at most six items.";
        return false;
    }
    restored.calendar_count = 0;
    memset(restored.calendars, 0, sizeof(restored.calendars));
    for (JsonObject item : calendars) {
        const char *entity_id = item["entity_id"] | "";
        const char *label = item["label"] | "";
        const char *color = item["color"] | "cyan";
        if (strncmp(entity_id, "calendar.", 9) != 0 ||
            strlen(entity_id) >= sizeof(restored.calendars[0].entity_id) ||
            !label[0] || strlen(label) >= sizeof(restored.calendars[0].label) ||
            strlen(color) >= sizeof(restored.calendars[0].color)) {
            error = "Backup contains an invalid calendar source.";
            return false;
        }
        PanelCalendarSource &out = restored.calendars[restored.calendar_count++];
        copy_text(out.entity_id, sizeof(out.entity_id), entity_id);
        copy_text(out.label, sizeof(out.label), label);
        copy_text(out.color, sizeof(out.color), color);
    }
    restored.calendar_week_starts_monday = doc["calendar_week_starts_monday"] | true;
    restored.calendar_days = doc["calendar_days"] | 7;
    if (restored.calendar_days != 1 && restored.calendar_days != 3 && restored.calendar_days != 7) {
        error = "Backup Calendar view must contain 1, 3, or 7 days.";
        return false;
    }
    restored.calendar_entity_id[0] = '\0';
    if (restored.calendar_count)
        copy_text(restored.calendar_entity_id, sizeof(restored.calendar_entity_id),
                  restored.calendars[0].entity_id);

    String collection;
    auto parse_collection = [&](const char *key,
                                bool (*parser)(const String &, PanelConfig &, String &)) -> bool {
        if (!doc[key].is<JsonArray>()) {
            error = String("Backup ") + key + " must be an array.";
            return false;
        }
        collection = "";
        serializeJson(doc[key], collection);
        return parser(collection, restored, error);
    };
    if (!parse_collection("climate_devices", config_service_parse_climate_devices)) return false;
    restored.climate_show_humidity = doc["climate_show_humidity"] | true;
    restored.climate_show_fan = doc["climate_show_fan"] | true;
    restored.climate_show_presets = doc["climate_show_presets"] | true;

    const char *alarm_entity = doc["alarm_entity_id"] | "";
    if (alarm_entity[0] && (strncmp(alarm_entity, "alarm_control_panel.", 20) != 0 ||
                            strlen(alarm_entity) >= sizeof(restored.alarm_entity_id))) {
        error = "Backup contains an invalid Alarmo entity.";
        return false;
    }
    copy_text(restored.alarm_entity_id, sizeof(restored.alarm_entity_id), alarm_entity);
    restored.security_show_abnormal_summary = doc["security_show_abnormal_summary"] | true;
    restored.security_confirm_arming = doc["security_confirm_arming"] | true;
    restored.security_code_to_arm = doc["security_code_to_arm"] | false;
    restored.security_arm_home = doc["security_arm_home"] | true;
    restored.security_arm_away = doc["security_arm_away"] | true;
    restored.security_arm_night = doc["security_arm_night"] | true;
    restored.security_arm_vacation = doc["security_arm_vacation"] | false;
    if (!restored.security_arm_home && !restored.security_arm_away &&
        !restored.security_arm_night && !restored.security_arm_vacation) {
        error = "Backup must enable at least one Security arming mode.";
        return false;
    }
    if (!parse_collection("security_devices", config_service_parse_security_devices) ||
        !parse_collection("security_dynamic_devices", config_service_parse_security_dynamic_devices) ||
        !config_service_validate_security_device_uniqueness(restored, error)) return false;

    auto restore_media = [&](const char *key, PanelMediaShortcut *target,
                             uint8_t &count, size_t capacity) -> bool {
        JsonArray items = doc[key].as<JsonArray>();
        if (items.isNull() || items.size() > capacity) {
            error = String("Backup ") + key + " exceeds its supported capacity.";
            return false;
        }
        count = 0;
        memset(target, 0, sizeof(PanelMediaShortcut) * capacity);
        for (JsonObject item : items) {
            const char *label = item["label"] | "";
            const char *icon = item["icon"] | "music";
            const char *entity_id = item["entity_id"] | "";
            const char *content_id = item["media_content_id"] | "";
            const char *content_type = item["media_content_type"] | "";
            if (!label[0] || strlen(label) >= sizeof(target[0].label) ||
                !icon[0] || strlen(icon) >= sizeof(target[0].icon) ||
                strncmp(entity_id, "media_player.", 13) != 0 ||
                strlen(entity_id) >= sizeof(target[0].entity_id) ||
                !content_id[0] || strlen(content_id) >= sizeof(target[0].media_content_id) ||
                !content_type[0] || strlen(content_type) >= sizeof(target[0].media_content_type)) {
                error = String("Backup contains an invalid ") + key + " item.";
                return false;
            }
            PanelMediaShortcut &out = target[count++];
            copy_text(out.label, sizeof(out.label), label);
            copy_text(out.icon, sizeof(out.icon), icon);
            copy_text(out.entity_id, sizeof(out.entity_id), entity_id);
            copy_text(out.media_content_id, sizeof(out.media_content_id), content_id);
            copy_text(out.media_content_type, sizeof(out.media_content_type), content_type);
        }
        return true;
    };
    if (!restore_media("media_shortcuts", restored.media_shortcuts,
                       restored.media_shortcut_count, PANEL_MAX_MEDIA_SHORTCUTS) ||
        !restore_media("media_favorites", restored.media_favorites,
                       restored.media_favorite_count, PANEL_MAX_MEDIA_FAVORITES)) return false;

    if (!parse_collection("room_controls", config_service_parse_room_controls) ||
        !parse_collection("rooms", config_service_parse_rooms) ||
        !parse_collection("overview_widgets", config_service_parse_overview_widgets) ||
        !parse_collection("overview_quick_actions", config_service_parse_overview_quick_actions) ||
        !parse_collection("overview_items", config_service_parse_overview_items)) return false;
    for (uint8_t i = 0; i < restored.room_control_count; ++i) {
        if (restored.room_controls[i].room_index >= restored.room_count) {
            error = "Backup assigns a Room control to a missing panel room.";
            return false;
        }
    }

    JsonArray players = doc["media_players"].as<JsonArray>();
    if (players.isNull() || players.size() > PANEL_MAX_MEDIA_PLAYERS) {
        error = "Backup media players must be an array of at most six items.";
        return false;
    }
    restored.media_player_count = 0;
    memset(restored.media_players, 0, sizeof(restored.media_players));
    for (JsonVariant item : players) {
        const char *entity_id = item | "";
        if (strncmp(entity_id, "media_player.", 13) != 0 ||
            strlen(entity_id) >= PANEL_MEDIA_ENTITY_ID_LEN) {
            error = "Backup contains an invalid media player entity.";
            return false;
        }
        for (uint8_t i = 0; i < restored.media_player_count; ++i) {
            if (strcmp(restored.media_players[i], entity_id) == 0) {
                error = "Backup contains a duplicate media player.";
                return false;
            }
        }
        copy_text(restored.media_players[restored.media_player_count++],
                  PANEL_MEDIA_ENTITY_ID_LEN, entity_id);
    }

    if (!config_service_save(restored)) {
        error = "Validated backup could not be written to panel storage.";
        return false;
    }
    return true;
}

bool config_service_save(const PanelConfig &config) {
    std::unique_ptr<PanelConfig> clean_storage(new (std::nothrow) PanelConfig(config));
    if (!clean_storage) { return false; }
    PanelConfig &clean = *clean_storage;
    if (clean.room_control_count > HA_MAX_AREA_ENTITIES) return false;
    clean.backlight = constrain(static_cast<int>(clean.backlight), 10, 100);
    if ((clean.weather_entity_id[0] && strncmp(clean.weather_entity_id, "weather.", 8) != 0) ||
        (clean.weather_hourly_entity_id[0] &&
         strncmp(clean.weather_hourly_entity_id, "weather.", 8) != 0) ||
        (clean.weather_daily_entity_id[0] &&
         strncmp(clean.weather_daily_entity_id, "weather.", 8) != 0)) return false;
    if (strcmp(clean.weather_layout, "balanced") != 0 &&
        strcmp(clean.weather_layout, "current_focus") != 0 &&
        strcmp(clean.weather_layout, "forecast_focus") != 0)
        copy_text(clean.weather_layout, sizeof(clean.weather_layout), "balanced");
    if (!clean.weather_show_current && !clean.weather_show_hourly && !clean.weather_show_daily) {
        clean.weather_show_current = true;
        clean.weather_show_hourly = true;
        clean.weather_show_daily = true;
    }
    if (clean.media_shortcut_count > PANEL_MAX_MEDIA_SHORTCUTS) {
        clean.media_shortcut_count = PANEL_MAX_MEDIA_SHORTCUTS;
    }
    if (clean.media_favorite_count > PANEL_MAX_MEDIA_FAVORITES) {
        clean.media_favorite_count = PANEL_MAX_MEDIA_FAVORITES;
    }
    if (clean.media_player_count > PANEL_MAX_MEDIA_PLAYERS) {
        clean.media_player_count = PANEL_MAX_MEDIA_PLAYERS;
    }
    if (clean.calendar_count > PANEL_MAX_CALENDARS) clean.calendar_count = PANEL_MAX_CALENDARS;
    if (clean.climate_device_count > PANEL_MAX_CLIMATE_DEVICES)
        clean.climate_device_count = PANEL_MAX_CLIMATE_DEVICES;
    if (clean.calendar_days != 1 && clean.calendar_days != 3 && clean.calendar_days != 7)
        clean.calendar_days = 7;
    clean.calendar_entity_id[0] = '\0';
    if (clean.calendar_count)
        copy_text(clean.calendar_entity_id, sizeof(clean.calendar_entity_id), clean.calendars[0].entity_id);
    if (clean.security_device_count > PANEL_MAX_SECURITY_DEVICES)
        clean.security_device_count = PANEL_MAX_SECURITY_DEVICES;
    if (clean.security_dynamic_device_count > PANEL_MAX_SECURITY_DYNAMIC_DEVICES)
        clean.security_dynamic_device_count = PANEL_MAX_SECURITY_DYNAMIC_DEVICES;
    String security_error;
    if (!config_service_validate_security_device_uniqueness(clean, security_error)) return false;
    if (clean.alarm_entity_id[0] && strncmp(clean.alarm_entity_id, "alarm_control_panel.", 20) != 0)
        return false;
    if (!clean.security_arm_home && !clean.security_arm_away &&
        !clean.security_arm_night && !clean.security_arm_vacation)
        clean.security_arm_away = true;
    if (clean.overview_widget_count == 0 || clean.overview_widget_count > PANEL_MAX_OVERVIEW_WIDGETS)
        config_service_set_overview_defaults(clean);
    if (clean.overview_quick_action_count > PANEL_MAX_OVERVIEW_QUICK_ACTIONS)
        config_service_set_overview_quick_action_defaults(clean);
    if (clean.overview_item_count == 0 || clean.overview_item_count > PANEL_MAX_OVERVIEW_ITEMS)
        config_service_set_overview_item_defaults(clean);
    if (clean.module_count == 0) config_service_set_profile_defaults(clean);
    if (!save_internal(clean)) return false;
    g_config = clean;
    return true;
}

bool config_service_module_enabled(const char *module_id) {
    if (!module_id) return false;
    for (uint8_t i = 0; i < g_config.module_count; ++i) if (strcmp(g_config.modules[i], module_id) == 0) return true;
    return false;
}

bool config_service_parse_modules_csv(const String &csv, PanelConfig &config) {
    config.module_count = 0;
    memset(config.modules, 0, sizeof(config.modules));
    int start = 0;
    while (start < static_cast<int>(csv.length())) {
        int comma = csv.indexOf(',', start);
        if (comma < 0) comma = csv.length();
        String token = csv.substring(start, comma);
        token.trim(); token.toLowerCase();
        if (!token.isEmpty()) add_module(config, token.c_str());
        start = comma + 1;
    }
    return config.module_count > 0;
}

String config_service_modules_csv() {
    String result;
    for (uint8_t i = 0; i < g_config.module_count; ++i) { if (i) result += ","; result += g_config.modules[i]; }
    return result;
}
