#include "calendar_module.h"

#include "config_service.h"
#include "display_text.h"
#include "module_ui.h"
#include "ui_theme.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

namespace {
constexpr uint32_t BG = ui_theme::BG, SURFACE = ui_theme::SURFACE;
constexpr uint32_t TEXT = ui_theme::TEXT, MUTED = ui_theme::MUTED;
constexpr uint32_t BORDER = ui_theme::BORDER, ACCENT = ui_theme::ACCENT;

void style_box(lv_obj_t *object, uint32_t color, int radius = 14, int border = 1) {
    lv_obj_set_style_bg_color(object, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(object, radius, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, border, LV_PART_MAIN);
    if (border) lv_obj_set_style_border_color(object, lv_color_hex(BORDER), LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color) {
    lv_obj_t *object = lv_label_create(parent);
    lv_label_set_text(object, text ? text : "");
    lv_obj_set_style_text_font(object, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(object, lv_color_hex(color), LV_PART_MAIN);
    return object;
}

void safe_text(lv_obj_t *object, const char *value) {
    char display[640];
    panel_display_text(display, sizeof(display), value ? value : "");
    lv_label_set_text(object, display);
}

uint32_t named_color(const char *color) {
    if (!color) return ui_theme::CYAN;
    if (strcmp(color, "green") == 0) return ui_theme::SUCCESS;
    if (strcmp(color, "yellow") == 0) return ui_theme::WARN;
    if (strcmp(color, "red") == 0) return ui_theme::DANGER;
    if (strcmp(color, "purple") == 0) return 0xA78BFA;
    if (strcmp(color, "blue") == 0) return 0x60A5FA;
    return ui_theme::CYAN;
}
}

void CalendarModule::select_today() {
    period_offset_ = 0;
    const time_t now = time(nullptr);
    const struct tm *local = localtime(&now);
    if (!local) { selected_day_ = 0; return; }
    const PanelConfig &config = config_service_get();
    const uint8_t day_count = config.calendar_days == 1 || config.calendar_days == 3 ? config.calendar_days : 7;
    const bool monday = config.calendar_week_starts_monday;
    selected_day_ = day_count == 7 ?
        static_cast<uint8_t>((local->tm_wday - (monday ? 1 : 0) + 7) % 7) : 0;
}

void CalendarModule::create(lv_obj_t *parent) {
    parent_ = parent;
    lv_obj_set_style_bg_color(parent, lv_color_hex(BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(parent, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(parent, 0, LV_PART_MAIN);
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    module_ui::title(parent, "Calendar", "Your selected calendars and schedule at a glance");
    week_label_ = label(parent, "Loading week...", &lv_font_montserrat_18, TEXT);
    lv_obj_set_pos(week_label_, 24, 94); lv_obj_set_width(week_label_, 580);
    status_ = label(parent, "", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(status_, 610, 98); lv_obj_set_width(status_, 300);
    lv_obj_set_style_text_align(status_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_long_mode(status_, LV_LABEL_LONG_DOT);

    auto nav_button = [&](const char *text, int x, int width) {
        lv_obj_t *button = lv_obj_create(parent);
        lv_obj_set_pos(button, x, module_ui::PAGE_CONTENT_TOP); lv_obj_set_size(button, width, 48);
        style_box(button, SURFACE, 14, 1);
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *caption = label(button, text, &lv_font_montserrat_16, TEXT);
        lv_obj_center(caption);
        lv_obj_add_event_cb(button, nav_cb, LV_EVENT_CLICKED, this);
        return button;
    };
    previous_ = nav_button("<", 960, 58);
    today_ = nav_button("Today", 1028, 112);
    next_ = nav_button(">", 1150, 58);

    const int day_gap = 8, day_x = 24, day_y = module_ui::PAGE_CONTENT_TOP + 60;
    const int day_width = (1232 - day_gap * 6) / 7;
    for (uint8_t i = 0; i < 7; ++i) {
        DaySlot &slot = days_[i];
        slot.root = lv_obj_create(parent);
        lv_obj_set_pos(slot.root, day_x + i * (day_width + day_gap), day_y);
        lv_obj_set_size(slot.root, day_width, 92);
        style_box(slot.root, SURFACE, 16, 1);
        lv_obj_add_flag(slot.root, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(slot.root, day_cb, LV_EVENT_CLICKED, this);
        slot.weekday = label(slot.root, "---", &lv_font_montserrat_12, MUTED);
        lv_obj_set_pos(slot.weekday, 0, 8); lv_obj_set_width(slot.weekday, day_width);
        lv_obj_set_style_text_align(slot.weekday, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        slot.date = label(slot.root, "--", &lv_font_montserrat_24, TEXT);
        lv_obj_set_pos(slot.date, 0, 29); lv_obj_set_width(slot.date, day_width);
        lv_obj_set_style_text_align(slot.date, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        slot.count = label(slot.root, "", &lv_font_montserrat_12, MUTED);
        lv_obj_set_pos(slot.count, 0, 66); lv_obj_set_width(slot.count, day_width);
        lv_obj_set_style_text_align(slot.count, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }

    lv_obj_t *agenda_card = lv_obj_create(parent);
    lv_obj_set_pos(agenda_card, 24, 244); lv_obj_set_size(agenda_card, 1232, 358);
    style_box(agenda_card, SURFACE, 18, 1);
    lv_obj_remove_flag(agenda_card, LV_OBJ_FLAG_SCROLLABLE);
    selected_label_ = label(agenda_card, "Today", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(selected_label_, 20, 15); lv_obj_set_width(selected_label_, 540);
    empty_ = label(agenda_card, "No events", &lv_font_montserrat_18, MUTED);
    lv_obj_set_pos(empty_, 20, 82); lv_obj_set_width(empty_, 1192);
    lv_obj_set_style_text_align(empty_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    agenda_ = lv_obj_create(agenda_card);
    lv_obj_set_pos(agenda_, 12, 54); lv_obj_set_size(agenda_, 1208, 292);
    style_box(agenda_, SURFACE, 0, 0);
    lv_obj_set_style_pad_all(agenda_, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_row(agenda_, 8, LV_PART_MAIN);
    lv_obj_set_flex_flow(agenda_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(agenda_, LV_DIR_VER);
    for (uint8_t i = 0; i < 8; ++i) {
        EventSlot &slot = events_[i];
        slot.root = lv_obj_create(agenda_);
        lv_obj_set_size(slot.root, LV_PCT(100), 82);
        lv_obj_set_flex_grow(slot.root, 0);
        style_box(slot.root, 0x0D2138, 14, 1);
        lv_obj_add_flag(slot.root, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(slot.root, event_cb, LV_EVENT_CLICKED, this);
        slot.accent = lv_obj_create(slot.root);
        lv_obj_set_pos(slot.accent, 0, 0); lv_obj_set_size(slot.accent, 6, 82);
        style_box(slot.accent, ui_theme::CYAN, 3, 0);
        slot.time = label(slot.root, "", &lv_font_montserrat_14, MUTED);
        lv_obj_set_pos(slot.time, 22, 15); lv_obj_set_width(slot.time, 180);
        slot.title = label(slot.root, "", &lv_font_montserrat_18, TEXT);
        lv_obj_set_pos(slot.title, 210, 12); lv_obj_set_width(slot.title, 500);
        lv_label_set_long_mode(slot.title, LV_LABEL_LONG_DOT);
        slot.calendar = label(slot.root, "", &lv_font_montserrat_12, MUTED);
        lv_obj_set_pos(slot.calendar, 210, 46); lv_obj_set_width(slot.calendar, 300);
        slot.location = label(slot.root, "", &lv_font_montserrat_12, MUTED);
        lv_obj_set_pos(slot.location, 735, 30); lv_obj_set_width(slot.location, 420);
        lv_obj_set_style_text_align(slot.location, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        lv_label_set_long_mode(slot.location, LV_LABEL_LONG_DOT);
        lv_obj_add_flag(slot.root, LV_OBJ_FLAG_HIDDEN);
    }

    detail_overlay_ = lv_obj_create(parent);
    lv_obj_set_pos(detail_overlay_, 0, 0); lv_obj_set_size(detail_overlay_, 1280, 626);
    style_box(detail_overlay_, 0x030712, 0, 0);
    lv_obj_set_style_bg_opa(detail_overlay_, LV_OPA_80, LV_PART_MAIN);
    lv_obj_add_flag(detail_overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(detail_overlay_, close_detail_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *dialog = lv_obj_create(detail_overlay_);
    lv_obj_set_size(dialog, 1100, 540); lv_obj_center(dialog);
    style_box(dialog, SURFACE, 22, 1);
    lv_obj_remove_flag(dialog, LV_OBJ_FLAG_CLICKABLE);
    auto *eyebrow = label(dialog, "EVENT DETAILS", &lv_font_montserrat_12, ui_theme::CYAN);
    lv_obj_set_pos(eyebrow, 30, 24);
    detail_title_ = label(dialog, "", &lv_font_montserrat_28, TEXT);
    lv_obj_set_pos(detail_title_, 30, 52); lv_obj_set_width(detail_title_, 1040);
    lv_label_set_long_mode(detail_title_, LV_LABEL_LONG_WRAP);
    detail_meta_ = label(dialog, "", &lv_font_montserrat_16, MUTED);
    lv_obj_set_pos(detail_meta_, 30, 118); lv_obj_set_width(detail_meta_, 1040);
    detail_body_ = lv_obj_create(dialog);
    lv_obj_set_pos(detail_body_, 30, 158); lv_obj_set_size(detail_body_, 1040, 322);
    style_box(detail_body_, 0x0D2138, 14, 1);
    lv_obj_set_style_pad_all(detail_body_, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_row(detail_body_, 16, LV_PART_MAIN);
    lv_obj_set_flex_flow(detail_body_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(detail_body_, LV_DIR_VER);
    detail_location_ = label(detail_body_, "", &lv_font_montserrat_16, TEXT);
    lv_obj_set_width(detail_location_, LV_PCT(100));
    lv_label_set_long_mode(detail_location_, LV_LABEL_LONG_WRAP);
    detail_description_ = label(detail_body_, "", &lv_font_montserrat_16, TEXT);
    lv_obj_set_width(detail_description_, LV_PCT(100));
    lv_label_set_long_mode(detail_description_, LV_LABEL_LONG_WRAP);
    auto *close = label(dialog, "Tap outside to close", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(close, 30, 505);
    lv_obj_add_flag(detail_overlay_, LV_OBJ_FLAG_HIDDEN);

    select_today();
    initialized_ = true;
    update();
}

void CalendarModule::on_activate() {
    if (!initialized_) return;
    update();
}

void CalendarModule::update() {
    if (!parent_) return;
    ui_state_model_snapshot_calendar(period_offset_, selected_day_, model_);
    safe_text(week_label_, model_.week_label);
    safe_text(status_, model_.status);
    safe_text(selected_label_, model_.selected_day_label);
    const uint8_t day_count = model_.day_count == 1 || model_.day_count == 3 ? model_.day_count : 7;
    const int day_gap = day_count == 7 ? 8 : 12;
    const int strip_width = day_count == 1 ? 420 : 1232;
    const int day_width = (strip_width - day_gap * (day_count - 1)) / day_count;
    const int day_x = 24 + (1232 - strip_width) / 2;
    for (uint8_t i = 0; i < 7; ++i) {
        DaySlot &slot = days_[i];
        if (i >= day_count) {
            lv_obj_add_flag(slot.root, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(slot.root, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(slot.root, day_x + i * (day_width + day_gap), module_ui::PAGE_CONTENT_TOP + 60);
        lv_obj_set_width(slot.root, day_width);
        lv_obj_set_width(slot.weekday, day_width);
        lv_obj_set_width(slot.date, day_width);
        lv_obj_set_width(slot.count, day_width);
        safe_text(slot.weekday, model_.days[i].weekday);
        safe_text(slot.date, model_.days[i].date);
        char count[24] = {};
        if (model_.days[i].event_count)
            snprintf(count, sizeof(count), "%u event%s", static_cast<unsigned>(model_.days[i].event_count),
                     model_.days[i].event_count == 1 ? "" : "s");
        safe_text(slot.count, count);
        const bool selected = model_.days[i].selected;
        lv_obj_set_style_bg_color(slot.root, lv_color_hex(selected ? ACCENT : SURFACE), LV_PART_MAIN);
        lv_obj_set_style_border_color(slot.root,
            lv_color_hex(model_.days[i].today ? ui_theme::CYAN : BORDER), LV_PART_MAIN);
        lv_obj_set_style_border_width(slot.root, model_.days[i].today ? 2 : 1, LV_PART_MAIN);
        lv_obj_set_style_text_color(slot.weekday,
            lv_color_hex(selected ? 0xBFF7FF : MUTED), LV_PART_MAIN);
    }
    const bool has_events = model_.event_count > 0;
    if (has_events) {
        lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(agenda_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(empty_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(agenda_, LV_OBJ_FLAG_HIDDEN);
        safe_text(empty_, model_.status);
    }
    for (uint8_t i = 0; i < 8; ++i) {
        EventSlot &slot = events_[i];
        if (i >= model_.event_count) { lv_obj_add_flag(slot.root, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(slot.root, LV_OBJ_FLAG_HIDDEN);
        const CalendarEventViewModel &event = model_.events[i];
        safe_text(slot.time, event.time);
        safe_text(slot.title, event.title);
        safe_text(slot.calendar, event.calendar);
        char location[96] = {};
        if (event.location[0]) snprintf(location, sizeof(location), "@ %s", event.location);
        safe_text(slot.location, location);
        lv_obj_set_style_bg_color(slot.accent, lv_color_hex(named_color(event.color)), LV_PART_MAIN);
    }
}

void CalendarModule::show_event(uint8_t index) {
    if (index >= model_.event_count) return;
    const CalendarEventViewModel &event = model_.events[index];
    safe_text(detail_title_, event.title);
    char meta[176];
    snprintf(meta, sizeof(meta), "%s  |  %s", event.calendar, event.date_range);
    safe_text(detail_meta_, meta);
    char location[160];
    snprintf(location, sizeof(location), "LOCATION\n%s",
             event.location[0] ? event.location : "No location provided");
    safe_text(detail_location_, location);
    char description[544];
    snprintf(description, sizeof(description), "DETAILS\n%s",
             event.description[0] ? event.description : "No additional details provided.");
    safe_text(detail_description_, description);
    if (detail_body_) lv_obj_scroll_to_y(detail_body_, 0, LV_ANIM_OFF);
    lv_obj_remove_flag(detail_overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(detail_overlay_);
}

void CalendarModule::day_cb(lv_event_t *event) {
    auto *self = static_cast<CalendarModule *>(lv_event_get_user_data(event));
    if (!self) return;
    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(event));
    for (uint8_t i = 0; i < 7; ++i)
        if (self->days_[i].root == target) { self->selected_day_ = i; self->update(); return; }
}

void CalendarModule::nav_cb(lv_event_t *event) {
    auto *self = static_cast<CalendarModule *>(lv_event_get_user_data(event));
    if (!self) return;
    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(event));
    if (target == self->previous_) --self->period_offset_;
    else if (target == self->next_) ++self->period_offset_;
    else if (target == self->today_) self->select_today();
    self->update();
}

void CalendarModule::event_cb(lv_event_t *event) {
    auto *self = static_cast<CalendarModule *>(lv_event_get_user_data(event));
    if (!self) return;
    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(event));
    for (uint8_t i = 0; i < 8; ++i)
        if (self->events_[i].root == target) { self->show_event(i); return; }
}

void CalendarModule::close_detail_cb(lv_event_t *event) {
    auto *self = static_cast<CalendarModule *>(lv_event_get_user_data(event));
    if (self && self->detail_overlay_) lv_obj_add_flag(self->detail_overlay_, LV_OBJ_FLAG_HIDDEN);
}
