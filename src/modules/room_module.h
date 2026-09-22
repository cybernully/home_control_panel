#pragma once
#include "app_config.h"
#include "home_assistant.h"
#include "module.h"

class RoomModule final : public PanelModule {
public:
    const char *id() const override { return "room"; }
    const char *title() const override { return "Room"; }
    void create(lv_obj_t *parent) override;
    void update() override;

private:
    struct ControlCard {
        RoomModule *owner = nullptr;
        lv_obj_t *name = nullptr;
        lv_obj_t *detail = nullptr;
        lv_obj_t *button = nullptr;
        lv_obj_t *button_label = nullptr;
        char entity_id[96] = {};
        char domain[16] = {};
        bool bound = false;
    };

    struct SceneControl {
        RoomModule *owner = nullptr;
        lv_obj_t *button = nullptr;
        lv_obj_t *label = nullptr;
        char entity_id[96] = {};
        bool bound = false;
    };

    ControlCard controls_[HA_MAX_ROOM_CONTROLS];
    SceneControl scenes_[HA_MAX_AREA_SCENES];

    lv_obj_t *brightness_slider_ = nullptr;
    lv_obj_t *brightness_label_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    bool brightness_dragging_ = false;

    static void toggle_cb(lv_event_t *e);
    static void brightness_pressed_cb(lv_event_t *e);
    static void brightness_changed_cb(lv_event_t *e);
    static void brightness_released_cb(lv_event_t *e);
    static void scene_cb(lv_event_t *e);
};
