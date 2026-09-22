#pragma once
#include "module.h"
class ClimateModule final : public PanelModule {
public:
    const char *id() const override { return "climate"; }
    const char *title() const override { return "Climate"; }
    void create(lv_obj_t *parent) override;
    void update() override {}
private:
    int setpoint_ = 72;
    lv_obj_t *setpoint_label_ = nullptr;
    lv_obj_t *mode_label_ = nullptr;
    static void adjust_cb(lv_event_t *e);
    static void mode_cb(lv_event_t *e);
};
