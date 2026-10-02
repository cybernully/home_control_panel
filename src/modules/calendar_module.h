#pragma once

#include "module.h"
#include "ui_state_model.h"

class CalendarModule final : public PanelModule {
public:
    const char *id() const override { return "calendar"; }
    const char *title() const override { return "Calendar"; }
    void create(lv_obj_t *parent) override;
    void update() override;
    void on_activate() override;

private:
    struct DaySlot {
        lv_obj_t *root = nullptr;
        lv_obj_t *weekday = nullptr;
        lv_obj_t *date = nullptr;
        lv_obj_t *count = nullptr;
    };
    struct EventSlot {
        lv_obj_t *root = nullptr;
        lv_obj_t *accent = nullptr;
        lv_obj_t *time = nullptr;
        lv_obj_t *title = nullptr;
        lv_obj_t *calendar = nullptr;
        lv_obj_t *location = nullptr;
    };

    lv_obj_t *parent_ = nullptr;
    lv_obj_t *week_label_ = nullptr;
    lv_obj_t *status_ = nullptr;
    lv_obj_t *previous_ = nullptr;
    lv_obj_t *today_ = nullptr;
    lv_obj_t *next_ = nullptr;
    lv_obj_t *selected_label_ = nullptr;
    lv_obj_t *agenda_ = nullptr;
    lv_obj_t *empty_ = nullptr;
    DaySlot days_[7] = {};
    EventSlot events_[8] = {};
    lv_obj_t *detail_overlay_ = nullptr;
    lv_obj_t *detail_title_ = nullptr;
    lv_obj_t *detail_meta_ = nullptr;
    lv_obj_t *detail_location_ = nullptr;
    lv_obj_t *detail_body_ = nullptr;
    lv_obj_t *detail_description_ = nullptr;
    CalendarViewModel model_ = {};
    int16_t period_offset_ = 0;
    uint8_t selected_day_ = 0;
    bool initialized_ = false;

    void select_today();
    void show_event(uint8_t index);
    static void day_cb(lv_event_t *event);
    static void nav_cb(lv_event_t *event);
    static void event_cb(lv_event_t *event);
    static void close_detail_cb(lv_event_t *event);
};
