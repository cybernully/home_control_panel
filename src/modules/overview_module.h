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
        char type[16] = {};
        bool bound = false;
    };

    struct Widget {
        char type[PANEL_OVERVIEW_WIDGET_ID_LEN] = {};
        lv_obj_t *value = nullptr;
        lv_obj_t *detail = nullptr;
    };

    lv_obj_t *action_status_ = nullptr;
    Widget widgets_[PANEL_MAX_OVERVIEW_WIDGETS];
    uint8_t widget_count_ = 0;
    QuickAction actions_[PANEL_MAX_OVERVIEW_QUICK_ACTIONS];
    uint8_t action_count_ = 0;

    static void action_cb(lv_event_t *e);
};
