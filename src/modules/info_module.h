#pragma once
#include "module.h"
class InfoModule final : public PanelModule {
public:
    InfoModule(const char *id, const char *title, const char *description);
    const char *id() const override;
    const char *title() const override;
    void create(lv_obj_t *parent) override;
    void update() override;
private:
    const char *id_;
    const char *title_;
    const char *description_;
    lv_obj_t *status_ = nullptr;
};
