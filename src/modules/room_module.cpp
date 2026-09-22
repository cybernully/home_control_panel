#include "room_module.h"
#include "config_service.h"
#include "module_ui.h"
#include <Arduino.h>

using namespace module_ui;

namespace {
const char *kNames[4] = {"Main Lights", "Lamps", "Ceiling Fan", "Shades"};
const char *kDetails[4] = {"Dimmable light group", "Accent lighting", "Fan switch", "Window covering"};
}

void RoomModule::create(lv_obj_t *parent) {
    box(parent, BG, 0, 0);
    const PanelConfig &cfg = config_service_get();
    char sub[96];
    snprintf(sub, sizeof(sub), "Controls for area: %s", cfg.area_id[0] ? cfg.area_id : "not configured");
    module_ui::title(parent, "Room Controls", sub);
    add_preview_badge(parent);

    for (int i = 0; i < 4; ++i) {
        int x = 24 + (i % 4) * 306;
        lv_obj_t *c = card(parent, x, 92, 286, 176);
        lv_obj_t *name = label(c, kNames[i], &lv_font_montserrat_18, TEXT);
        lv_obj_set_pos(name, 16, 16);
        lv_obj_t *detail = label(c, kDetails[i], &lv_font_montserrat_12, MUTED);
        lv_obj_set_pos(detail, 16, 47);
        controls_[i].button = button(c, "OFF", 16, 92, 254, 58, CARD_ALT);
        controls_[i].label = lv_obj_get_child(controls_[i].button, 0);
        lv_obj_add_event_cb(controls_[i].button, toggle_cb, LV_EVENT_CLICKED, &controls_[i]);
    }

    lv_obj_t *brightness = card(parent, 24, 288, 590, 198);
    lv_obj_t *bt = label(brightness, "Room brightness", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(bt, 18, 18);
    brightness_label_ = label(brightness, "60%", &lv_font_montserrat_24, TEXT);
    lv_obj_align(brightness_label_, LV_ALIGN_TOP_RIGHT, -18, 14);
    lv_obj_t *slider = lv_slider_create(brightness);
    lv_obj_set_pos(slider, 24, 82);
    lv_obj_set_size(slider, 542, 28);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 60, LV_ANIM_OFF);
    style_slider(slider);
    lv_obj_add_event_cb(slider, brightness_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_t *hint = label(brightness, "Preview slider - ready for light domain binding", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(hint, 18, 142);

    lv_obj_t *scenes = card(parent, 634, 288, 618, 198);
    lv_obj_t *st = label(scenes, "Scenes", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(st, 18, 18);
    const char *scene_names[] = {"Relax", "Bright", "Movie"};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *b = button(scenes, scene_names[i], 18 + i * 194, 62, 178, 60, i == 0 ? ACCENT : CARD_ALT);
        lv_obj_add_event_cb(b, scene_cb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(b, const_cast<char *>(scene_names[i]));
    }
    scene_status_ = label(scenes, "No scene selected", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(scene_status_, 18, 150);
}

void RoomModule::toggle_cb(lv_event_t *e) {
    auto *ctl = static_cast<ToggleControl *>(lv_event_get_user_data(e));
    if (!ctl) return;
    ctl->on = !ctl->on;
    set_button(ctl->button, ctl->label, ctl->on, "ON", "OFF");
}

void RoomModule::brightness_cb(lv_event_t *e) {
    auto *self = static_cast<RoomModule *>(lv_event_get_user_data(e));
    if (!self || !self->brightness_label_) return;
    int value = static_cast<int>(lv_slider_get_value(static_cast<lv_obj_t *>(lv_event_get_target(e))));
    char text[16];
    snprintf(text, sizeof(text), "%d%%", value);
    lv_label_set_text(self->brightness_label_, text);
}

void RoomModule::scene_cb(lv_event_t *e) {
    auto *self = static_cast<RoomModule *>(lv_event_get_user_data(e));
    if (!self || !self->scene_status_) return;
    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    const char *name = static_cast<const char *>(lv_obj_get_user_data(target));
    char msg[80];
    snprintf(msg, sizeof(msg), "%s selected (local preview)", name ? name : "Scene");
    lv_label_set_text(self->scene_status_, msg);
}
