#pragma once
#include <lvgl.h>
#include <stdint.h>
#include <stdio.h>

namespace module_ui {
constexpr uint32_t BG = 0x0F172A;
constexpr uint32_t CARD = 0x172033;
constexpr uint32_t CARD_ALT = 0x1E293B;
constexpr uint32_t BORDER = 0x334155;
constexpr uint32_t TEXT = 0xF8FAFC;
constexpr uint32_t MUTED = 0x94A3B8;
constexpr uint32_t ACCENT = 0x2563EB;
constexpr uint32_t ACCENT_SOFT = 0x1D4ED8;
constexpr uint32_t SUCCESS = 0x22C55E;
constexpr uint32_t WARN = 0xF59E0B;
constexpr uint32_t DANGER = 0xEF4444;

inline void box(lv_obj_t *o, uint32_t bg = CARD, int radius = 14, int border = 1) {
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(o, radius, LV_PART_MAIN);
    lv_obj_set_style_border_width(o, border, LV_PART_MAIN);
    if (border) lv_obj_set_style_border_color(o, lv_color_hex(BORDER), LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

inline lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                       uint32_t color = TEXT) {
    lv_obj_t *o = lv_label_create(parent);
    lv_label_set_text(o, text ? text : "");
    lv_obj_set_style_text_font(o, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(o, lv_color_hex(color), LV_PART_MAIN);
    return o;
}

inline lv_obj_t *title(lv_obj_t *parent, const char *heading, const char *subtitle) {
    lv_obj_t *h = label(parent, heading, &lv_font_montserrat_28, TEXT);
    lv_obj_set_pos(h, 24, 16);
    if (subtitle && subtitle[0]) {
        lv_obj_t *s = label(parent, subtitle, &lv_font_montserrat_14, MUTED);
        lv_obj_set_pos(s, 24, 52);
    }
    return h;
}

inline lv_obj_t *card(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    box(o, CARD, 14, 1);
    return o;
}

inline lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                        uint32_t bg = CARD_ALT) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    box(b, bg, 12, 1);
    lv_obj_t *t = label(b, text, &lv_font_montserrat_16, TEXT);
    lv_obj_center(t);
    return b;
}

inline void set_button(lv_obj_t *button_obj, lv_obj_t *label_obj, bool active,
                       const char *on_text, const char *off_text) {
    if (!button_obj || !label_obj) return;
    lv_obj_set_style_bg_color(button_obj, lv_color_hex(active ? ACCENT : CARD_ALT), LV_PART_MAIN);
    lv_obj_set_style_border_color(button_obj, lv_color_hex(active ? ACCENT : BORDER), LV_PART_MAIN);
    lv_label_set_text(label_obj, active ? on_text : off_text);
}

inline void set_enabled(lv_obj_t *obj, bool enabled) {
    if (!obj) return;
    if (enabled) {
        lv_obj_remove_state(obj, LV_STATE_DISABLED);
        lv_obj_set_style_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    } else {
        lv_obj_add_state(obj, LV_STATE_DISABLED);
        lv_obj_set_style_opa(obj, LV_OPA_50, LV_PART_MAIN);
    }
}

inline lv_obj_t *chip(lv_obj_t *parent, const char *text, int x, int y, uint32_t color) {
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, 150, 30);
    box(c, color, 15, 0);
    lv_obj_t *t = label(c, text, &lv_font_montserrat_12, TEXT);
    lv_obj_center(t);
    return c;
}

inline void style_slider(lv_obj_t *slider) {
    lv_obj_set_style_bg_color(slider, lv_color_hex(CARD_ALT), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, 8, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(TEXT), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 5, LV_PART_KNOB);
}

inline void add_preview_badge(lv_obj_t *parent) {
    lv_obj_t *b = chip(parent, "UI PREVIEW", 1092, 18, ACCENT_SOFT);
    lv_obj_set_width(b, 160);
}

inline void add_live_badge(lv_obj_t *parent) {
    lv_obj_t *b = chip(parent, "HA LIVE", 1092, 18, SUCCESS);
    lv_obj_set_width(b, 160);
}
}
