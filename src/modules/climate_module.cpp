#include "climate_module.h"

#include "display_text.h"
#include "ha_icons_font.h"
#include "module_ui.h"
#include "ui_shell.h"

#include <stdio.h>
#include <string.h>

using namespace module_ui;

namespace {
void safe_text(lv_obj_t *target, const char *value) {
    if (!target) return;
    char safe[128];
    panel_display_text(safe, sizeof(safe), value ? value : "");
    lv_label_set_text(target, safe);
}

void option_label(char *out, size_t out_len, const char *value) {
    if (!out || !out_len) return;
    size_t written = 0;
    bool upper = true;
    for (size_t i = 0; value && value[i] && written + 1 < out_len; ++i) {
        char c = value[i];
        if (c == '_') { c = ' '; upper = true; }
        else if (upper && c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
            upper = false;
        } else upper = false;
        out[written++] = c;
    }
    out[written] = '\0';
}

void temperature_text(char *out, size_t out_len, float value, const char *unit,
                      bool available, float step = 1.0f) {
    if (!available) { snprintf(out, out_len, "--"); return; }
    const bool fractional = step > 0.0f && step < 1.0f;
    snprintf(out, out_len, fractional ? "%.1f%s" : "%.0f%s", value,
             unit && unit[0] ? unit : "°");
}

uint32_t mode_color(const char *mode) {
    if (!mode) return ACCENT;
    if (strcmp(mode, "heat") == 0) return 0xFF725C;
    if (strcmp(mode, "cool") == 0) return 0x40B8FF;
    if (strcmp(mode, "off") == 0) return MUTED;
    if (strcmp(mode, "dry") == 0) return 0x48D5C4;
    if (strcmp(mode, "fan_only") == 0) return 0x44D7E8;
    return ACCENT;
}
}

