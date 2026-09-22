#include "info_module.h"
#include "config_service.h"
#include "home_assistant.h"

InfoModule::InfoModule(const char *id, const char *title, const char *description) : id_(id), title_(title), description_(description) {}
const char *InfoModule::id() const { return id_; }
const char *InfoModule::title() const { return title_; }

void InfoModule::create(lv_obj_t *parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x111827), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(parent, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(parent, 28, LV_PART_MAIN);
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, title_);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0xF8FAFC), LV_PART_MAIN);
    lv_obj_set_pos(title, 0, 0);
    lv_obj_t *description = lv_label_create(parent);
    lv_label_set_text(description, description_);
    lv_obj_set_width(description, 1060);
    lv_label_set_long_mode(description, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(description, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(description, lv_color_hex(0x94A3B8), LV_PART_MAIN);
    lv_obj_set_pos(description, 0, 54);
    status_ = lv_label_create(parent);
    lv_obj_set_width(status_, 1060);
    lv_label_set_long_mode(status_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(status_, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(status_, lv_color_hex(0xCBD5E1), LV_PART_MAIN);
    lv_obj_set_pos(status_, 0, 130);
    update();
}

void InfoModule::update() {
    if (!status_) return;
    HomeAssistantStatus ha = {};
    home_assistant_get_status(ha);
    const PanelConfig &cfg = config_service_get();
    char text[512];
    snprintf(text, sizeof(text), "Profile: %s\nArea: %s\nHome Assistant: %s\n\n%s", cfg.profile, cfg.area_id[0] ? cfg.area_id : "(none)", ha.message[0] ? ha.message : "not tested", description_);
    lv_label_set_text(status_, text);
}
