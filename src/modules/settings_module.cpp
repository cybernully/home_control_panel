#include <memory>
#include <new>
#include "settings_module.h"

#include "app_config.h"
#include "board_lvgl.h"
#include "config_service.h"
#include "home_assistant.h"
#include "module_ui.h"
#include "network_service.h"
#include "runtime_stats.h"

#include <Arduino.h>
#include <SPIFFS.h>

using namespace module_ui;

namespace {
lv_obj_t *stat_card(lv_obj_t *parent, const char *name, int x, int y, int w) {
    lv_obj_t *item = card(parent, x, y, w, 78);
    lv_obj_t *heading = label(item, name, &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(heading, 14, 10);
    lv_obj_t *value = label(item, "Checking...", &lv_font_montserrat_16, TEXT);
    lv_obj_set_pos(value, 14, 34);
    lv_obj_set_width(value, w - 28);
    lv_label_set_long_mode(value, LV_LABEL_LONG_DOT);
    return value;
}

uint8_t used_percent(size_t used, size_t total) {
    return total ? static_cast<uint8_t>((used * 100U + total / 2U) / total) : 0;
}
}

void SettingsModule::create(lv_obj_t *parent) {
    box(parent, BG, 0, 0);
    module_ui::title(parent, "System", "Panel controls, capacity, and connection diagnostics");

    const PanelConfig &cfg = config_service_get();

    lv_obj_t *backlight = card(parent, 24, 92, 390, 166);
    lv_obj_t *bh = label(backlight, "Display brightness", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(bh, 18, 18);

    char pct[16];
    snprintf(pct, sizeof(pct), "%u%%", static_cast<unsigned>(cfg.backlight));
    brightness_label_ = label(backlight, pct, &lv_font_montserrat_24, TEXT);
    lv_obj_align(brightness_label_, LV_ALIGN_TOP_RIGHT, -18, 14);

    lv_obj_t *slider = lv_slider_create(backlight);
    lv_obj_set_pos(slider, 24, 84);
    lv_obj_set_size(slider, 342, 28);
    lv_slider_set_range(slider, 10, 100);
    lv_slider_set_value(slider, cfg.backlight, LV_ANIM_OFF);
    style_slider(slider);
    lv_obj_add_event_cb(slider, backlight_changed_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(slider, backlight_released_cb, LV_EVENT_RELEASED, this);

    lv_obj_t *hint = label(
        backlight,
        "Changes are applied immediately and saved on release.",
        &lv_font_montserrat_12, MUTED);
    lv_obj_set_width(hint, 340);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(hint, 18, 128);

    heap_label_ = stat_card(parent, "INTERNAL RAM", 434, 92, 196);
    psram_label_ = stat_card(parent, "PSRAM", 648, 92, 196);
    storage_label_ = stat_card(parent, "FLASH STORAGE", 862, 92, 190);
    cpu_label_ = stat_card(parent, "CPU", 1070, 92, 182);
    runtime_label_ = stat_card(parent, "UPTIME / NETWORK", 434, 180, 394);

    lv_obj_t *identity = card(parent, 846, 180, 406, 78);
    lv_obj_t *ih = label(identity, "PANEL IDENTITY", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(ih, 14, 10);
    identity_label_ = label(identity, "Checking...", &lv_font_montserrat_14, TEXT);
    lv_obj_set_width(identity_label_, 378);
    lv_label_set_long_mode(identity_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(identity_label_, 14, 36);

    lv_obj_t *ha = card(parent, 24, 278, 1228, 210);
    lv_obj_t *hh = label(ha, "Home Assistant", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(hh, 18, 18);

    ha_label_ = label(ha, "Checking connection...", &lv_font_montserrat_16, MUTED);
    lv_obj_set_width(ha_label_, 820);
    lv_label_set_long_mode(ha_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(ha_label_, 18, 58);

    discovery_label_ = label(ha, "Discovery not started", &lv_font_montserrat_12, MUTED);
    lv_obj_set_width(discovery_label_, 900);
    lv_label_set_long_mode(discovery_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(discovery_label_, 18, 98);

    lv_obj_t *rediscover = button(ha, "Rediscover Area", 1000, 132, 206, 54, ACCENT);
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

        char detail[420];
        snprintf(detail, sizeof(detail),
                 "%s | %s | %s | v%s | %s",
                 cfg.display_name, cfg.profile, cfg.area_id[0] ? cfg.area_id : "no area", APP_VERSION,
                 ip.length() == 0 ? "Offline" : ip.c_str());
        lv_label_set_text(identity_label_, detail);
    }

    const size_t heap_total = ESP.getHeapSize();
    const size_t heap_free = ESP.getFreeHeap();
    const size_t psram_total = ESP.getPsramSize();
    const size_t psram_free = ESP.getFreePsram();
    const size_t storage_total = SPIFFS.totalBytes();
    const size_t storage_used = SPIFFS.usedBytes();
    char metric[72];
    if (heap_label_) {
        snprintf(metric, sizeof(metric), "%u / %u KB (%u%% used)",
                 static_cast<unsigned>((heap_total - heap_free) / 1024U),
                 static_cast<unsigned>(heap_total / 1024U),
                 static_cast<unsigned>(used_percent(heap_total - heap_free, heap_total)));
        lv_label_set_text(heap_label_, metric);
    }
    if (psram_label_) {
        snprintf(metric, sizeof(metric), "%u / %u KB (%u%% used)",
                 static_cast<unsigned>((psram_total - psram_free) / 1024U),
                 static_cast<unsigned>(psram_total / 1024U),
                 static_cast<unsigned>(used_percent(psram_total - psram_free, psram_total)));
        lv_label_set_text(psram_label_, metric);
    }
    if (storage_label_) {
        snprintf(metric, sizeof(metric), "%u / %u KB (%u%% used)",
                 static_cast<unsigned>(storage_used / 1024U),
                 static_cast<unsigned>(storage_total / 1024U),
                 static_cast<unsigned>(used_percent(storage_used, storage_total)));
        lv_label_set_text(storage_label_, metric);
    }
    if (cpu_label_) {
        snprintf(metric, sizeof(metric), "%uMHz / app %u%%",
                 static_cast<unsigned>(ESP.getCpuFreqMHz()),
                 static_cast<unsigned>(runtime_stats_app_loop_percent()));
        lv_label_set_text(cpu_label_, metric);
    }
    if (runtime_label_) {
        const uint32_t seconds = millis() / 1000UL;
        const bool online = network_service_connected();
        if (online) {
            snprintf(metric, sizeof(metric), "%lud %02lu:%02lu | Wi-Fi online %d dBm",
                     static_cast<unsigned long>(seconds / 86400UL),
                     static_cast<unsigned long>((seconds / 3600UL) % 24UL),
                     static_cast<unsigned long>((seconds / 60UL) % 60UL), network_service_rssi());
        } else {
            snprintf(metric, sizeof(metric), "%lud %02lu:%02lu | Wi-Fi offline",
                     static_cast<unsigned long>(seconds / 86400UL),
                     static_cast<unsigned long>((seconds / 3600UL) % 24UL),
                     static_cast<unsigned long>((seconds / 60UL) % 60UL));
        }
        lv_label_set_text(runtime_label_, metric);
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
        const bool recent_action = discovery.last_action_ms &&
                                   millis() - discovery.last_action_ms < 12000;
        if (discovery.area_found) {
            snprintf(text, sizeof(text),
                     "Area: %s | %u entities / %u devices | WS: %s | %s%s%s",
                     discovery.area_name[0] ? discovery.area_name : discovery.area_id,
                     static_cast<unsigned>(discovery.entity_count),
                     static_cast<unsigned>(discovery.device_count),
                     discovery.websocket_authenticated ? "authenticated" : "offline",
                     discovery.message[0] ? discovery.message : "",
                     recent_action ? " | Action: " : "",
                     recent_action ? discovery.last_action : "");
        } else {
            snprintf(text, sizeof(text), "WS: %s | %s%s%s",
                     discovery.websocket_authenticated ? "authenticated" : "offline",
                     discovery.message[0] ? discovery.message : "waiting",
                     recent_action ? " | Action: " : "",
                     recent_action ? discovery.last_action : "");
        }
        lv_label_set_text(discovery_label_, text);
    }
}
