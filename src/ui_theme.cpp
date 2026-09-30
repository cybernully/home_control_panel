#include "ui_theme.h"

namespace ui_theme {
void surface(lv_obj_t *obj, uint32_t color, int radius, int border) {
    if (!obj) return;
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, border, LV_PART_MAIN);
    if (border) lv_obj_set_style_border_color(obj, lv_color_hex(BORDER), LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void interactive(lv_obj_t *obj, bool active, bool enabled) {
    if (!obj) return;
    lv_obj_set_style_bg_color(obj, lv_color_hex(active ? 0x0B385D : SURFACE_RAISED), LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, lv_color_hex(active ? BORDER_ACTIVE : BORDER), LV_PART_MAIN);
    lv_obj_set_style_opa(obj, enabled ? LV_OPA_COVER : LV_OPA_50, LV_PART_MAIN);
    if (enabled) lv_obj_remove_state(obj, LV_STATE_DISABLED);
    else lv_obj_add_state(obj, LV_STATE_DISABLED);
}

void text(lv_obj_t *obj, uint32_t color) {
    if (obj) lv_obj_set_style_text_color(obj, lv_color_hex(color), LV_PART_MAIN);
}

void slider(lv_obj_t *obj) {
    if (!obj) return;
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x294F75), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, lv_color_hex(ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(obj, 8, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(obj, lv_color_hex(TEXT), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(obj, 6, LV_PART_KNOB);
}

void set_glyph(lv_obj_t *label, uint32_t codepoint) {
    if (!label) return;
    char utf8[5] = {};
    utf8[0] = static_cast<char>(0xF0 | (codepoint >> 18));
    utf8[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
    utf8[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    utf8[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
    lv_label_set_text(label, utf8);
}
}
