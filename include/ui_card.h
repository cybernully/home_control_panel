#pragma once

#include <lvgl.h>
#include <stdint.h>

enum class UiCardVariant : uint8_t { CONTROL, SLIDER, FAN, ACTION, NAVIGATION, STATUS };

struct UiCard {
    UiCardVariant variant = UiCardVariant::CONTROL;
    lv_obj_t *root = nullptr;
    lv_obj_t *icon = nullptr;
    lv_obj_t *title = nullptr;
    lv_obj_t *subtitle = nullptr;
    lv_obj_t *value = nullptr;
    lv_obj_t *toggle = nullptr;
    lv_obj_t *toggle_knob = nullptr;
    lv_obj_t *slider = nullptr;
    lv_obj_t *action = nullptr;
    lv_obj_t *action_label = nullptr;
    lv_obj_t *fan_buttons[4] = {};
    lv_obj_t *fan_labels[4] = {};
    lv_obj_t *chevron = nullptr;
};

void ui_card_create(UiCard &card, lv_obj_t *parent, UiCardVariant variant,
                    int x, int y, int width, int height);
void ui_card_set_content(UiCard &card, uint32_t glyph, uint32_t glyph_color,
                         const char *title, const char *subtitle);
void ui_card_set_state(UiCard &card, bool active, bool available);
void ui_card_set_level(UiCard &card, uint8_t percentage, bool update_slider = true);
void ui_card_set_fan_level(UiCard &card, uint8_t percentage, bool active, bool available);
void ui_card_set_action(UiCard &card, const char *label);
void ui_card_set_visible(UiCard &card, bool visible);
