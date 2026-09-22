#include "climate_module.h"
#include "module_ui.h"
#include <Arduino.h>
using namespace module_ui;

void ClimateModule::create(lv_obj_t *parent) {
    box(parent, BG, 0, 0);
    module_ui::title(parent, "Climate", "Thermostat and room comfort controls");
    add_preview_badge(parent);

    lv_obj_t *thermo = card(parent, 24, 92, 760, 394);
    lv_obj_t *current = label(thermo, "CURRENT  71 F", &lv_font_montserrat_16, MUTED);
    lv_obj_set_pos(current, 24, 24);
    lv_obj_t *set = label(thermo, "SETPOINT", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(set, 24, 82);
    setpoint_label_ = label(thermo, "72 F", &lv_font_montserrat_28, TEXT);
    lv_obj_set_style_text_font(setpoint_label_, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_pos(setpoint_label_, 314, 78);

    lv_obj_t *minus = button(thermo, "-", 24, 148, 210, 100, CARD_ALT);
    lv_obj_add_event_cb(minus, adjust_cb, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(minus, reinterpret_cast<void *>(static_cast<intptr_t>(-1)));
    lv_obj_t *plus = button(thermo, "+", 502, 148, 210, 100, ACCENT);
    lv_obj_add_event_cb(plus, adjust_cb, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(plus, reinterpret_cast<void *>(static_cast<intptr_t>(1)));
    lv_obj_t *hint = label(thermo, "Touch +/- to test the thermostat UI.", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(hint, 24, 304);

    lv_obj_t *modes = card(parent, 804, 92, 448, 394);
    lv_obj_t *mh = label(modes, "Mode", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(mh, 20, 20);
    const char *names[] = {"HEAT", "COOL", "AUTO", "OFF"};
    for (int i=0;i<4;++i) {
        lv_obj_t *b = button(modes, names[i], 20 + (i%2)*204, 70 + (i/2)*82, 184, 64, i==2?ACCENT:CARD_ALT);
        lv_obj_add_event_cb(b, mode_cb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(b, const_cast<char *>(names[i]));
    }
    mode_label_ = label(modes, "Selected mode: AUTO", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(mode_label_, 20, 260);
    lv_obj_t *status = label(modes, "Indoor humidity 42%", &lv_font_montserrat_16, TEXT);
    lv_obj_set_pos(status, 20, 310);
}

void ClimateModule::adjust_cb(lv_event_t *e) {
    auto *self = static_cast<ClimateModule *>(lv_event_get_user_data(e));
    if (!self || !self->setpoint_label_) return;
    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    intptr_t delta = reinterpret_cast<intptr_t>(lv_obj_get_user_data(target));
    self->setpoint_ += static_cast<int>(delta);
    if (self->setpoint_ < 55) self->setpoint_ = 55;
    if (self->setpoint_ > 85) self->setpoint_ = 85;
    char text[16]; snprintf(text, sizeof(text), "%d F", self->setpoint_);
    lv_label_set_text(self->setpoint_label_, text);
}

void ClimateModule::mode_cb(lv_event_t *e) {
    auto *self = static_cast<ClimateModule *>(lv_event_get_user_data(e));
    if (!self || !self->mode_label_) return;
    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    const char *mode = static_cast<const char *>(lv_obj_get_user_data(target));
    char text[56]; snprintf(text, sizeof(text), "Selected mode: %s", mode ? mode : "AUTO");
    lv_label_set_text(self->mode_label_, text);
}
