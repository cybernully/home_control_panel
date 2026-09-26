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
    copy_text(cfg.area_id, sizeof(cfg.area_id), "living_room");
    cfg.backlight = APP_DEFAULT_BACKLIGHT;
    cfg.dark_mode = true;
    cfg.screen_timeout_seconds = APP_DEFAULT_SCREEN_TIMEOUT_SECONDS;
    config_service_set_profile_defaults(cfg);
    config_service_set_overview_defaults(cfg);
    config_service_set_overview_quick_action_defaults(cfg);
}

bool save_internal(const PanelConfig &cfg) {
    if (!g_mounted) return false;
    File f = SPIFFS.open(PANEL_CONFIG_PATH, FILE_WRITE);
    if (!f) return false;
    JsonDocument doc;
    doc["schema"] = 2;
    doc["device_id"] = cfg.device_id;
    doc["display_name"] = cfg.display_name;
    doc["profile"] = cfg.profile;
    doc["area_id"] = cfg.area_id;
    doc["backlight"] = cfg.backlight;
    doc["dark_mode"] = cfg.dark_mode;
    doc["screen_timeout_seconds"] = cfg.screen_timeout_seconds;
    doc["explicit_layout"] = cfg.explicit_layout;
    JsonArray modules = doc["modules"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.module_count; ++i) modules.add(cfg.modules[i]);
    JsonArray shortcuts = doc["media_shortcuts"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.media_shortcut_count; ++i) {
        JsonObject item = shortcuts.add<JsonObject>();
        item["label"] = cfg.media_shortcuts[i].label;
        item["entity_id"] = cfg.media_shortcuts[i].entity_id;
        item["media_content_id"] = cfg.media_shortcuts[i].media_content_id;
        item["media_content_type"] = cfg.media_shortcuts[i].media_content_type;
    }
    JsonArray room = doc["room_controls"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.room_control_count; ++i) {
        JsonObject item = room.add<JsonObject>();
        item["entity_id"] = cfg.room_controls[i].entity_id;
        item["label"] = cfg.room_controls[i].label;
        item["placement"] = cfg.room_controls[i].placement;
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
    const size_t written = serializeJsonPretty(doc, f);
    f.close();
    return written > 0;
}
}

void config_service_set_profile_defaults(PanelConfig &cfg) {
    cfg.module_count = 0;
    memset(cfg.modules, 0, sizeof(cfg.modules));
    if (strcmp(cfg.profile, "calendar") == 0) {
        add_module(cfg, "overview"); add_module(cfg, "calendar"); add_module(cfg, "weather"); add_module(cfg, "security"); add_module(cfg, "settings");
    } else if (strcmp(cfg.profile, "whole_home") == 0) {
        add_module(cfg, "overview"); add_module(cfg, "rooms"); add_module(cfg, "media"); add_module(cfg, "climate"); add_module(cfg, "security"); add_module(cfg, "settings");
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
    copy_text(loaded.area_id, sizeof(loaded.area_id), doc["area_id"] | g_config.area_id);
    loaded.backlight = constrain(static_cast<int>(doc["backlight"] | APP_DEFAULT_BACKLIGHT), 10, 100);
    loaded.dark_mode = doc["dark_mode"] | true;
    loaded.screen_timeout_seconds = doc["screen_timeout_seconds"] | APP_DEFAULT_SCREEN_TIMEOUT_SECONDS;
    // Schema 1 did not have a layout editor. Preserve its automatic discovery
    // until the owner saves a layout from the 1.5 web manager.
    loaded.explicit_layout = doc["explicit_layout"] | false;
    if (loaded.screen_timeout_seconds > 3600U) loaded.screen_timeout_seconds = APP_DEFAULT_SCREEN_TIMEOUT_SECONDS;

    JsonArray modules = doc["modules"].as<JsonArray>();
    if (!modules.isNull()) for (JsonVariant item : modules) add_module(loaded, item.as<const char *>());
    if (loaded.module_count == 0) config_service_set_profile_defaults(loaded);

    JsonArray shortcuts = doc["media_shortcuts"].as<JsonArray>();
    if (!shortcuts.isNull()) {
        for (JsonObject item : shortcuts) {
            if (loaded.media_shortcut_count >= PANEL_MAX_MEDIA_SHORTCUTS) break;
            const char *label = item["label"] | "";
            const char *entity_id = item["entity_id"] | "";
            const char *content_id = item["media_content_id"] | "";
            const char *content_type = item["media_content_type"] | "";
            if (!label[0] || !entity_id[0] || !content_id[0] || !content_type[0]) continue;
            PanelMediaShortcut &shortcut =
                loaded.media_shortcuts[loaded.media_shortcut_count++];
            copy_text(shortcut.label, sizeof(shortcut.label), label);
            copy_text(shortcut.entity_id, sizeof(shortcut.entity_id), entity_id);
            copy_text(shortcut.media_content_id, sizeof(shortcut.media_content_id), content_id);
            copy_text(shortcut.media_content_type, sizeof(shortcut.media_content_type), content_type);
        }
    }
    if (!doc["room_controls"].isNull()) {
        String json, error;
        serializeJson(doc["room_controls"], json);
        if (!config_service_parse_room_controls(json, loaded, error)) {
            Serial0.printf("[Config] Invalid room preferences: %s\n", error.c_str());
        }
    }
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
    JsonArray players = doc["media_players"].as<JsonArray>();
    if (!players.isNull()) for (JsonVariant item : players) {
        const char *id = item.as<const char *>();
        if (!id || strncmp(id, "media_player.", 13) != 0 ||
            loaded.media_player_count >= PANEL_MAX_MEDIA_PLAYERS) continue;
        copy_text(loaded.media_players[loaded.media_player_count++],
                  PANEL_MEDIA_ENTITY_ID_LEN, id);
    }
    g_config = loaded;
    Serial0.printf("[Config] %s profile=%s area=%s modules=%u media_shortcuts=%u\n",
                   g_config.device_id, g_config.profile, g_config.area_id,
                   static_cast<unsigned>(g_config.module_count),
                   static_cast<unsigned>(g_config.media_shortcut_count));
    return true;
}

const PanelConfig &config_service_get() { return g_config; }

bool config_service_save(const PanelConfig &config) {
    std::unique_ptr<PanelConfig> clean_storage(new (std::nothrow) PanelConfig(config));
    if (!clean_storage) { return false; }
    PanelConfig &clean = *clean_storage;
    if (clean.room_control_count > HA_MAX_AREA_ENTITIES) return false;
    clean.backlight = constrain(static_cast<int>(clean.backlight), 10, 100);
    if (clean.media_shortcut_count > PANEL_MAX_MEDIA_SHORTCUTS) {
        clean.media_shortcut_count = PANEL_MAX_MEDIA_SHORTCUTS;
    }
    if (clean.media_player_count > PANEL_MAX_MEDIA_PLAYERS) {
        clean.media_player_count = PANEL_MAX_MEDIA_PLAYERS;
    }
    if (clean.overview_widget_count == 0 || clean.overview_widget_count > PANEL_MAX_OVERVIEW_WIDGETS)
        config_service_set_overview_defaults(clean);
    if (clean.overview_quick_action_count > PANEL_MAX_OVERVIEW_QUICK_ACTIONS)
        config_service_set_overview_quick_action_defaults(clean);
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
