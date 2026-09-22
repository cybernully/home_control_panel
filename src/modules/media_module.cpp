#include "media_module.h"
#include "module_ui.h"
#include <Arduino.h>
using namespace module_ui;

void MediaModule::create(lv_obj_t *parent) {
    box(parent, BG, 0, 0);
    module_ui::title(parent, "Media", "Home Assistant media-player control surface");
    add_preview_badge(parent);

    lv_obj_t *now = card(parent, 24, 92, 760, 394);
    lv_obj_t *eyebrow = label(now, "NOW PLAYING", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(eyebrow, 22, 20);
    lv_obj_t *track = label(now, "Nothing playing", &lv_font_montserrat_28, TEXT);
    lv_obj_set_pos(track, 22, 58);
    lv_obj_t *artist = label(now, "Media entity not bound", &lv_font_montserrat_16, MUTED);
    lv_obj_set_pos(artist, 22, 101);

    lv_obj_t *prev = button(now, "PREV", 22, 174, 180, 76, CARD_ALT);
    (void)prev;
    play_button_ = button(now, "PLAY", 222, 174, 294, 76, ACCENT);
    play_label_ = lv_obj_get_child(play_button_, 0);
    lv_obj_add_event_cb(play_button_, play_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *next = button(now, "NEXT", 536, 174, 180, 76, CARD_ALT);
    (void)next;

    lv_obj_t *note = label(now, "Transport buttons are touch-active now; HA service calls will be wired to the selected media_player entity.", &lv_font_montserrat_14, MUTED);
    lv_obj_set_width(note, 700);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(note, 22, 294);

    lv_obj_t *right = card(parent, 804, 92, 448, 394);
    lv_obj_t *vt = label(right, "Volume", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(vt, 20, 20);
    volume_label_ = label(right, "35%", &lv_font_montserrat_24, TEXT);
    lv_obj_align(volume_label_, LV_ALIGN_TOP_RIGHT, -20, 16);
    lv_obj_t *slider = lv_slider_create(right);
    lv_obj_set_pos(slider, 26, 78);
    lv_obj_set_size(slider, 396, 28);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 35, LV_ANIM_OFF);
    style_slider(slider);
    lv_obj_add_event_cb(slider, volume_cb, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t *src = label(right, "Source", &lv_font_montserrat_18, TEXT);
    lv_obj_set_pos(src, 20, 146);
    const char *sources[] = {"TV", "Music", "Cast"};
    for (int i=0;i<3;++i) {
        lv_obj_t *b = button(right, sources[i], 20 + i*136, 184, 124, 54, i==0?ACCENT:CARD_ALT);
        lv_obj_add_event_cb(b, source_cb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(b, const_cast<char *>(sources[i]));
    }
    source_label_ = label(right, "Selected: TV", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(source_label_, 20, 270);
}

void MediaModule::play_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (!self) return;
    self->playing_ = !self->playing_;
    set_button(self->play_button_, self->play_label_, self->playing_, "PAUSE", "PLAY");
}

void MediaModule::volume_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (!self || !self->volume_label_) return;
    int value = static_cast<int>(lv_slider_get_value(static_cast<lv_obj_t *>(lv_event_get_target(e))));
    char text[16]; snprintf(text, sizeof(text), "%d%%", value);
    lv_label_set_text(self->volume_label_, text);
}

void MediaModule::source_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (!self || !self->source_label_) return;
    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    const char *name = static_cast<const char *>(lv_obj_get_user_data(target));
    char text[48]; snprintf(text, sizeof(text), "Selected: %s", name ? name : "Source");
    lv_label_set_text(self->source_label_, text);
}
