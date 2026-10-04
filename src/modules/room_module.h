#pragma once

#include "module.h"
#include "ui_card.h"
#include "ui_state_model.h"

class RoomModule final : public PanelModule {
public:
    const char *id() const override { return "room"; }
    const char *title() const override { return "Rooms"; }
    void create(lv_obj_t *parent) override;
    void update() override;
    void on_deactivate() override;

private:
    struct BoundCard {
        RoomModule *owner = nullptr;
        UiCard card = {};
        RoomControlViewModel control = {};
        bool dragging = false;
        lv_obj_t *parent = nullptr;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };
    struct GroupCard {
        RoomModule *owner = nullptr;
        uint8_t index = 0;
        UiCard card = {};
    };

    RoomViewModel room_ = {};
    RoomControlViewModel controls_[PANEL_MAX_ROOM_CONTROLS] = {};
    size_t control_count_ = 0;
    BoundCard favorites_[4] = {};
    BoundCard popup_cards_[6] = {};
    GroupCard groups_[4] = {};
    lv_obj_t *room_selector_ = nullptr;
    lv_obj_t *status_icons_[4] = {};
    lv_obj_t *status_values_[4] = {};
    lv_obj_t *status_captions_[4] = {};
    lv_obj_t *empty_ = nullptr;
    lv_obj_t *overlay_ = nullptr;
    lv_obj_t *popup_title_ = nullptr;
    lv_obj_t *popup_feedback_ = nullptr;
    lv_obj_t *previous_ = nullptr;
    lv_obj_t *next_ = nullptr;
    int group_ = -1;
    int page_ = 0;

    void bind(BoundCard &slot, const RoomControlViewModel *control);
    void prepare_card(BoundCard &slot, lv_obj_t *parent, UiCardVariant variant,
                      int x, int y, int width, int height);
    void ensure_card_variant(BoundCard &slot, UiCardVariant variant);
    void render_popup();
    static void action_cb(lv_event_t *event);
    static void slider_cb(lv_event_t *event);
    static void fan_speed_cb(lv_event_t *event);
    static void group_cb(lv_event_t *event);
    static void close_cb(lv_event_t *event);
    static void page_cb(lv_event_t *event);
    static void room_changed_cb(lv_event_t *event);
};
