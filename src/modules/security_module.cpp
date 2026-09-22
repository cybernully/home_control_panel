#include "security_module.h"
#include "module_ui.h"
#include <Arduino.h>
#include <cstring>
using namespace module_ui;

void SecurityModule::create(lv_obj_t *parent) {
    box(parent, BG, 0, 0);
    module_ui::title(parent, "Security", "Alarm, locks and active sensor summary");
    add_preview_badge(parent);

    lv_obj_t *alarm = card(parent, 24, 92, 760, 394);
    lv_obj_t *ah = label(alarm, "ALARM", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(ah, 22, 20);
    state_label_ = label(alarm, "DISARMED", &lv_font_montserrat_28, SUCCESS);
    lv_obj_set_pos(state_label_, 22, 54);
    const char *modes[] = {"DISARM", "HOME", "AWAY", "NIGHT"};
    for (int i=0;i<4;++i) {
        lv_obj_t *b = button(alarm, modes[i], 22 + (i%2)*354, 128 + (i/2)*92, 330, 72, i==0?SUCCESS:CARD_ALT);
        lv_obj_add_event_cb(b, arm_cb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(b, const_cast<char *>(modes[i]));
    }

    lv_obj_t *sensors = card(parent, 804, 92, 448, 394);
    lv_obj_t *sh = label(sensors, "Sensors", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(sh, 20, 18);
    struct Row { const char *name; const char *state; uint32_t color; } rows[] = {
        {"Front door", "Locked", SUCCESS},
        {"Garage", "Closed", SUCCESS},
        {"Motion", "Clear", SUCCESS},
        {"Smoke / CO", "Normal", SUCCESS},
    };
    for (int i=0;i<4;++i) {
        lv_obj_t *r = card(sensors, 18, 62 + i*74, 412, 60);
        lv_obj_set_style_bg_color(r, lv_color_hex(CARD_ALT), LV_PART_MAIN);
        lv_obj_t *n = label(r, rows[i].name, &lv_font_montserrat_14, TEXT);
        lv_obj_set_pos(n, 14, 19);
        lv_obj_t *s = label(r, rows[i].state, &lv_font_montserrat_14, rows[i].color);
        lv_obj_align(s, LV_ALIGN_RIGHT_MID, -14, 0);
    }
}

void SecurityModule::arm_cb(lv_event_t *e) {
    auto *self = static_cast<SecurityModule *>(lv_event_get_user_data(e));
    if (!self || !self->state_label_) return;
    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    const char *mode = static_cast<const char *>(lv_obj_get_user_data(target));
    lv_label_set_text(self->state_label_, mode ? mode : "DISARMED");
    lv_obj_set_style_text_color(self->state_label_, lv_color_hex(mode && strcmp(mode,"DISARM")==0 ? SUCCESS : WARN), LV_PART_MAIN);
}
