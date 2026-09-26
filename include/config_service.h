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

struct PanelConfig {
    char device_id[32];
    char display_name[48];
    char profile[20];
    char area_id[64];
    char modules[PANEL_MAX_MODULES][PANEL_MODULE_ID_LEN];
    uint8_t module_count;
    PanelMediaShortcut media_shortcuts[PANEL_MAX_MEDIA_SHORTCUTS];
    uint8_t media_shortcut_count;
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
