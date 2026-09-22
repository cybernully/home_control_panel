#pragma once
#include "module.h"
class MediaModule final : public PanelModule {
public:
    const char *id() const override { return "media"; }
    const char *title() const override { return "Media"; }
    void create(lv_obj_t *parent) override;
    void update() override {}
private:
    bool playing_ = false;
    lv_obj_t *play_button_ = nullptr;
    lv_obj_t *play_label_ = nullptr;
    lv_obj_t *volume_label_ = nullptr;
    lv_obj_t *source_label_ = nullptr;
    static void play_cb(lv_event_t *e);
    static void volume_cb(lv_event_t *e);
    static void source_cb(lv_event_t *e);
};
