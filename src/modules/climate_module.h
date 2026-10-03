#pragma once

#include "module.h"
#include "ui_state_model.h"

class ClimateModule final : public PanelModule {
public:
    const char *id() const override { return "climate"; }
    const char *title() const override { return "Climate"; }
    void create(lv_obj_t *parent) override;
    void update() override;

private:
    ClimateViewModel view_ = {};
    uint8_t selected_index_ = 0;
    lv_obj_t *tabs_[PANEL_MAX_CLIMATE_DEVICES] = {};
    lv_obj_t *thermostat_card_ = nullptr;
    lv_obj_t *details_card_ = nullptr;
    lv_obj_t *controls_card_ = nullptr;
    lv_obj_t *empty_card_ = nullptr;
    lv_obj_t *name_label_ = nullptr;
    lv_obj_t *connection_label_ = nullptr;
    lv_obj_t *current_label_ = nullptr;
    lv_obj_t *target_label_ = nullptr;
    lv_obj_t *target_detail_ = nullptr;
    lv_obj_t *humidity_label_ = nullptr;
    lv_obj_t *action_label_ = nullptr;
    lv_obj_t *feedback_label_ = nullptr;
    lv_obj_t *minus_button_ = nullptr;
    lv_obj_t *plus_button_ = nullptr;
    lv_obj_t *mode_buttons_[HA_MAX_CLIMATE_MODES] = {};
    lv_obj_t *fan_button_ = nullptr;
    lv_obj_t *preset_button_ = nullptr;

    static void tab_cb(lv_event_t *event);
    static void adjust_cb(lv_event_t *event);
    static void mode_cb(lv_event_t *event);
    static void fan_cb(lv_event_t *event);
    static void preset_cb(lv_event_t *event);
};
