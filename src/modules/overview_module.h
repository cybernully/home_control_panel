#pragma once

#include "app_config.h"
#include "module.h"
#include "ui_card.h"
#include "ui_state_model.h"

class OverviewModule final : public PanelModule {
public:
    const char *id() const override { return "overview"; }
    const char *title() const override { return "Overview"; }
    void create(lv_obj_t *parent) override;
    void update() override;
    void on_deactivate() override;

private:
    struct BoundCard {
        OverviewModule *owner = nullptr;
        UiCard card = {};
        OverviewCardViewModel model = {};
    };

    BoundCard cards_[PANEL_MAX_OVERVIEW_ITEMS] = {};
    OverviewCardViewModel view_[PANEL_MAX_OVERVIEW_ITEMS] = {};
    size_t card_count_ = 0;
    lv_obj_t *feedback_ = nullptr;
    lv_obj_t *overlay_ = nullptr;
    lv_obj_t *confirm_title_ = nullptr;
    lv_obj_t *confirm_detail_ = nullptr;
    BoundCard *pending_ = nullptr;

    void bind(BoundCard &card, const OverviewCardViewModel &model);
    void request_action(BoundCard &card);
    void execute_action(BoundCard &card);
    void close_confirmation();
    static void card_cb(lv_event_t *event);
    static void confirm_cb(lv_event_t *event);
    static void cancel_cb(lv_event_t *event);
};
