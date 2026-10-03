#include "weather_module.h"

#include "config_service.h"
#include "display_text.h"
#include "ha_icons_font.h"
#include "module_ui.h"
#include "ui_theme.h"

#include <stdio.h>
#include <string.h>

using namespace module_ui;

namespace {
void safe_text(lv_obj_t *target, const char *value) {
    if (!target) return;
    char safe[160];
    panel_display_text(safe, sizeof(safe), value ? value : "");
    lv_label_set_text(target, safe);
}

void show(lv_obj_t *object, bool visible) {
    if (!object) return;
    if (visible) lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *forecast_card(lv_obj_t *parent, WeatherModule::ForecastSlot &slot) {
    slot.root = lv_obj_create(parent);
    box(slot.root, CARD_ALT, 12, 1);
    slot.period = label(slot.root, "--", &lv_font_montserrat_14, MUTED);
    slot.icon = label(slot.root, "", &ha_icons_font, ui_theme::CYAN);
    ui_theme::set_glyph(slot.icon, 0xF0595);
    slot.temperature = label(slot.root, "--", &lv_font_montserrat_20, TEXT);
    slot.condition = label(slot.root, "Waiting", &lv_font_montserrat_12, MUTED);
    slot.detail = label(slot.root, "", &lv_font_montserrat_12, MUTED);
    return slot.root;
}
}

void WeatherModule::create(lv_obj_t *parent) {
    box(parent, BG, 0, 0);
    module_ui::title(parent, "Weather", "Current conditions and forecasts from Home Assistant");
    subtitle_ = label(parent, "Select a weather entity in Web Admin", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(subtitle_, 650, 54);
    lv_obj_set_width(subtitle_, 606);
    lv_obj_set_style_text_align(subtitle_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_long_mode(subtitle_, LV_LABEL_LONG_DOT);

    current_card_ = card(parent, 24, PAGE_CONTENT_TOP, 360, 522);
    lv_obj_t *current_heading = label(current_card_, "CURRENT", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(current_heading, 22, 18);
    current_icon_ = label(current_card_, "", &ha_icons_font, ui_theme::CYAN);
    ui_theme::set_glyph(current_icon_, 0xF0595);
    lv_obj_set_width(current_icon_, 100);
    lv_obj_set_style_text_align(current_icon_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(current_icon_, 24, 86);
    current_temperature_ = label(current_card_, "--", &lv_font_montserrat_28, TEXT);
    lv_obj_set_width(current_temperature_, 170);
    lv_obj_set_style_text_align(current_temperature_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(current_temperature_, 126, 82);
    current_condition_ = label(current_card_, "Waiting for Home Assistant", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(current_condition_, 22, 168);
    lv_obj_set_width(current_condition_, 316);
    lv_obj_set_style_text_align(current_condition_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(current_condition_, LV_LABEL_LONG_DOT);
    current_detail_ = label(current_card_, "", &lv_font_montserrat_16, MUTED);
    lv_obj_set_pos(current_detail_, 24, 225);
    lv_obj_set_width(current_detail_, 312);
    lv_obj_set_style_text_align(current_detail_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(current_detail_, LV_LABEL_LONG_WRAP);

    hourly_card_ = card(parent, 400, PAGE_CONTENT_TOP, 856, 250);
    lv_obj_t *hourly_heading = label(hourly_card_, "HOURLY FORECAST", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(hourly_heading, 20, 16);
    hourly_status_ = label(hourly_card_, "Loading forecast...", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(hourly_status_, 640, 18);
    lv_obj_set_width(hourly_status_, 190);
    lv_obj_set_style_text_align(hourly_status_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    for (auto &slot : hourly_) forecast_card(hourly_card_, slot);

    daily_card_ = card(parent, 400, 346, 856, 256);
    lv_obj_t *daily_heading = label(daily_card_, "MULTI-DAY FORECAST", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(daily_heading, 20, 16);
    daily_status_ = label(daily_card_, "Loading forecast...", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(daily_status_, 640, 18);
    lv_obj_set_width(daily_status_, 190);
    lv_obj_set_style_text_align(daily_status_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    for (auto &slot : daily_) forecast_card(daily_card_, slot);

    apply_layout();
    update();
}

void WeatherModule::layout_hourly(int width, int height) {
    const int gap = 8, left = 16, top = 50;
    const int item_width = (width - left * 2 - gap * (HA_MAX_WEATHER_HOURLY - 1)) / HA_MAX_WEATHER_HOURLY;
    const int item_height = height - top - 14;
    for (int i = 0; i < HA_MAX_WEATHER_HOURLY; ++i) {
        auto &slot = hourly_[i];
        lv_obj_set_pos(slot.root, left + i * (item_width + gap), top);
        lv_obj_set_size(slot.root, item_width, item_height);
        lv_obj_set_pos(slot.period, 0, 10); lv_obj_set_width(slot.period, item_width);
        lv_obj_set_style_text_align(slot.period, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_pos(slot.icon, 0, 38); lv_obj_set_width(slot.icon, item_width);
        lv_obj_set_style_text_align(slot.icon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_pos(slot.temperature, 0, 76); lv_obj_set_width(slot.temperature, item_width);
        lv_obj_set_style_text_align(slot.temperature, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_pos(slot.condition, 6, 108); lv_obj_set_width(slot.condition, item_width - 12);
        lv_obj_set_style_text_align(slot.condition, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_label_set_long_mode(slot.condition, LV_LABEL_LONG_DOT);
        lv_obj_set_pos(slot.detail, 0, item_height - 26); lv_obj_set_width(slot.detail, item_width);
        lv_obj_set_style_text_align(slot.detail, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
}

void WeatherModule::layout_daily(int width, int height) {
    const int gap = 10, left = 16, top = 50;
    const int item_width = (width - left * 2 - gap * (HA_MAX_WEATHER_DAILY - 1)) / HA_MAX_WEATHER_DAILY;
    const int item_height = height - top - 14;
    for (int i = 0; i < HA_MAX_WEATHER_DAILY; ++i) {
        auto &slot = daily_[i];
        lv_obj_set_pos(slot.root, left + i * (item_width + gap), top);
        lv_obj_set_size(slot.root, item_width, item_height);
        lv_obj_set_pos(slot.period, 12, 12);
        lv_obj_set_pos(slot.icon, 12, 48);
        lv_obj_set_pos(slot.temperature, 54, 45); lv_obj_set_width(slot.temperature, item_width - 64);
        lv_obj_set_style_text_align(slot.temperature, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        lv_obj_set_pos(slot.condition, 12, 86); lv_obj_set_width(slot.condition, item_width - 24);
        lv_label_set_long_mode(slot.condition, LV_LABEL_LONG_DOT);
        lv_obj_set_pos(slot.detail, 12, item_height - 30); lv_obj_set_width(slot.detail, item_width - 24);
    }
}

void WeatherModule::apply_layout() {
    const PanelConfig &cfg = config_service_get();
    const uint8_t sections = (cfg.weather_show_current ? 1 : 0) |
                             (cfg.weather_show_hourly ? 2 : 0) |
                             (cfg.weather_show_daily ? 4 : 0);
    if (sections == applied_sections_ && strcmp(applied_layout_, cfg.weather_layout) == 0) return;
    applied_sections_ = sections;
    snprintf(applied_layout_, sizeof(applied_layout_), "%s", cfg.weather_layout);
    show(current_card_, cfg.weather_show_current);
    show(hourly_card_, cfg.weather_show_hourly);
    show(daily_card_, cfg.weather_show_daily);

    if (sections == 1 || sections == 2 || sections == 4) {
        lv_obj_t *only = sections == 1 ? current_card_ : sections == 2 ? hourly_card_ : daily_card_;
        lv_obj_set_pos(only, 24, PAGE_CONTENT_TOP); lv_obj_set_size(only, 1232, 522);
    } else if (sections == 6) {
        lv_obj_set_pos(hourly_card_, 24, PAGE_CONTENT_TOP); lv_obj_set_size(hourly_card_, 1232, 250);
        lv_obj_set_pos(daily_card_, 24, 346); lv_obj_set_size(daily_card_, 1232, 256);
    } else if (sections == 3 || sections == 5) {
        lv_obj_t *forecast = sections == 3 ? hourly_card_ : daily_card_;
        lv_obj_set_pos(current_card_, 24, PAGE_CONTENT_TOP); lv_obj_set_size(current_card_, 380, 522);
        lv_obj_set_pos(forecast, 420, PAGE_CONTENT_TOP); lv_obj_set_size(forecast, 836, 522);
    } else {
        const int current_width = strcmp(cfg.weather_layout, "current_focus") == 0 ? 480 :
                                  strcmp(cfg.weather_layout, "forecast_focus") == 0 ? 320 : 360;
        const int forecast_x = 24 + current_width + 16;
        const int forecast_width = 1232 - current_width - 16;
        lv_obj_set_pos(current_card_, 24, PAGE_CONTENT_TOP); lv_obj_set_size(current_card_, current_width, 522);
        lv_obj_set_pos(hourly_card_, forecast_x, PAGE_CONTENT_TOP); lv_obj_set_size(hourly_card_, forecast_width, 250);
        lv_obj_set_pos(daily_card_, forecast_x, 346); lv_obj_set_size(daily_card_, forecast_width, 256);
    }
    // LVGL resolves new coordinates lazily. Resolve the two containers before
    // deriving their child grid so the very first Weather frame is complete.
    if (cfg.weather_show_hourly) {
        lv_obj_update_layout(hourly_card_);
        layout_hourly(lv_obj_get_width(hourly_card_), lv_obj_get_height(hourly_card_));
    }
    if (cfg.weather_show_daily) {
        lv_obj_update_layout(daily_card_);
        layout_daily(lv_obj_get_width(daily_card_), lv_obj_get_height(daily_card_));
    }
    if (cfg.weather_show_current) {
        lv_obj_update_layout(current_card_);
        const int width = lv_obj_get_width(current_card_);
        const int group_x = width > 280 ? (width - 260) / 2 : 10;
        lv_obj_set_pos(current_icon_, group_x, 86);
        lv_obj_set_pos(current_temperature_, group_x + 100, 82);
        lv_obj_set_width(current_temperature_, 160);
        lv_obj_set_pos(current_condition_, 22, 168);
        lv_obj_set_width(current_condition_, width - 44);
        lv_obj_set_pos(current_detail_, 24, 225);
        lv_obj_set_width(current_detail_, width - 48);
    }
}

void WeatherModule::update() {
    if (!current_card_) return;
    apply_layout();
    WeatherViewModel model = {};
    const bool found = ui_state_model_snapshot_weather(model);
    safe_text(subtitle_, model.entity_name[0] ? model.entity_name : "Select a weather entity in Web Admin");
    safe_text(current_temperature_, model.temperature);
    safe_text(current_condition_, model.condition);
    safe_text(current_detail_, model.detail);
    lv_obj_set_style_text_color(current_icon_, lv_color_hex(found && model.available ? ui_theme::CYAN : MUTED), LV_PART_MAIN);
    safe_text(hourly_status_, model.hourly_count ? "Live" : model.loading ? "Loading..." : "Unavailable");
    safe_text(daily_status_, model.daily_count ? "Live" : model.loading ? "Loading..." : "Unavailable");
    for (uint8_t i = 0; i < HA_MAX_WEATHER_HOURLY; ++i) {
        const bool visible = i < model.hourly_count;
        show(hourly_[i].root, visible);
        if (!visible) continue;
        safe_text(hourly_[i].period, model.hourly[i].period);
        safe_text(hourly_[i].temperature, model.hourly[i].temperature);
        safe_text(hourly_[i].condition, model.hourly[i].condition);
        safe_text(hourly_[i].detail, model.hourly[i].detail);
    }
    for (uint8_t i = 0; i < HA_MAX_WEATHER_DAILY; ++i) {
        const bool visible = i < model.daily_count;
        show(daily_[i].root, visible);
        if (!visible) continue;
        safe_text(daily_[i].period, model.daily[i].period);
        safe_text(daily_[i].temperature, model.daily[i].temperature);
        safe_text(daily_[i].condition, model.daily[i].condition);
        safe_text(daily_[i].detail, model.daily[i].detail);
    }
}
