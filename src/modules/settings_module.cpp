#include <memory>
#include <new>
#include "settings_module.h"

#include "app_config.h"
#include "board_lvgl.h"
#include "config_service.h"
#include "home_assistant.h"
#include "module_ui.h"
#include "network_service.h"

#include <Arduino.h>

using namespace module_ui;

void SettingsModule::create(lv_obj_t *parent) {
    box(parent, BG, 0, 0);
    module_ui::title(parent, "Settings", "Panel-local controls and Home Assistant status");

    const PanelConfig &cfg = config_service_get();

    lv_obj_t *backlight = card(parent, 24, 92, 590, 220);
    lv_obj_t *bh = label(backlight, "Display brightness", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(bh, 18, 18);

    char pct[16];
    snprintf(pct, sizeof(pct), "%u%%", static_cast<unsigned>(cfg.backlight));
    brightness_label_ = label(backlight, pct, &lv_font_montserrat_24, TEXT);
    lv_obj_align(brightness_label_, LV_ALIGN_TOP_RIGHT, -18, 14);

    lv_obj_t *slider = lv_slider_create(backlight);
    lv_obj_set_pos(slider, 24, 84);
    lv_obj_set_size(slider, 542, 28);
    lv_slider_set_range(slider, 10, 100);
    lv_slider_set_value(slider, cfg.backlight, LV_ANIM_OFF);
    style_slider(slider);
    lv_obj_add_event_cb(slider, backlight_changed_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(slider, backlight_released_cb, LV_EVENT_RELEASED, this);

    lv_obj_t *hint = label(
        backlight,
        "Changes are applied immediately and saved on release.",
        &lv_font_montserrat_12, MUTED);
    lv_obj_set_width(hint, 540);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(hint, 18, 144);

    lv_obj_t *identity = card(parent, 634, 92, 618, 220);
    lv_obj_t *ih = label(identity, "Panel identity", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(ih, 18, 18);

    char detail[320];
    snprintf(detail, sizeof(detail),
             "Name: %s\nProfile: %s\nArea: %s\nFirmware: %s\nIP address: Checking...",
             cfg.display_name, cfg.profile,
             cfg.area_id[0] ? cfg.area_id : "(none)", APP_VERSION);
    identity_label_ = label(identity, detail, &lv_font_montserrat_16, TEXT);
    lv_obj_set_pos(identity_label_, 18, 60);

    lv_obj_t *ha = card(parent, 24, 332, 1228, 154);
    lv_obj_t *hh = label(ha, "Home Assistant", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(hh, 18, 18);

    ha_label_ = label(ha, "Checking connection...", &lv_font_montserrat_16, MUTED);
    lv_obj_set_width(ha_label_, 820);
    lv_label_set_long_mode(ha_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(ha_label_, 18, 58);

    discovery_label_ = label(ha, "Discovery not started", &lv_font_montserrat_12, MUTED);
    lv_obj_set_width(discovery_label_, 900);
    lv_label_set_long_mode(discovery_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(discovery_label_, 18, 104);

    lv_obj_t *rediscover = button(ha, "Rediscover Area", 1000, 50, 206, 62, ACCENT);
    lv_obj_add_event_cb(rediscover, rediscover_cb, LV_EVENT_CLICKED, this);
}

void SettingsModule::backlight_changed_cb(lv_event_t *e) {
    auto *self = static_cast<SettingsModule *>(lv_event_get_user_data(e));
    if (!self || !self->brightness_label_) return;

    const int value = static_cast<int>(
        lv_slider_get_value(static_cast<lv_obj_t *>(lv_event_get_target(e))));
    board_set_backlight(static_cast<uint8_t>(value));

    char text[16];
    snprintf(text, sizeof(text), "%d%%", value);
    lv_label_set_text(self->brightness_label_, text);
}

void SettingsModule::backlight_released_cb(lv_event_t *e) {
    const int value = static_cast<int>(
        lv_slider_get_value(static_cast<lv_obj_t *>(lv_event_get_target(e))));
    std::unique_ptr<PanelConfig> cfg_storage(new (std::nothrow) PanelConfig(config_service_get()));
    if (!cfg_storage) { return; }
    PanelConfig &cfg = *cfg_storage;
    cfg.backlight = static_cast<uint8_t>(value);
    config_service_save(cfg);
}

void SettingsModule::rediscover_cb(lv_event_t *e) {
    auto *self = static_cast<SettingsModule *>(lv_event_get_user_data(e));
    if (!self || !self->discovery_label_) return;

    const bool queued = home_assistant_request_discovery();
    lv_label_set_text(self->discovery_label_,
                      queued ? "Area rediscovery requested."
                             : "Could not request discovery; check Home Assistant configuration.");
}

void SettingsModule::update() {
    if (identity_label_) {
        const PanelConfig &cfg = config_service_get();
        const String ip = network_service_ip();

        char detail[320];
        snprintf(detail, sizeof(detail),
                 "Name: %s\nProfile: %s\nArea: %s\nFirmware: %s\nIP address: %s",
                 cfg.display_name, cfg.profile,
                 cfg.area_id[0] ? cfg.area_id : "(none)", APP_VERSION,
                 ip.length() == 0 ? "Offline" : ip.c_str());
        lv_label_set_text(identity_label_, detail);
    }

    HomeAssistantStatus health = {};
    home_assistant_get_status(health);

    if (ha_label_) {
        char text[200];
        const String base = home_assistant_base_url();
        snprintf(text, sizeof(text), "Status: %s%s%s",
                 health.message[0] ? health.message : "not checked",
                 base.isEmpty() ? "" : " | URL: ",
                 base.isEmpty() ? "" : base.c_str());
        lv_label_set_text(ha_label_, text);
    }

    if (discovery_label_) {
        HomeAssistantDiscoveryStatus discovery = {};
        home_assistant_get_discovery_status(discovery);

        char text[240];
        if (discovery.area_found) {
            snprintf(text, sizeof(text),
                     "Area: %s | %u entities / %u devices | WS: %s | %s",
                     discovery.area_name[0] ? discovery.area_name : discovery.area_id,
                     static_cast<unsigned>(discovery.entity_count),
                     static_cast<unsigned>(discovery.device_count),
                     discovery.websocket_authenticated ? "authenticated" : "offline",
                     discovery.message[0] ? discovery.message : "");
        } else {
            snprintf(text, sizeof(text), "WS: %s | %s",
                     discovery.websocket_authenticated ? "authenticated" : "offline",
                     discovery.message[0] ? discovery.message : "waiting");
        }
        lv_label_set_text(discovery_label_, text);
    }
}
