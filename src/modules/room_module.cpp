#include "room_module.h"

#include "config_service.h"
#include "display_text.h"
#include "ha_icons_font.h"
#include "module_ui.h"
#include "ui_shell.h"
#include "ui_theme.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

using namespace module_ui;

namespace {
constexpr const char *GROUP_NAMES[] = {"Lights", "Devices", "Shades", "Scenes"};
constexpr uint32_t GROUP_ICONS[] = {0xF0335, 0xF07E9, 0xF00AC, 0xF03D8};
// Keep the glyph set within the small embedded MDI subset compiled into
// ha_icons_font; this avoids missing-glyph boxes on the panel.
constexpr uint32_t STATUS_ICONS[] = {0xF0335, 0xF1011, 0xF07E9, 0xF0425};

void display(lv_obj_t *label, const char *value) {
    if (!label) return;
    char safe[192];
    panel_display_text(safe, sizeof(safe), value ? value : "");
    lv_label_set_text(label, safe);
}

int group_for(UiControlKind kind) {
    if (kind == UiControlKind::Light) return 0;
    if (kind == UiControlKind::Cover) return 2;
    if (kind == UiControlKind::Scene) return 3;
    return 1;
}

uint32_t glyph_for(UiControlKind kind, bool active) {
    switch (kind) {
        case UiControlKind::Light: return active ? 0xF0335 : 0xF0336;
        case UiControlKind::Fan: return active ? 0xF0210 : 0xF081D;
        case UiControlKind::Cover: return active ? 0xF1011 : 0xF00AC;
        case UiControlKind::Scene: return active ? 0xF03D8 : 0xF0E0C;
        case UiControlKind::Switch: return active ? 0xF07E9 : 0xF0902;
        default: return 0xF0425;
    }
}

UiCardVariant variant_for(const RoomControlViewModel &control) {
    if (control.kind == UiControlKind::Scene) return UiCardVariant::ACTION;
    if (control.supports_level) return UiCardVariant::SLIDER;
    return UiCardVariant::CONTROL;
}

lv_obj_t *section_heading(lv_obj_t *parent, const char *value, int y) {
    lv_obj_t *heading = label(parent, value, &lv_font_montserrat_24, TEXT);
    lv_obj_set_pos(heading, 26, y);
    return heading;
}
}

