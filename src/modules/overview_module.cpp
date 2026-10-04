#include "overview_module.h"

#include "display_text.h"
#include "ha_icons_font.h"
#include "module_ui.h"
#include "ui_theme.h"

#include <stdio.h>
#include <string.h>

using namespace module_ui;

namespace {
uint32_t glyph_for(const OverviewCardViewModel &model) {
    if (strcmp(model.type, "home_status") == 0)
        return ui_theme::status_glyph("shield", model.entity_id, model.title, model.active);
    if (strcmp(model.type, "lights") == 0 || strcmp(model.type, "all_lights") == 0)
        return ui_theme::status_glyph("light", model.entity_id, model.title, model.active);
    if (strcmp(model.type, "network") == 0)
        return ui_theme::status_glyph("power", model.entity_id, model.title, model.active);
    return ui_theme::status_glyph(model.icon, model.entity_id, model.title, model.active);
}

uint32_t active_color(const char *color) {
    return ui_theme::status_color(color);
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
    module_ui::title(parent, id(), "Overview", "Whole-home status, controls, and quick actions");
    feedback_ = label(parent, "Live status from Home Assistant", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(feedback_, 700, 54);
    lv_obj_set_width(feedback_, 556);
    lv_obj_set_style_text_align(feedback_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
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
                       24 + column * 307, PAGE_CONTENT_TOP + row * 132, span * 307 - 12, 120);
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
    if (!card.model.actionable) return;
    if (!card.model.confirm) { execute_action(card); return; }
    pending_ = &card;
    char title[96];
    snprintf(title, sizeof(title), "%s?", card.model.title);
    safe_text(confirm_title_, title);
    char detail[192];
    if (card.model.action_entity_id[0] && strcmp(card.model.action_entity_id, card.model.entity_id) != 0) {
        snprintf(detail, sizeof(detail), "Current status: %.48s\nAction target: %.80s\n\nContinue?",
                 card.model.state_text, card.model.action_entity_id);
    } else {
        snprintf(detail, sizeof(detail), "Current status: %s\n\nDo you want to continue with this Home Assistant action?", card.model.state_text);
    }
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
