#include "security_module.h"

#include "display_text.h"
#include "ha_icons_font.h"
#include "module_ui.h"
#include "ui_theme.h"

#include <stdio.h>
#include <string.h>

using namespace module_ui;

namespace {
const char *const MODES[] = {"home", "away", "night", "vacation"};
const char *const MODE_LABELS[] = {"Home", "Away", "Night", "Vacation"};

void safe_text(lv_obj_t *target, const char *value) {
    if (!target) return;
    char safe[384];
    panel_display_text(safe, sizeof(safe), value ? value : "");
    lv_label_set_text(target, safe);
}

bool is_disarmed(const SecurityViewModel &view) {
    return view.available && strcmp(view.alarm_state, "disarmed") == 0;
}
}

void SecurityModule::create(lv_obj_t *parent) {
    box(parent, BG, 0, 0);
    module_ui::title(parent, id(), "Security", "Alarmo protection and monitored-device status");

    alarm_card_ = card(parent, 24, PAGE_CONTENT_TOP, 720, 236);
    alarm_icon_ = label(alarm_card_, "", &ha_icons_font, SUCCESS);
    lv_obj_set_pos(alarm_icon_, 26, 28);
    lv_obj_set_size(alarm_icon_, 76, 76);
    lv_obj_set_style_text_align(alarm_icon_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    ui_theme::set_glyph(alarm_icon_, 0xF068A);
    alarm_name_ = label(alarm_card_, "Alarmo", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(alarm_name_, 118, 22);
    lv_obj_set_width(alarm_name_, 570);
    lv_label_set_long_mode(alarm_name_, LV_LABEL_LONG_DOT);
    state_label_ = label(alarm_card_, "NOT CONFIGURED", &lv_font_montserrat_28, TEXT);
    lv_obj_set_pos(state_label_, 118, 48);
    lv_obj_set_width(state_label_, 570);
    lv_label_set_long_mode(state_label_, LV_LABEL_LONG_DOT);
    state_detail_ = label(alarm_card_, "Choose an Alarmo entity in Web Admin", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(state_detail_, 118, 86);
    lv_obj_set_width(state_detail_, 570);
    lv_label_set_long_mode(state_detail_, LV_LABEL_LONG_DOT);

    for (uint8_t i = 0; i < 4; ++i) {
        mode_bindings_[i] = {this, MODES[i]};
        arm_buttons_[i] = button(alarm_card_, MODE_LABELS[i], 18 + i * 137, 164, 127, 54, CARD_ALT);
        lv_obj_add_event_cb(arm_buttons_[i], mode_cb, LV_EVENT_CLICKED, &mode_bindings_[i]);
    }
    mode_bindings_[4] = {this, "disarm"};
    disarm_button_ = button(alarm_card_, "Disarm", 566, 164, 136, 54, DANGER);
    lv_obj_add_event_cb(disarm_button_, mode_cb, LV_EVENT_CLICKED, &mode_bindings_[4]);

    summary_card_ = card(parent, 760, PAGE_CONTENT_TOP, 496, 236);
    lv_obj_t *summary_heading = label(summary_card_, "DYNAMIC ATTENTION", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(summary_heading, 22, 20);
    summary_title_ = label(summary_card_, "All clear", &lv_font_montserrat_24, SUCCESS);
    lv_obj_set_pos(summary_title_, 22, 48);
    lv_obj_set_width(summary_title_, 452);
    summary_detail_ = label(summary_card_, "No monitored devices configured", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(summary_detail_, 22, 88);
    lv_obj_set_size(summary_detail_, 452, 124);
    lv_label_set_long_mode(summary_detail_, LV_LABEL_LONG_WRAP);

    lv_obj_t *devices_heading = label(parent, "Monitored devices", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(devices_heading, 24, 330);
    feedback_ = label(parent, "Persistent cards; dynamic devices appear above only when abnormal", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(feedback_, 300, 335);
    lv_obj_set_width(feedback_, 956);
    lv_obj_set_style_text_align(feedback_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_long_mode(feedback_, LV_LABEL_LONG_DOT);

    for (uint8_t i = 0; i < PANEL_MAX_SECURITY_DEVICES; ++i) {
        const int column = i % 4;
        const int row = i / 4;
        ui_card_create(device_cards_[i], parent, UiCardVariant::STATUS,
                       24 + column * 307, 364 + row * 122, 295, 110);
        ui_card_set_visible(device_cards_[i], false);
    }

    confirm_overlay_ = lv_obj_create(parent);
    lv_obj_set_pos(confirm_overlay_, 0, 0);
    lv_obj_set_size(confirm_overlay_, 1280, lv_pct(100));
    box(confirm_overlay_, 0x020A14, 0, 0);
    lv_obj_set_style_bg_opa(confirm_overlay_, LV_OPA_80, LV_PART_MAIN);
    lv_obj_add_event_cb(confirm_overlay_, cancel_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *confirm_dialog = card(confirm_overlay_, 345, 166, 590, 292);
    lv_obj_remove_flag(confirm_dialog, LV_OBJ_FLAG_EVENT_BUBBLE);
    confirm_title_ = label(confirm_dialog, "Confirm arming", &lv_font_montserrat_28, TEXT);
    lv_obj_set_pos(confirm_title_, 28, 28);
    confirm_detail_ = label(confirm_dialog, "", &lv_font_montserrat_16, MUTED);
    lv_obj_set_pos(confirm_detail_, 28, 84);
    lv_obj_set_size(confirm_detail_, 534, 90);
    lv_label_set_long_mode(confirm_detail_, LV_LABEL_LONG_WRAP);
    lv_obj_t *confirm_cancel = button(confirm_dialog, "Cancel", 28, 210, 250, 56, CARD_ALT);
    lv_obj_t *confirm_go = button(confirm_dialog, "Arm", 312, 210, 250, 56, ACCENT);
    lv_obj_add_event_cb(confirm_cancel, cancel_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(confirm_go, confirm_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(confirm_overlay_, LV_OBJ_FLAG_HIDDEN);

    keypad_overlay_ = lv_obj_create(parent);
    lv_obj_set_pos(keypad_overlay_, 0, 0);
    lv_obj_set_size(keypad_overlay_, 1280, lv_pct(100));
    box(keypad_overlay_, 0x020A14, 0, 0);
    lv_obj_set_style_bg_opa(keypad_overlay_, LV_OPA_80, LV_PART_MAIN);
    lv_obj_add_event_cb(keypad_overlay_, cancel_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *keypad = card(keypad_overlay_, 405, 22, 470, 612);
    lv_obj_remove_flag(keypad, LV_OBJ_FLAG_EVENT_BUBBLE);
    keypad_title_ = label(keypad, "Enter code", &lv_font_montserrat_24, TEXT);
    lv_obj_set_pos(keypad_title_, 24, 20);
    lv_obj_set_width(keypad_title_, 422);
    lv_obj_set_style_text_align(keypad_title_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    keypad_dots_ = label(keypad, "Enter PIN", &lv_font_montserrat_28, ACCENT);
    lv_obj_set_pos(keypad_dots_, 24, 58);
    lv_obj_set_width(keypad_dots_, 422);
    lv_obj_set_style_text_align(keypad_dots_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    const char *keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "Clear", "0", "Back"};
    const intptr_t values[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, -2, 0, -1};
    for (int i = 0; i < 12; ++i) {
        lv_obj_t *key = button(keypad, keys[i], 24 + (i % 3) * 142, 106 + (i / 3) * 86,
                               126, 70, CARD_ALT);
        lv_obj_set_user_data(key, reinterpret_cast<void *>(values[i]));
        lv_obj_add_event_cb(key, keypad_key_cb, LV_EVENT_CLICKED, this);
    }
    lv_obj_t *keypad_cancel = button(keypad, "Cancel", 24, 466, 198, 62, CARD_ALT);
    keypad_submit_ = button(keypad, "Disarm", 248, 466, 198, 62, DANGER);
    lv_obj_add_event_cb(keypad_cancel, cancel_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(keypad_submit_, keypad_submit_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *privacy = label(keypad, "Code is sent to Home Assistant and is never saved", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(privacy, 24, 552);
    lv_obj_set_width(privacy, 422);
    lv_obj_set_style_text_align(privacy, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_add_flag(keypad_overlay_, LV_OBJ_FLAG_HIDDEN);

    update();
}

void SecurityModule::update() {
    if (!state_label_) return;
    ui_state_model_snapshot_security(view_);
    safe_text(alarm_name_, view_.configured ? view_.alarm_name : "Alarmo");
    safe_text(state_label_, view_.state_label);
    safe_text(state_detail_, view_.state_detail);
    const uint32_t alarm_color = view_.triggered ? DANGER : view_.armed ? WARN :
                                 view_.available ? SUCCESS : MUTED;
    lv_obj_set_style_text_color(state_label_, lv_color_hex(alarm_color), LV_PART_MAIN);
    lv_obj_set_style_text_color(alarm_icon_, lv_color_hex(alarm_color), LV_PART_MAIN);

    const bool modes_enabled[] = {view_.arm_home, view_.arm_away, view_.arm_night, view_.arm_vacation};
    const bool can_arm = view_.command_ready && is_disarmed(view_);
    uint8_t visible_modes = 0;
    for (bool enabled : modes_enabled) if (enabled) ++visible_modes;
    const int mode_gap = 10;
    const int mode_width = visible_modes ? (530 - mode_gap * (visible_modes - 1)) / visible_modes : 127;
    uint8_t visible_index = 0;
    for (uint8_t i = 0; i < 4; ++i) {
        if (modes_enabled[i]) {
            lv_obj_remove_flag(arm_buttons_[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_x(arm_buttons_[i], 18 + visible_index * (mode_width + mode_gap));
            lv_obj_set_width(arm_buttons_[i], mode_width);
            ++visible_index;
        } else lv_obj_add_flag(arm_buttons_[i], LV_OBJ_FLAG_HIDDEN);
        set_enabled(arm_buttons_[i], can_arm && modes_enabled[i]);
    }
    set_enabled(disarm_button_, view_.command_ready && !is_disarmed(view_));

    if (view_.show_abnormal_summary) {
        lv_obj_remove_flag(summary_card_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(alarm_card_, 720);
        safe_text(summary_title_, view_.abnormal_count ? "Attention needed" : "All clear");
        lv_obj_set_style_text_color(summary_title_, lv_color_hex(view_.abnormal_count ? DANGER : SUCCESS), LV_PART_MAIN);
        char details[384] = {};
        snprintf(details, sizeof(details), "%s", view_.abnormal_summary);
        uint8_t listed = 0;
        auto append_attention = [&](const SecurityDeviceViewModel &device) {
            if (!device.abnormal || listed >= 4) return;
            const size_t used = strlen(details);
            snprintf(details + used, sizeof(details) - used, "\n%s - %s",
                     device.title, device.state_text);
            ++listed;
        };
        for (uint8_t i = 0; i < view_.device_count; ++i)
            append_attention(view_.devices[i]);
        for (uint8_t i = 0; i < view_.dynamic_device_count; ++i)
            append_attention(view_.dynamic_devices[i]);
        if (view_.abnormal_count > listed) {
            const size_t used = strlen(details);
            snprintf(details + used, sizeof(details) - used, "\n+%u more",
                     static_cast<unsigned>(view_.abnormal_count - listed));
        }
        safe_text(summary_detail_, details);
    } else {
        lv_obj_add_flag(summary_card_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(alarm_card_, 1232);
    }

    for (uint8_t i = 0; i < PANEL_MAX_SECURITY_DEVICES; ++i) {
        const bool visible = i < view_.device_count;
        ui_card_set_visible(device_cards_[i], visible);
        if (!visible) continue;
        const SecurityDeviceViewModel &device = view_.devices[i];
        const uint32_t color = device.abnormal ? ui_theme::status_color(device.color) : SUCCESS;
        const uint32_t glyph = ui_theme::status_glyph(device.icon, device.entity_id,
                                                       device.title, device.abnormal);
        ui_card_set_content(device_cards_[i], glyph, color, device.title, device.state_text);
        ui_card_set_state(device_cards_[i], device.abnormal, device.available);
    }
}

void SecurityModule::request_mode(const char *mode) {
    if (!mode) return;
    if (strcmp(mode, "disarm") == 0 || view_.code_to_arm) show_keypad(mode);
    else if (view_.confirm_arming) show_confirmation(mode);
    else {
        snprintf(pending_mode_, sizeof(pending_mode_), "%s", mode);
        execute_mode();
    }
}

void SecurityModule::show_keypad(const char *mode) {
    snprintf(pending_mode_, sizeof(pending_mode_), "%s", mode ? mode : "");
    clear_code();
    char heading[64];
    snprintf(heading, sizeof(heading), "Enter code to %s",
             strcmp(pending_mode_, "disarm") == 0 ? "disarm" : "arm");
    safe_text(keypad_title_, heading);
    safe_text(lv_obj_get_child(keypad_submit_, 0),
              strcmp(pending_mode_, "disarm") == 0 ? "Disarm" : "Arm");
    lv_obj_remove_flag(keypad_overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(keypad_overlay_);
}

void SecurityModule::show_confirmation(const char *mode) {
    snprintf(pending_mode_, sizeof(pending_mode_), "%s", mode ? mode : "");
    char detail[180];
    snprintf(detail, sizeof(detail),
             "Arm %s using %s?\n\nHome Assistant will report the exit delay and final armed state.",
             mode ? mode : "", view_.alarm_name);
    safe_text(confirm_detail_, detail);
    lv_obj_remove_flag(confirm_overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(confirm_overlay_);
}

void SecurityModule::execute_mode(const char *code) {
    const bool queued = ui_state_model_security_action(view_, pending_mode_, code);
    safe_text(feedback_, queued ? "Command queued - waiting for Alarmo" :
                                 "Could not queue command - check Alarmo and Home Assistant");
    close_overlays();
}

void SecurityModule::clear_code() {
    memset(code_, 0, sizeof(code_));
    code_length_ = 0;
    update_code_display();
}

void SecurityModule::update_code_display() {
    char dots[32] = {};
    for (uint8_t i = 0; i < code_length_ && i < 12; ++i) {
        dots[i * 2] = '*';
        dots[i * 2 + 1] = ' ';
    }
    safe_text(keypad_dots_, code_length_ ? dots : "Enter PIN");
    set_enabled(keypad_submit_, code_length_ > 0);
}

void SecurityModule::close_overlays() {
    if (confirm_overlay_) lv_obj_add_flag(confirm_overlay_, LV_OBJ_FLAG_HIDDEN);
    if (keypad_overlay_) lv_obj_add_flag(keypad_overlay_, LV_OBJ_FLAG_HIDDEN);
    memset(pending_mode_, 0, sizeof(pending_mode_));
    clear_code();
}

void SecurityModule::on_deactivate() { close_overlays(); }

void SecurityModule::mode_cb(lv_event_t *event) {
    auto *binding = static_cast<ModeBinding *>(lv_event_get_user_data(event));
    if (binding && binding->owner) binding->owner->request_mode(binding->mode);
}

void SecurityModule::confirm_cb(lv_event_t *event) {
    auto *owner = static_cast<SecurityModule *>(lv_event_get_user_data(event));
    if (owner) owner->execute_mode();
}

void SecurityModule::cancel_cb(lv_event_t *event) {
    auto *owner = static_cast<SecurityModule *>(lv_event_get_user_data(event));
    if (owner) owner->close_overlays();
}

void SecurityModule::keypad_key_cb(lv_event_t *event) {
    auto *owner = static_cast<SecurityModule *>(lv_event_get_user_data(event));
    if (!owner) return;
    const intptr_t value = reinterpret_cast<intptr_t>(lv_obj_get_user_data(
        static_cast<lv_obj_t *>(lv_event_get_target(event))));
    if (value == -2) owner->clear_code();
    else if (value == -1) {
        if (owner->code_length_) owner->code_[--owner->code_length_] = '\0';
    } else if (value >= 0 && value <= 9 && owner->code_length_ < sizeof(owner->code_) - 1) {
        owner->code_[owner->code_length_++] = static_cast<char>('0' + value);
        owner->code_[owner->code_length_] = '\0';
    }
    owner->update_code_display();
}

void SecurityModule::keypad_submit_cb(lv_event_t *event) {
    auto *owner = static_cast<SecurityModule *>(lv_event_get_user_data(event));
    if (owner && owner->code_length_) owner->execute_mode(owner->code_);
}
