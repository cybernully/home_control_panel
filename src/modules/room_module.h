#pragma once
#include "module.h"
class RoomModule final : public PanelModule {
public:
    const char *id() const override { return "room"; }
    const char *title() const override { return "Room"; }
    void create(lv_obj_t *parent) override;
    void update() override {}
private:
    struct ToggleControl { lv_obj_t *button=nullptr; lv_obj_t *label=nullptr; bool on=false; };
    ToggleControl controls_[4];
    lv_obj_t *brightness_label_ = nullptr;
    lv_obj_t *scene_status_ = nullptr;
    static void toggle_cb(lv_event_t *e);
    static void brightness_cb(lv_event_t *e);
    static void scene_cb(lv_event_t *e);
};
