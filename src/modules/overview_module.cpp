#include "overview_module.h"

#include "display_text.h"
#include "ha_icons_font.h"
#include "module_ui.h"
#include "ui_theme.h"

#include <stdio.h>
#include <string.h>

using namespace module_ui;

namespace {
bool contains_ci(const char *text, const char *needle) {
    if (!text || !needle || !needle[0]) return false;
    for (const char *start = text; *start; ++start) {
        const char *a = start, *b = needle;
        while (*a && *b) {
            char left = *a, right = *b;
            if (left >= 'A' && left <= 'Z') left = static_cast<char>(left - 'A' + 'a');
            if (right >= 'A' && right <= 'Z') right = static_cast<char>(right - 'A' + 'a');
            if (left != right) break;
            ++a; ++b;
        }
        if (!*b) return true;
    }
    return false;
}

const char *effective_icon(const OverviewCardViewModel &model) {
    if (model.icon[0] && strcmp(model.icon, "auto") != 0) return model.icon;
    if (strncmp(model.entity_id, "light.", 6) == 0) return "light";
    if (strncmp(model.entity_id, "fan.", 4) == 0) return "fan";
    if (strncmp(model.entity_id, "cover.", 6) == 0)
        return contains_ci(model.entity_id, "garage") || contains_ci(model.title, "garage") ? "garage" : "cover";
    if (strncmp(model.entity_id, "lock.", 5) == 0) return "lock";
    if (strncmp(model.entity_id, "binary_sensor.", 14) == 0)
        return contains_ci(model.title, "motion") ? "motion" : "door";
    if (strncmp(model.entity_id, "weather.", 8) == 0) return "weather";
    if (strncmp(model.entity_id, "sensor.", 7) == 0 && contains_ci(model.title, "humidity")) return "humidity";
    if (strncmp(model.entity_id, "sensor.", 7) == 0 && contains_ci(model.title, "temp")) return "temperature";
    if (strcmp(model.type, "home_status") == 0) return "shield";
    if (strcmp(model.type, "lights") == 0 || strcmp(model.type, "all_lights") == 0) return "light";
    if (strcmp(model.type, "network") == 0) return "power";
    return "alert";
}

uint32_t glyph_for(const OverviewCardViewModel &model) {
    const char *icon = effective_icon(model);
    if (strcmp(icon, "garage") == 0) return model.active ? 0xF06DA : 0xF06D9;
    if (strcmp(icon, "door") == 0) return model.active ? 0xF081C : 0xF081B;
    if (strcmp(icon, "lock") == 0) return model.active ? 0xF033F : 0xF033E;
    if (strcmp(icon, "motion") == 0) return 0xF0D91;
    if (strcmp(icon, "light") == 0) return model.active ? 0xF0335 : 0xF0336;
    if (strcmp(icon, "fan") == 0) return model.active ? 0xF0210 : 0xF081D;
    if (strcmp(icon, "cover") == 0) return model.active ? 0xF1011 : 0xF00AC;
    if (strcmp(icon, "window") == 0) return model.active ? 0xF05B1 : 0xF05AE;
    if (strcmp(icon, "camera") == 0) return 0xF0100;
    if (strcmp(icon, "shield") == 0) return 0xF068A;
    if (strcmp(icon, "temperature") == 0) return 0xF050F;
    if (strcmp(icon, "humidity") == 0) return 0xF058E;
    if (strcmp(icon, "power") == 0) return model.active ? 0xF0425 : 0xF0902;
    if (strcmp(icon, "weather") == 0) return 0xF0595;
    return model.active ? 0xF05E0 : 0xF0028;
}

uint32_t active_color(const char *color) {
    if (strcmp(color, "green") == 0) return ui_theme::SUCCESS;
    if (strcmp(color, "yellow") == 0) return ui_theme::YELLOW;
    if (strcmp(color, "red") == 0) return ui_theme::DANGER;
    if (strcmp(color, "purple") == 0) return 0xA78BFA;
    if (strcmp(color, "blue") == 0) return 0x70A5FF;
    return ui_theme::CYAN;
}

void safe_text(lv_obj_t *target, const char *value) {
    if (!target) return;
    char safe[192];
    panel_display_text(safe, sizeof(safe), value ? value : "");
    lv_label_set_text(target, safe);
}
}

