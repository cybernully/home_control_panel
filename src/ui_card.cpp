#include "ui_card.h"

#include "ha_icons_font.h"
#include "display_text.h"
#include "ui_theme.h"

namespace {
lv_obj_t *make_label(lv_obj_t *parent, const char *value, const lv_font_t *font,
                     uint32_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, value ? value : "");
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    return label;
}
void ellipsis(lv_obj_t *label, int width) {
    lv_obj_set_width(label, width);
    const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    if (font) lv_obj_set_height(label, font->line_height);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
}
}

void ui_card_create(UiCard &card, lv_obj_t *parent, UiCardVariant variant,
                    int x, int y, int width, int height) {
    card = {};
    card.variant = variant;
    card.root = lv_button_create(parent);
    lv_obj_set_pos(card.root, x, y);
    lv_obj_set_size(card.root, width, height);
    ui_theme::surface(card.root, ui_theme::SURFACE_RAISED, 16, 1);

    card.icon = make_label(card.root, "", &ha_icons_font, ui_theme::MUTED);
    lv_obj_set_size(card.icon, 54, 54);
    lv_obj_set_style_text_align(card.icon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    const bool primary_control = variant == UiCardVariant::CONTROL ||
                                 variant == UiCardVariant::SLIDER ||
                                 variant == UiCardVariant::FAN ||
                                 variant == UiCardVariant::ACTION;
    const int icon_y = variant == UiCardVariant::NAVIGATION ? (height - 54) / 2 :
                       variant == UiCardVariant::STATUS ? (height - 54) / 2 : 12;
    lv_obj_set_pos(card.icon, 18, icon_y);
    lv_obj_remove_flag(card.icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(card.icon, LV_OBJ_FLAG_EVENT_BUBBLE);

    card.title = make_label(card.root, "", &lv_font_montserrat_18, ui_theme::TEXT);
    const int title_x = variant == UiCardVariant::NAVIGATION ? 88 : primary_control ? 76 : 80;
    const int title_y = variant == UiCardVariant::NAVIGATION ? height / 2 - 27 :
                        variant == UiCardVariant::STATUS ? height / 2 - 25 : 14;
    lv_obj_set_pos(card.title, title_x, title_y);
    ellipsis(card.title, width - title_x - 28);
    card.subtitle = make_label(card.root, "", &lv_font_montserrat_14, ui_theme::MUTED);
    lv_obj_set_pos(card.subtitle, title_x, variant == UiCardVariant::NAVIGATION ? height / 2 + 5 :
                   variant == UiCardVariant::STATUS ? height / 2 + 8 : 45);
    ellipsis(card.subtitle, width - title_x - 28);

    if (variant == UiCardVariant::CONTROL) {
        card.toggle = lv_obj_create(card.root);
        lv_obj_set_size(card.toggle, 62, 34);
        lv_obj_set_pos(card.toggle, width - 78, height - 46);
        ui_theme::surface(card.toggle, 0x36587C, 17, 0);
        card.toggle_knob = lv_obj_create(card.toggle);
        lv_obj_set_size(card.toggle_knob, 28, 28);
        lv_obj_set_pos(card.toggle_knob, 3, 3);
        ui_theme::surface(card.toggle_knob, ui_theme::TEXT, 14, 0);
        lv_obj_remove_flag(card.toggle, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(card.toggle_knob, LV_OBJ_FLAG_CLICKABLE);
    }
    if (variant == UiCardVariant::SLIDER) {
        card.slider = lv_slider_create(card.root);
        lv_obj_set_pos(card.slider, 20, height - 38);
        lv_obj_set_size(card.slider, width - 90, 22);
        lv_obj_set_ext_click_area(card.slider, 12);
        lv_slider_set_range(card.slider, 0, 100);
        ui_theme::slider(card.slider);
        lv_obj_remove_flag(card.slider, LV_OBJ_FLAG_EVENT_BUBBLE);
        card.value = make_label(card.root, "0%", &lv_font_montserrat_14, ui_theme::MUTED);
        lv_obj_set_pos(card.value, width - 58, height - 41);
    }
    if (variant == UiCardVariant::FAN) {
        static const char *const labels[] = {"Off", "Low", "Med", "High"};
        const int gap = 4;
        const int button_width = (width - 36 - gap * 3) / 4;
        for (int i = 0; i < 4; ++i) {
            card.fan_buttons[i] = lv_button_create(card.root);
            lv_obj_set_pos(card.fan_buttons[i], 18 + i * (button_width + gap), height - 46);
            lv_obj_set_size(card.fan_buttons[i], button_width, 34);
            ui_theme::surface(card.fan_buttons[i], ui_theme::SURFACE, 9, 1);
            lv_obj_set_style_shadow_width(card.fan_buttons[i], 0, LV_PART_MAIN);
            lv_obj_remove_flag(card.fan_buttons[i], LV_OBJ_FLAG_EVENT_BUBBLE);
            card.fan_labels[i] = make_label(card.fan_buttons[i], labels[i],
                                             &lv_font_montserrat_12, ui_theme::MUTED);
            lv_obj_center(card.fan_labels[i]);
        }
    }
    if (variant == UiCardVariant::ACTION) {
        card.action = lv_obj_create(card.root);
        lv_obj_set_pos(card.action, 18, height - 52);
        lv_obj_set_size(card.action, width - 36, 40);
        ui_theme::surface(card.action, 0x294982, 12, 1);
        card.action_label = make_label(card.action, "Activate", &lv_font_montserrat_16, ui_theme::TEXT);
        lv_obj_center(card.action_label);
        lv_obj_remove_flag(card.action, LV_OBJ_FLAG_CLICKABLE);
    }
    if (variant == UiCardVariant::NAVIGATION) {
        card.chevron = make_label(card.root, ">", &lv_font_montserrat_24, ui_theme::TEXT);
        lv_obj_align(card.chevron, LV_ALIGN_RIGHT_MID, -18, 0);
        lv_obj_remove_flag(card.chevron, LV_OBJ_FLAG_CLICKABLE);
        ellipsis(card.title, width - title_x - 62);
        ellipsis(card.subtitle, width - title_x - 62);
    }
}

void ui_card_set_content(UiCard &card, uint32_t glyph, uint32_t glyph_color,
                         const char *title, const char *subtitle) {
    ui_theme::set_glyph(card.icon, glyph);
    lv_obj_set_style_text_color(card.icon, lv_color_hex(glyph_color), LV_PART_MAIN);
    char safe_title[96], safe_subtitle[96];
    panel_display_text(safe_title, sizeof(safe_title), title ? title : "");
    panel_display_text(safe_subtitle, sizeof(safe_subtitle), subtitle ? subtitle : "");
    lv_label_set_text(card.title, safe_title);
    lv_label_set_text(card.subtitle, safe_subtitle);
}

void ui_card_set_state(UiCard &card, bool active, bool available) {
    ui_theme::interactive(card.root, active, available);
    if (card.toggle) {
        lv_obj_set_style_bg_color(card.toggle, lv_color_hex(active ? ui_theme::ACCENT : 0x36587C), LV_PART_MAIN);
        lv_obj_set_x(card.toggle_knob, active ? 31 : 3);
    }
    if (card.slider) {
        if (available) lv_obj_remove_state(card.slider, LV_STATE_DISABLED);
        else lv_obj_add_state(card.slider, LV_STATE_DISABLED);
    }
}

void ui_card_set_level(UiCard &card, uint8_t percentage, bool update_slider) {
    if (card.slider && update_slider) lv_slider_set_value(card.slider, percentage, LV_ANIM_OFF);
    if (card.value) {
        char value[8];
        lv_snprintf(value, sizeof(value), "%u%%", static_cast<unsigned>(percentage));
        lv_label_set_text(card.value, value);
    }
}

void ui_card_set_fan_level(UiCard &card, uint8_t percentage, bool active, bool available) {
    int selected = 0;
    if (active && percentage > 0) selected = percentage <= 40 ? 1 : percentage <= 75 ? 2 : 3;
    for (int i = 0; i < 4; ++i) {
        if (!card.fan_buttons[i]) continue;
        const bool highlighted = i == selected;
        ui_theme::surface(card.fan_buttons[i], highlighted ? ui_theme::ACCENT : ui_theme::SURFACE,
                          9, highlighted ? 0 : 1);
        lv_obj_set_style_text_color(card.fan_labels[i],
                                    lv_color_hex(highlighted ? ui_theme::TEXT : ui_theme::MUTED),
                                    LV_PART_MAIN);
        if (available) lv_obj_remove_state(card.fan_buttons[i], LV_STATE_DISABLED);
        else lv_obj_add_state(card.fan_buttons[i], LV_STATE_DISABLED);
    }
}

void ui_card_set_action(UiCard &card, const char *label) {
    if (card.action_label) lv_label_set_text(card.action_label, label ? label : "Activate");
}

void ui_card_set_visible(UiCard &card, bool visible) {
    if (!card.root) return;
    if (visible) lv_obj_remove_flag(card.root, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(card.root, LV_OBJ_FLAG_HIDDEN);
}