void ClimateModule::create(lv_obj_t *parent) {
    box(parent, BG, 0, 0);
    module_ui::title(parent, "Climate", "Live thermostats, comfort and HVAC controls");

    for (uint8_t i = 0; i < PANEL_MAX_CLIMATE_DEVICES; ++i) {
        tabs_[i] = button(parent, "Thermostat", 24 + i * 307, PAGE_CONTENT_TOP, 295, 46, CARD_ALT);
        lv_obj_t *tab_label = lv_obj_get_child(tabs_[i], 0);
        lv_obj_set_width(tab_label, 263);
        lv_obj_set_style_text_align(tab_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_label_set_long_mode(tab_label, LV_LABEL_LONG_DOT);
        lv_obj_center(tab_label);
        lv_obj_set_user_data(tabs_[i], reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        lv_obj_add_event_cb(tabs_[i], tab_cb, LV_EVENT_CLICKED, this);
        lv_obj_add_flag(tabs_[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Three consistent cards mirror the Room, Weather, and Security layouts:
    // read-only state, primary adjustment, then secondary controls.
    thermostat_card_ = card(parent, 24, 140, 340, 482);
    name_label_ = label(thermostat_card_, "Thermostat", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(name_label_, 22, 18);
    lv_obj_set_width(name_label_, 296);
    lv_label_set_long_mode(name_label_, LV_LABEL_LONG_DOT);
    connection_label_ = label(thermostat_card_, "Waiting for Home Assistant", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(connection_label_, 22, 50);
    lv_obj_set_width(connection_label_, 296);
    lv_label_set_long_mode(connection_label_, LV_LABEL_LONG_DOT);
    lv_obj_t *thermometer = label(thermostat_card_, "", &ha_icons_font, 0xFF725C);
    ui_theme::set_glyph(thermometer, 0xF050F);
    lv_obj_set_pos(thermometer, 62, 112);
    lv_obj_set_size(thermometer, 64, 72);
    lv_obj_t *current_caption = label(thermostat_card_, "CURRENT", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(current_caption, 142, 112);
    current_label_ = label(thermostat_card_, "--", &lv_font_montserrat_28, TEXT);
    lv_obj_set_pos(current_label_, 142, 140);
    lv_obj_set_width(current_label_, 176);

    lv_obj_t *humidity_panel = lv_obj_create(thermostat_card_);
    lv_obj_set_pos(humidity_panel, 20, 218);
    lv_obj_set_size(humidity_panel, 300, 72);
    box(humidity_panel, CARD_ALT, 14, 1);
    lv_obj_t *humidity_icon = label(humidity_panel, "", &ha_icons_font, 0x40B8FF);
    ui_theme::set_glyph(humidity_icon, 0xF058E);
    lv_obj_set_pos(humidity_icon, 20, 17);
    lv_obj_set_size(humidity_icon, 40, 40);
    humidity_label_ = label(humidity_panel, "Humidity --", &lv_font_montserrat_16, TEXT);
    lv_obj_set_pos(humidity_label_, 68, 23);
    lv_obj_set_width(humidity_label_, 210);

    lv_obj_t *state_caption = label(thermostat_card_, "CURRENT STATE", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(state_caption, 22, 330);
    action_label_ = label(thermostat_card_, "Idle", &lv_font_montserrat_24, TEXT);
    lv_obj_set_pos(action_label_, 22, 358);
    lv_obj_set_width(action_label_, 296);
    feedback_label_ = label(thermostat_card_, "Updated from live Home Assistant state",
                            &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(feedback_label_, 22, 420);
    lv_obj_set_width(feedback_label_, 296);
    lv_label_set_long_mode(feedback_label_, LV_LABEL_LONG_WRAP);

    details_card_ = card(parent, 380, 140, 400, 482);
    lv_obj_t *target_caption = label(details_card_, "TEMPERATURE", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(target_caption, 22, 18);
    lv_obj_t *target_subtitle = label(details_card_, "TARGET", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(target_subtitle, 0, 92);
    lv_obj_set_width(target_subtitle, 400);
    lv_obj_set_style_text_align(target_subtitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    target_label_ = label(details_card_, "--", &lv_font_montserrat_28, ACCENT);
    lv_obj_set_pos(target_label_, 24, 120);
    lv_obj_set_width(target_label_, 352);
    lv_obj_set_style_text_align(target_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    target_detail_ = label(details_card_, "No target reported", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(target_detail_, 24, 168);
    lv_obj_set_width(target_detail_, 352);
    lv_obj_set_style_text_align(target_detail_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    minus_button_ = button(details_card_, "-", 22, 238, 166, 94, CARD_ALT);
    plus_button_ = button(details_card_, "+", 212, 238, 166, 94, ACCENT);
    lv_obj_set_style_text_font(lv_obj_get_child(minus_button_, 0), &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_font(lv_obj_get_child(plus_button_, 0), &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_user_data(minus_button_, reinterpret_cast<void *>(static_cast<intptr_t>(-1)));
    lv_obj_set_user_data(plus_button_, reinterpret_cast<void *>(static_cast<intptr_t>(1)));
    lv_obj_add_event_cb(minus_button_, adjust_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(plus_button_, adjust_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *setpoint_hint = label(details_card_, "Use the large touch controls to adjust the active Home Assistant setpoint.",
                                    &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(setpoint_hint, 34, 378);
    lv_obj_set_size(setpoint_hint, 332, 70);
    lv_obj_set_style_text_align(setpoint_hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(setpoint_hint, LV_LABEL_LONG_WRAP);

    controls_card_ = card(parent, 796, 140, 460, 482);
    lv_obj_t *mode_caption = label(controls_card_, "HVAC MODE", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(mode_caption, 22, 18);
    for (uint8_t i = 0; i < HA_MAX_CLIMATE_MODES; ++i) {
        const int column = i % 4;
        const int row = i / 4;
        mode_buttons_[i] = button(controls_card_, "Mode", 22 + column * 105,
                                  54 + row * 58, 98, 48, CARD_ALT);
        lv_obj_set_user_data(mode_buttons_[i], reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        lv_obj_add_event_cb(mode_buttons_[i], mode_cb, LV_EVENT_CLICKED, this);
        lv_obj_add_flag(mode_buttons_[i], LV_OBJ_FLAG_HIDDEN);
    }
    fan_button_ = button(controls_card_, "Fan: Not available", 22, 214, 416, 64, CARD_ALT);
    preset_button_ = button(controls_card_, "Preset: Not available", 22, 294, 416, 64, CARD_ALT);
    lv_obj_add_event_cb(fan_button_, fan_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(preset_button_, preset_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *hint = label(controls_card_, "Tap Fan or Preset to advance through the options reported by this device.",
                           &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(hint, 32, 400);
    lv_obj_set_size(hint, 396, 58);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);

    empty_card_ = card(parent, 24, 140, 1232, 482);
    lv_obj_t *empty_icon = label(empty_card_, "", &ha_icons_font, MUTED);
    ui_theme::set_glyph(empty_icon, 0xF0393);
    lv_obj_set_pos(empty_icon, 558, 116);
    lv_obj_set_size(empty_icon, 116, 82);
    lv_obj_set_style_text_align(empty_icon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_t *empty_title = label(empty_card_, "No Climate devices configured", &lv_font_montserrat_28, TEXT);
    lv_obj_set_pos(empty_title, 0, 220);
    lv_obj_set_width(empty_title, 1232);
    lv_obj_set_style_text_align(empty_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_t *empty_detail = label(empty_card_, "Open Web Admin > Climate and select up to four climate.* entities.",
                                  &lv_font_montserrat_16, MUTED);
    lv_obj_set_pos(empty_detail, 0, 270);
    lv_obj_set_width(empty_detail, 1232);
    lv_obj_set_style_text_align(empty_detail, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    update();
}

void ClimateModule::update() {
    if (!thermostat_card_) return;
    ui_state_model_snapshot_climate(selected_index_, view_);
    if (selected_index_ >= view_.tab_count) selected_index_ = 0;
    const bool configured = view_.configured;
    if (configured) {
        lv_obj_remove_flag(thermostat_card_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(details_card_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(controls_card_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(empty_card_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(thermostat_card_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(details_card_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(controls_card_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(empty_card_, LV_OBJ_FLAG_HIDDEN);
    }
    for (uint8_t i = 0; i < PANEL_MAX_CLIMATE_DEVICES; ++i) {
        if (i >= view_.tab_count) { lv_obj_add_flag(tabs_[i], LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(tabs_[i], LV_OBJ_FLAG_HIDDEN);
        safe_text(lv_obj_get_child(tabs_[i], 0), view_.tabs[i].label);
        lv_obj_set_style_bg_color(tabs_[i], lv_color_hex(i == selected_index_ ? ACCENT : CARD_ALT), LV_PART_MAIN);
        lv_obj_set_style_border_color(tabs_[i], lv_color_hex(view_.tabs[i].available ?
                                      (i == selected_index_ ? 0x66D9FF : BORDER) : DANGER), LV_PART_MAIN);
    }
    if (!configured) return;

    safe_text(name_label_, view_.name);
    safe_text(connection_label_, view_.available ? view_.entity_id : "Unavailable - waiting for Home Assistant");
    lv_obj_set_style_text_color(connection_label_, lv_color_hex(view_.available ? MUTED : DANGER), LV_PART_MAIN);
    char text[96];
    temperature_text(text, sizeof(text), view_.current_temperature, view_.temperature_unit,
                     view_.available && view_.has_current_temperature, view_.target_step);
    safe_text(current_label_, text);
    if (view_.has_target_range) {
        char low[24], high[24];
        temperature_text(low, sizeof(low), view_.target_low, view_.temperature_unit, view_.available, view_.target_step);
        temperature_text(high, sizeof(high), view_.target_high, view_.temperature_unit, view_.available, view_.target_step);
        snprintf(text, sizeof(text), "%s - %s", low, high);
        safe_text(target_label_, text);
        safe_text(target_detail_, "Automatic comfort range");
    } else {
        temperature_text(text, sizeof(text), view_.target_temperature, view_.temperature_unit,
                         view_.available && view_.has_target_temperature, view_.target_step);
        safe_text(target_label_, text);
        safe_text(target_detail_, view_.has_target_temperature ? "Temperature setpoint" : "No target reported");
    }
    const bool adjustable = view_.command_ready && (view_.has_target_temperature || view_.has_target_range);
    set_enabled(minus_button_, adjustable);
    set_enabled(plus_button_, adjustable);
    if (view_.show_humidity && view_.has_humidity && view_.available) {
        snprintf(text, sizeof(text), "Humidity %.0f%%", view_.humidity);
        safe_text(humidity_label_, text);
        lv_obj_remove_flag(humidity_label_, LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(humidity_label_, LV_OBJ_FLAG_HIDDEN);

    char readable[48];
    option_label(readable, sizeof(readable), view_.hvac_action[0] ? view_.hvac_action : view_.hvac_mode);
    safe_text(action_label_, !view_.available ? "Unavailable" : readable[0] ? readable : "Idle");
    lv_obj_set_style_text_color(action_label_, lv_color_hex(view_.available ? mode_color(view_.hvac_mode) : DANGER), LV_PART_MAIN);
    for (uint8_t i = 0; i < HA_MAX_CLIMATE_MODES; ++i) {
        if (i >= view_.hvac_mode_count) { lv_obj_add_flag(mode_buttons_[i], LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(mode_buttons_[i], LV_OBJ_FLAG_HIDDEN);
        option_label(readable, sizeof(readable), view_.hvac_modes[i]);
        safe_text(lv_obj_get_child(mode_buttons_[i], 0), readable);
        const bool selected = strcmp(view_.hvac_modes[i], view_.hvac_mode) == 0;
        lv_obj_set_style_bg_color(mode_buttons_[i], lv_color_hex(selected ? mode_color(view_.hvac_mode) : CARD_ALT), LV_PART_MAIN);
        set_enabled(mode_buttons_[i], view_.command_ready);
    }
    if (view_.show_fan && view_.fan_mode_count) {
        option_label(readable, sizeof(readable), view_.fan_mode[0] ? view_.fan_mode : view_.fan_modes[0]);
        snprintf(text, sizeof(text), "Fan  •  %s", readable);
        safe_text(lv_obj_get_child(fan_button_, 0), text);
        lv_obj_remove_flag(fan_button_, LV_OBJ_FLAG_HIDDEN);
        set_enabled(fan_button_, view_.command_ready);
    } else lv_obj_add_flag(fan_button_, LV_OBJ_FLAG_HIDDEN);
    if (view_.show_presets && view_.preset_count) {
        option_label(readable, sizeof(readable), view_.preset_mode[0] ? view_.preset_mode : view_.presets[0]);
        snprintf(text, sizeof(text), "Preset  •  %s", readable);
        safe_text(lv_obj_get_child(preset_button_, 0), text);
        lv_obj_remove_flag(preset_button_, LV_OBJ_FLAG_HIDDEN);
        set_enabled(preset_button_, view_.command_ready);
    } else lv_obj_add_flag(preset_button_, LV_OBJ_FLAG_HIDDEN);
}

void ClimateModule::tab_cb(lv_event_t *event) {
    auto *self = static_cast<ClimateModule *>(lv_event_get_user_data(event));
    if (!self) return;
    const intptr_t index = reinterpret_cast<intptr_t>(lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_target(event))));
    if (index < 0 || index >= self->view_.tab_count) return;
    self->selected_index_ = static_cast<uint8_t>(index);
    self->update();
}

void ClimateModule::adjust_cb(lv_event_t *event) {
    auto *self = static_cast<ClimateModule *>(lv_event_get_user_data(event));
    if (!self) return;
    const int direction = static_cast<int>(reinterpret_cast<intptr_t>(
        lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_target(event)))));
    const bool queued = ui_state_model_climate_adjust_target(self->view_, direction);
    ui_shell_report_status(queued ? "Climate setpoint queued for Home Assistant" :
                                    "Climate setpoint could not be queued");
}

void ClimateModule::mode_cb(lv_event_t *event) {
    auto *self = static_cast<ClimateModule *>(lv_event_get_user_data(event));
    if (!self) return;
    const intptr_t index = reinterpret_cast<intptr_t>(lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_target(event))));
    if (index < 0 || index >= self->view_.hvac_mode_count) return;
    const bool queued = ui_state_model_climate_set_hvac_mode(self->view_, self->view_.hvac_modes[index]);
    ui_shell_report_status(queued ? "HVAC mode queued for Home Assistant" : "HVAC mode could not be queued");
}

void ClimateModule::fan_cb(lv_event_t *event) {
    auto *self = static_cast<ClimateModule *>(lv_event_get_user_data(event));
    if (!self || !self->view_.fan_mode_count) return;
    uint8_t next = 0;
    for (uint8_t i = 0; i < self->view_.fan_mode_count; ++i)
        if (strcmp(self->view_.fan_modes[i], self->view_.fan_mode) == 0)
            next = (i + 1) % self->view_.fan_mode_count;
    const bool queued = ui_state_model_climate_set_fan_mode(self->view_, self->view_.fan_modes[next]);
    ui_shell_report_status(queued ? "Climate fan mode queued for Home Assistant" : "Fan mode could not be queued");
}

void ClimateModule::preset_cb(lv_event_t *event) {
    auto *self = static_cast<ClimateModule *>(lv_event_get_user_data(event));
    if (!self || !self->view_.preset_count) return;
    uint8_t next = 0;
    for (uint8_t i = 0; i < self->view_.preset_count; ++i)
        if (strcmp(self->view_.presets[i], self->view_.preset_mode) == 0)
            next = (i + 1) % self->view_.preset_count;
    const bool queued = ui_state_model_climate_set_preset(self->view_, self->view_.presets[next]);
    ui_shell_report_status(queued ? "Climate preset queued for Home Assistant" : "Preset could not be queued");
}
