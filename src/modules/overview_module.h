#pragma once
#include "module.h"
class OverviewModule final : public PanelModule {
public:
    const char *id() const override { return "overview"; }
    const char *title() const override { return "Overview"; }
    void create(lv_obj_t *parent) override;
    void update() override;
private:
    lv_obj_t *area_value_ = nullptr;
    lv_obj_t *ha_value_ = nullptr;
    lv_obj_t *action_status_ = nullptr;
    static void action_cb(lv_event_t *e);
};
