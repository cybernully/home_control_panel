#include "ui_theme.h"

#include <string.h>

namespace ui_theme {
namespace {
bool contains_ci(const char *text_value, const char *needle) {
    if (!text_value || !needle || !needle[0]) return false;
    for (const char *start = text_value; *start; ++start) {
        const char *a = start, *b = needle;
        while (*a && *b) {
            char left = *a, right = *b;
            if (left >= 'A' && left <= 'Z') left = static_cast<char>(left - 'A' + 'a');
            if (right >= 'A' && right <= 'Z') right = static_cast<char>(right - 'A' + 'a');
            if (left != right) break;
            ++a; ++b;
        }
        if (!*b) return true;
    }
    return false;
}
}
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

uint32_t status_glyph(const char *configured_icon, const char *entity_id,
                      const char *title, bool active) {
    const char *icon = configured_icon && configured_icon[0] ? configured_icon : "auto";
    if (strcmp(icon, "auto") == 0) {
        if (entity_id && strncmp(entity_id, "light.", 6) == 0) icon = "light";
        else if (entity_id && strncmp(entity_id, "fan.", 4) == 0) icon = "fan";
        else if (entity_id && strncmp(entity_id, "cover.", 6) == 0)
            icon = contains_ci(entity_id, "garage") || contains_ci(title, "garage") ? "garage" : "cover";
        else if (entity_id && strncmp(entity_id, "lock.", 5) == 0) icon = "lock";
        else if (entity_id && strncmp(entity_id, "binary_sensor.", 14) == 0)
            icon = contains_ci(title, "motion") ? "motion" : "door";
        else if (entity_id && strncmp(entity_id, "weather.", 8) == 0) icon = "weather";
        else if (entity_id && strncmp(entity_id, "timer.", 6) == 0) icon = "timer";
        else if (entity_id && strncmp(entity_id, "sensor.", 7) == 0 && contains_ci(title, "humidity")) icon = "humidity";
        else if (entity_id && strncmp(entity_id, "sensor.", 7) == 0 && contains_ci(title, "temp")) icon = "temperature";
        else icon = "alert";
    }
    if (strcmp(icon, "garage") == 0) return active ? 0xF06DA : 0xF06D9;
    if (strcmp(icon, "door") == 0) return active ? 0xF081C : 0xF081B;
    if (strcmp(icon, "lock") == 0) return active ? 0xF033F : 0xF033E;
    if (strcmp(icon, "motion") == 0) return 0xF0D91;
    if (strcmp(icon, "light") == 0) return active ? 0xF0335 : 0xF0336;
    if (strcmp(icon, "fan") == 0) return active ? 0xF0210 : 0xF081D;
    if (strcmp(icon, "cover") == 0) return active ? 0xF1011 : 0xF00AC;
    if (strcmp(icon, "window") == 0) return active ? 0xF05B1 : 0xF05AE;
    if (strcmp(icon, "camera") == 0) return 0xF0100;
    if (strcmp(icon, "shield") == 0) return 0xF068A;
    if (strcmp(icon, "temperature") == 0) return 0xF050F;
    if (strcmp(icon, "humidity") == 0) return 0xF058E;
    if (strcmp(icon, "power") == 0) return active ? 0xF0425 : 0xF0902;
    if (strcmp(icon, "devices") == 0) return 0xF07E9;
    if (strcmp(icon, "weather") == 0) return 0xF0595;
    if (strcmp(icon, "timer") == 0) return 0xF051B;
    return active ? 0xF05E0 : 0xF0028;
}

uint32_t status_color(const char *color) {
    if (color && strcmp(color, "green") == 0) return SUCCESS;
    if (color && strcmp(color, "yellow") == 0) return YELLOW;
    if (color && strcmp(color, "red") == 0) return DANGER;
    if (color && strcmp(color, "purple") == 0) return 0xA78BFA;
    if (color && strcmp(color, "blue") == 0) return 0x70A5FF;
    return CYAN;
}
}
