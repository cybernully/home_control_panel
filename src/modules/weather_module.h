#pragma once

#include "module.h"
#include "ui_state_model.h"

class WeatherModule final : public PanelModule {
public:
    const char *id() const override { return "weather"; }
    const char *title() const override { return "Weather"; }
    void create(lv_obj_t *parent) override;
    void update() override;
    struct ForecastSlot {
        lv_obj_t *root = nullptr;
        lv_obj_t *period = nullptr;
        lv_obj_t *icon = nullptr;
        lv_obj_t *temperature = nullptr;
        lv_obj_t *condition = nullptr;
        lv_obj_t *detail = nullptr;
    };

private:

    lv_obj_t *subtitle_ = nullptr;
    lv_obj_t *current_card_ = nullptr;
    lv_obj_t *current_icon_ = nullptr;
    lv_obj_t *current_temperature_ = nullptr;
    lv_obj_t *current_condition_ = nullptr;
    lv_obj_t *current_detail_ = nullptr;
    lv_obj_t *hourly_card_ = nullptr;
    lv_obj_t *hourly_status_ = nullptr;
    lv_obj_t *daily_card_ = nullptr;
    lv_obj_t *daily_status_ = nullptr;
    ForecastSlot hourly_[HA_MAX_WEATHER_HOURLY] = {};
    ForecastSlot daily_[HA_MAX_WEATHER_DAILY] = {};
    char applied_layout_[20] = {};
    uint8_t applied_sections_ = 0;

    void apply_layout();
    void layout_hourly(int width, int height);
    void layout_daily(int width, int height);
};