void RoomModule::create(lv_obj_t *parent) {
    box(parent, BG, 0, 0);
    ui_state_model_snapshot_room(room_, controls_, PANEL_MAX_ROOM_CONTROLS, control_count_);

    lv_obj_t *room_status = card(parent, 24, 10, 1232, 78);
    lv_obj_set_style_radius(room_status, 14, LV_PART_MAIN);
    room_selector_ = lv_dropdown_create(room_status);
    lv_obj_set_pos(room_selector_, 12, 10);
    lv_obj_set_size(room_selector_, 270, 58);
    ui_theme::surface(room_selector_, ui_theme::SURFACE_RAISED, 12, 1);
    lv_obj_set_style_text_font(room_selector_, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(room_selector_, lv_color_hex(TEXT), LV_PART_MAIN);
    lv_obj_set_style_pad_left(room_selector_, 18, LV_PART_MAIN);
    lv_dropdown_set_symbol(room_selector_, nullptr);
    char room_options[PANEL_MAX_ROOMS * (PANEL_ROOM_NAME_LEN + 1)] = {};
    const PanelConfig &cfg = config_service_get();
    for (uint8_t i = 0; i < cfg.room_count; ++i) {
        char safe[PANEL_ROOM_NAME_LEN];
        panel_display_text(safe, sizeof(safe), cfg.rooms[i].tab_label);
        if (i) strncat(room_options, "\n", sizeof(room_options) - strlen(room_options) - 1);
        strncat(room_options, safe, sizeof(room_options) - strlen(room_options) - 1);
    }
    lv_dropdown_set_options(room_selector_, room_options[0] ? room_options : "Room");
    lv_dropdown_set_selected(room_selector_, room_.active_room);
    lv_obj_set_ext_click_area(room_selector_, 8);
    lv_obj_add_event_cb(room_selector_, room_changed_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_t *room_chevron = label(room_status, "v", &lv_font_montserrat_18, MUTED);
    lv_obj_set_pos(room_chevron, 254, 29);

    const int metric_x[] = {310, 530, 750, 970};
    for (int i = 0; i < 4; ++i) {
        lv_obj_t *divider = lv_obj_create(room_status);
        box(divider, BORDER, 0, 0);
        lv_obj_set_pos(divider, metric_x[i] - 12, 14);
        lv_obj_set_size(divider, 1, 50);
        lv_obj_t *icon = label(room_status, "", &ha_icons_font,
                               i == 0 ? 0xFF715F : i == 1 ? 0x4AA5FF : i == 3 ? ui_theme::SUCCESS : TEXT);
        ui_theme::set_glyph(icon, STATUS_ICONS[i]);
        lv_obj_set_pos(icon, metric_x[i], 22);
        lv_obj_set_size(icon, 42, 40);
        lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        status_values_[i] = label(room_status, "--", &lv_font_montserrat_18, TEXT);
        lv_obj_set_pos(status_values_[i], metric_x[i] + 50, 13);
        lv_obj_set_width(status_values_[i], i == 3 ? 170 : 140);
        status_captions_[i] = label(room_status, "", &lv_font_montserrat_12, MUTED);
        lv_obj_set_pos(status_captions_[i], metric_x[i] + 50, 43);
    }
    display(status_captions_[0], "Temperature");
    display(status_captions_[1], "Humidity");
    display(status_captions_[2], "Devices Online");
    display(status_captions_[3], "System Status");

    section_heading(parent, "Favorite Controls", 102);
    for (int i = 0; i < 4; ++i) {
        favorites_[i].owner = this;
        const UiCardVariant variant = i < room_.favorite_count
                                          ? variant_for(room_.favorites[i])
                                          : UiCardVariant::CONTROL;
        ui_card_create(favorites_[i].card, parent, variant, 24 + i * 312, 138, 296, 170);
        lv_obj_add_event_cb(favorites_[i].card.root, action_cb, LV_EVENT_CLICKED, &favorites_[i]);
        if (favorites_[i].card.slider)
            lv_obj_add_event_cb(favorites_[i].card.slider, slider_cb, LV_EVENT_ALL, &favorites_[i]);
    }

    empty_ = card(parent, 24, 138, 1232, 170);
    lv_obj_t *empty_title = label(empty_, "Build your favorites", &lv_font_montserrat_24, TEXT);
    lv_obj_set_pos(empty_title, 28, 38);
    lv_obj_t *empty_copy = label(empty_, "Choose up to four primary controls in Web Admin. Everything else remains available below.",
                                 &lv_font_montserrat_16, MUTED);
    lv_obj_set_pos(empty_copy, 28, 82);

    section_heading(parent, "Quick Access", 340);
    for (int i = 0; i < 4; ++i) {
        groups_[i].owner = this;
        groups_[i].index = static_cast<uint8_t>(i);
        ui_card_create(groups_[i].card, parent, UiCardVariant::NAVIGATION,
                       24 + i * 312, 376, 296, 154);
        lv_obj_add_event_cb(groups_[i].card.root, group_cb, LV_EVENT_CLICKED, &groups_[i]);
    }

    overlay_ = lv_obj_create(parent);
    lv_obj_set_pos(overlay_, 0, 0);
    lv_obj_set_size(overlay_, 1280, lv_pct(100));
    box(overlay_, 0x020A14, 0, 0);
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_80, LV_PART_MAIN);
    lv_obj_add_event_cb(overlay_, close_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *sheet = card(overlay_, 116, 18, 1048, 590);
    lv_obj_set_style_radius(sheet, 24, LV_PART_MAIN);
    lv_obj_remove_flag(sheet, LV_OBJ_FLAG_EVENT_BUBBLE);
    popup_title_ = label(sheet, "", &lv_font_montserrat_24, TEXT);
    lv_obj_set_pos(popup_title_, 24, 22);
    popup_feedback_ = label(sheet, "", &lv_font_montserrat_14, MUTED);
    lv_obj_align(popup_feedback_, LV_ALIGN_BOTTOM_MID, 0, -24);
    lv_obj_t *close = button(sheet, "Close", 870, 12, 154, 52);
    lv_obj_add_event_cb(close, close_cb, LV_EVENT_CLICKED, this);
    for (int i = 0; i < 6; ++i) {
        popup_cards_[i].owner = this;
        ui_card_create(popup_cards_[i].card, sheet, UiCardVariant::SLIDER,
                       24 + (i % 2) * 508, 80 + (i / 2) * 140, 492, 124);
        lv_obj_add_event_cb(popup_cards_[i].card.root, action_cb, LV_EVENT_CLICKED, &popup_cards_[i]);
        lv_obj_add_event_cb(popup_cards_[i].card.slider, slider_cb, LV_EVENT_ALL, &popup_cards_[i]);
    }
    previous_ = button(sheet, "Previous", 24, 522, 160, 48);
    next_ = button(sheet, "Next", 864, 522, 160, 48);
    lv_obj_add_event_cb(previous_, page_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(next_, page_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
    update();
}

void RoomModule::bind(BoundCard &slot, const RoomControlViewModel *control) {
    if (!control) {
        slot.dragging = false;
        slot.control = {};
        ui_card_set_visible(slot.card, false);
        return;
    }
    if (slot.dragging) return;
    slot.control = *control;
    const uint32_t icon_color = !control->available ? MUTED :
                                control->kind == UiControlKind::Light ? ui_theme::YELLOW :
                                control->kind == UiControlKind::Fan ? ui_theme::CYAN : 0x8EA7FF;
    ui_card_set_content(slot.card, glyph_for(control->kind, control->active), icon_color,
                        control->title, control->state_text);
    ui_card_set_state(slot.card, control->active, control->available);
    if (slot.card.slider) {
        const bool show_level = control->supports_level && control->kind != UiControlKind::Scene;
        if (show_level) lv_obj_remove_flag(slot.card.slider, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(slot.card.slider, LV_OBJ_FLAG_HIDDEN);
        if (slot.card.value) {
            if (show_level) lv_obj_remove_flag(slot.card.value, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(slot.card.value, LV_OBJ_FLAG_HIDDEN);
        }
        if (show_level) ui_card_set_level(slot.card, control->active ? control->level_pct : 0);
    }
    if (slot.card.toggle) {
        if (control->kind == UiControlKind::Scene) lv_obj_add_flag(slot.card.toggle, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(slot.card.toggle, LV_OBJ_FLAG_HIDDEN);
    }
    ui_card_set_visible(slot.card, true);
}

void RoomModule::update() {
    ui_state_model_snapshot_room(room_, controls_, PANEL_MAX_ROOM_CONTROLS, control_count_);
    if (room_selector_ && lv_dropdown_get_selected(room_selector_) != room_.active_room)
        lv_dropdown_set_selected(room_selector_, room_.active_room);
    for (int i = 0; i < 4; ++i)
        bind(favorites_[i], i < room_.favorite_count ? &room_.favorites[i] : nullptr);
    if (room_.favorite_count) lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(empty_, LV_OBJ_FLAG_HIDDEN);

    char summary[48];
    snprintf(summary, sizeof(summary), "%u on / %u total", room_.group_active[0], room_.group_total[0]);
    ui_card_set_content(groups_[0].card, GROUP_ICONS[0], ui_theme::YELLOW, "Lights", summary);
    snprintf(summary, sizeof(summary), "%u online", room_.devices_online);
    ui_card_set_content(groups_[1].card, GROUP_ICONS[1], 0xAFC6FF, "Devices", summary);
    snprintf(summary, sizeof(summary), "%u open", room_.group_active[2]);
    ui_card_set_content(groups_[2].card, GROUP_ICONS[2], 0xAFC6FF, "Shades", summary);
    snprintf(summary, sizeof(summary), "%u available", room_.group_total[3]);
    ui_card_set_content(groups_[3].card, GROUP_ICONS[3], 0xD8FF26, "Scenes", summary);
    for (int i = 0; i < 4; ++i) ui_theme::interactive(groups_[i].card.root, false, room_.group_total[i] > 0);

    display(status_values_[0], room_.temperature);
    display(status_values_[1], room_.humidity);
    snprintf(summary, sizeof(summary), "%u", room_.devices_online);
    display(status_values_[2], summary);
    display(status_values_[3], room_.system_status);
    lv_obj_set_style_text_color(status_values_[3], lv_color_hex(room_.healthy ? ui_theme::SUCCESS : ui_theme::WARN), LV_PART_MAIN);
    if (group_ >= 0) render_popup();
}

void RoomModule::render_popup() {
    size_t matching[PANEL_MAX_ROOM_CONTROLS];
    size_t total = 0;
    for (size_t i = 0; i < control_count_; ++i)
        if (group_for(controls_[i].kind) == group_) matching[total++] = i;
    const int pages = total ? static_cast<int>((total + 5) / 6) : 1;
    if (page_ >= pages) page_ = pages - 1;
    display(popup_title_, GROUP_NAMES[group_]);
    for (int i = 0; i < 6; ++i) {
        const size_t item = static_cast<size_t>(page_ * 6 + i);
        bind(popup_cards_[i], item < total ? &controls_[matching[item]] : nullptr);
    }
    char page_text[48];
    if (total) snprintf(page_text, sizeof(page_text), "%d / %d", page_ + 1, pages);
    else snprintf(page_text, sizeof(page_text), "No controls in this group");
    display(popup_feedback_, page_text);
    set_enabled(previous_, page_ > 0);
    set_enabled(next_, page_ + 1 < pages);
}

void RoomModule::action_cb(lv_event_t *event) {
    BoundCard *slot = static_cast<BoundCard *>(lv_event_get_user_data(event));
    if (!slot || !slot->owner || slot->dragging || !slot->control.entity_id[0]) return;
    const bool queued = ui_state_model_activate(slot->control);
    ui_shell_report_status(queued ? "Command queued for Home Assistant" : "Command could not be queued");
    slot->owner->update();
}

void RoomModule::slider_cb(lv_event_t *event) {
    BoundCard *slot = static_cast<BoundCard *>(lv_event_get_user_data(event));
    if (!slot || !slot->owner || !slot->card.slider) return;
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) slot->dragging = true;
    else if (code == LV_EVENT_VALUE_CHANGED && slot->dragging)
        ui_card_set_level(slot->card, static_cast<uint8_t>(lv_slider_get_value(slot->card.slider)), false);
    else if (code == LV_EVENT_PRESS_LOST) {
        slot->dragging = false;
        slot->owner->update();
    } else if (code == LV_EVENT_RELEASED && slot->dragging) {
        const uint8_t level = static_cast<uint8_t>(lv_slider_get_value(slot->card.slider));
        slot->dragging = false;
        const bool queued = ui_state_model_set_level(slot->control, level);
        ui_shell_report_status(queued ? "Level change queued for Home Assistant" : "Level change could not be queued");
        slot->owner->update();
    }
}

void RoomModule::group_cb(lv_event_t *event) {
    GroupCard *group = static_cast<GroupCard *>(lv_event_get_user_data(event));
    if (!group || !group->owner) return;
    group->owner->group_ = group->index;
    group->owner->page_ = 0;
    group->owner->render_popup();
    lv_obj_remove_flag(group->owner->overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(group->owner->overlay_);
}

void RoomModule::close_cb(lv_event_t *event) {
    RoomModule *self = static_cast<RoomModule *>(lv_event_get_user_data(event));
    if (self) self->on_deactivate();
}

void RoomModule::page_cb(lv_event_t *event) {
    RoomModule *self = static_cast<RoomModule *>(lv_event_get_user_data(event));
    if (!self) return;
    for (auto &slot : self->popup_cards_) slot.dragging = false;
    self->page_ += lv_event_get_target(event) == self->next_ ? 1 : -1;
    if (self->page_ < 0) self->page_ = 0;
    self->render_popup();
}

void RoomModule::room_changed_cb(lv_event_t *event) {
    RoomModule *self = static_cast<RoomModule *>(lv_event_get_user_data(event));
    if (!self || !self->room_selector_) return;
    self->on_deactivate();
    ui_state_model_set_active_room(static_cast<uint8_t>(lv_dropdown_get_selected(self->room_selector_)));
    self->update();
}

void RoomModule::on_deactivate() {
    group_ = -1;
    page_ = 0;
    for (auto &slot : favorites_) slot.dragging = false;
    for (auto &slot : popup_cards_) slot.dragging = false;
    if (overlay_) lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
}