void OverviewModule::create(lv_obj_t *parent) {
    box(parent, BG, 0, 0);
    lv_obj_t *heading = label(parent, "At a glance", &lv_font_montserrat_28, TEXT);
    lv_obj_set_pos(heading, 24, 13);
    feedback_ = label(parent, "Status and controls update live from Home Assistant", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(feedback_, 24, 48);
    lv_obj_set_width(feedback_, 1000);
    lv_label_set_long_mode(feedback_, LV_LABEL_LONG_DOT);

    card_count_ = ui_state_model_snapshot_overview(view_, PANEL_MAX_OVERVIEW_ITEMS);
    uint8_t row = 0, column = 0;
    for (size_t i = 0; i < card_count_; ++i) {
        const uint8_t span = view_[i].span == 1 || view_[i].span == 2 || view_[i].span == 4 ? view_[i].span : 1;
        if (column + span > 4) { ++row; column = 0; }
        if (row >= 4) { card_count_ = i; break; }
        BoundCard &slot = cards_[i];
        slot.owner = this;
        ui_card_create(slot.card, parent, UiCardVariant::STATUS,
                       24 + column * 307, 76 + row * 132, span * 307 - 12, 120);
        lv_obj_add_event_cb(slot.card.root, card_cb, LV_EVENT_CLICKED, &slot);
        bind(slot, view_[i]);
        column = static_cast<uint8_t>(column + span);
        if (column == 4) { ++row; column = 0; }
    }

    overlay_ = lv_obj_create(parent);
    lv_obj_set_pos(overlay_, 0, 0);
    lv_obj_set_size(overlay_, 1280, lv_pct(100));
    box(overlay_, 0x020A14, 0, 0);
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_80, LV_PART_MAIN);
    lv_obj_add_event_cb(overlay_, cancel_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *dialog = card(overlay_, 330, 146, 620, 300);
    lv_obj_set_style_radius(dialog, 22, LV_PART_MAIN);
    lv_obj_remove_flag(dialog, LV_OBJ_FLAG_EVENT_BUBBLE);
    confirm_title_ = label(dialog, "Confirm action", &lv_font_montserrat_28, TEXT);
    lv_obj_set_pos(confirm_title_, 28, 28);
    lv_obj_set_width(confirm_title_, 564);
    lv_label_set_long_mode(confirm_title_, LV_LABEL_LONG_DOT);
    confirm_detail_ = label(dialog, "", &lv_font_montserrat_16, MUTED);
    lv_obj_set_pos(confirm_detail_, 28, 88);
    lv_obj_set_width(confirm_detail_, 564);
    lv_label_set_long_mode(confirm_detail_, LV_LABEL_LONG_WRAP);
    lv_obj_t *cancel = button(dialog, "Cancel", 28, 220, 260, 56, CARD_ALT);
    lv_obj_t *confirm = button(dialog, "Confirm", 332, 220, 260, 56, ACCENT);
    lv_obj_add_event_cb(cancel, cancel_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(confirm, confirm_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
}

void OverviewModule::bind(BoundCard &slot, const OverviewCardViewModel &model) {
    slot.model = model;
    const uint32_t color = model.available && model.active ? active_color(model.color) : MUTED;
    ui_card_set_content(slot.card, glyph_for(model), color, model.title, model.state_text);
    ui_card_set_state(slot.card, model.active, model.available);
    if (!model.actionable) lv_obj_remove_state(slot.card.root, LV_STATE_DISABLED);
}

void OverviewModule::update() {
    OverviewCardViewModel latest[PANEL_MAX_OVERVIEW_ITEMS] = {};
    const size_t count = ui_state_model_snapshot_overview(latest, PANEL_MAX_OVERVIEW_ITEMS);
    const size_t visible = count < card_count_ ? count : card_count_;
    for (size_t i = 0; i < visible; ++i) bind(cards_[i], latest[i]);
}

void OverviewModule::request_action(BoundCard &card) {
    if (!card.model.actionable || !card.model.available) return;
    if (!card.model.confirm) { execute_action(card); return; }
    pending_ = &card;
    char title[96];
    snprintf(title, sizeof(title), "%s?", card.model.title);
    safe_text(confirm_title_, title);
    char detail[160];
    snprintf(detail, sizeof(detail), "Current status: %s\n\nDo you want to continue with this Home Assistant action?", card.model.state_text);
    safe_text(confirm_detail_, detail);
    lv_obj_remove_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(overlay_);
}

void OverviewModule::execute_action(BoundCard &card) {
    const bool queued = ui_state_model_activate_overview(card.model);
    safe_text(feedback_, queued ? "Command queued - waiting for Home Assistant" :
                                "Could not queue command - check Home Assistant connection");
    close_confirmation();
}

void OverviewModule::close_confirmation() {
    pending_ = nullptr;
    if (overlay_) lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
}

void OverviewModule::on_deactivate() { close_confirmation(); }

void OverviewModule::card_cb(lv_event_t *event) {
    auto *card = static_cast<BoundCard *>(lv_event_get_user_data(event));
    if (card && card->owner) card->owner->request_action(*card);
}

void OverviewModule::confirm_cb(lv_event_t *event) {
    auto *owner = static_cast<OverviewModule *>(lv_event_get_user_data(event));
    if (owner && owner->pending_) owner->execute_action(*owner->pending_);
}

void OverviewModule::cancel_cb(lv_event_t *event) {
    auto *owner = static_cast<OverviewModule *>(lv_event_get_user_data(event));
    if (owner) owner->close_confirmation();
}
