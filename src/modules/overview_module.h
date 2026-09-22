#pragma once
#include "app_config.h"
#include "home_assistant.h"
#include "module.h"

class OverviewModule final : public PanelModule {
public:
    const char *id() const override { return "overview"; }
    const char *title() const override { return "Overview"; }
    void create(lv_obj_t *parent) override;
    void update() override;

private:
    struct QuickAction {
        OverviewModule *owner = nullptr;
        lv_obj_t *button = nullptr;
        lv_obj_t *label = nullptr;
        char entity_id[96] = {};
        bool is_all_lights = false;
        bool bound = false;
    };

    lv_obj_t *area_value_ = nullptr;
    lv_obj_t *lights_value_ = nullptr;
    lv_obj_t *climate_value_ = nullptr;
    lv_obj_t *ha_value_ = nullptr;
    lv_obj_t *action_status_ = nullptr;
    QuickAction actions_[4];

    static void action_cb(lv_event_t *e);
};
