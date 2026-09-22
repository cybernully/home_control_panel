#pragma once
#include <lvgl.h>

class PanelModule {
public:
    virtual ~PanelModule() = default;
    virtual const char *id() const = 0;
    virtual const char *title() const = 0;
    virtual void create(lv_obj_t *parent) = 0;
    virtual void update() = 0;
    virtual void on_activate() {}
    virtual void on_deactivate() {}
};
