#pragma once
#include "module.h"
class SecurityModule final : public PanelModule {
public:
    const char *id() const override { return "security"; }
    const char *title() const override { return "Security"; }
    void create(lv_obj_t *parent) override;
    void update() override {}
private:
    lv_obj_t *state_label_ = nullptr;
    static void arm_cb(lv_event_t *e);
};
