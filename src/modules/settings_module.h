#pragma once
#include "module.h"

class SettingsModule final : public PanelModule {
public:
    const char *id() const override { return "settings"; }
    const char *title() const override { return "Settings"; }
    void create(lv_obj_t *parent) override;
    void update() override;

private:
    lv_obj_t *brightness_label_ = nullptr;
    lv_obj_t *ha_label_ = nullptr;
    lv_obj_t *discovery_label_ = nullptr;

    static void backlight_changed_cb(lv_event_t *e);
    static void backlight_released_cb(lv_event_t *e);
    static void rediscover_cb(lv_event_t *e);
};
