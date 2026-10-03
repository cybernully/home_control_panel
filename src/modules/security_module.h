#pragma once
#include "module.h"
#include "ui_card.h"
#include "ui_state_model.h"

class SecurityModule final : public PanelModule {
public:
    const char *id() const override { return "security"; }
    const char *title() const override { return "Security"; }
    void create(lv_obj_t *parent) override;
    void update() override;
    void on_deactivate() override;
private:
    struct ModeBinding {
        SecurityModule *owner = nullptr;
        const char *mode = nullptr;
    };

    lv_obj_t *state_label_ = nullptr;
    lv_obj_t *state_detail_ = nullptr;
    lv_obj_t *alarm_name_ = nullptr;
    lv_obj_t *alarm_icon_ = nullptr;
    lv_obj_t *feedback_ = nullptr;
    lv_obj_t *alarm_card_ = nullptr;
    lv_obj_t *summary_card_ = nullptr;
    lv_obj_t *summary_title_ = nullptr;
    lv_obj_t *summary_detail_ = nullptr;
    lv_obj_t *arm_buttons_[4] = {};
    lv_obj_t *disarm_button_ = nullptr;
    UiCard device_cards_[PANEL_MAX_SECURITY_DEVICES] = {};

    lv_obj_t *confirm_overlay_ = nullptr;
    lv_obj_t *confirm_title_ = nullptr;
    lv_obj_t *confirm_detail_ = nullptr;
    lv_obj_t *keypad_overlay_ = nullptr;
    lv_obj_t *keypad_title_ = nullptr;
    lv_obj_t *keypad_dots_ = nullptr;
    lv_obj_t *keypad_submit_ = nullptr;

    SecurityViewModel view_ = {};
    ModeBinding mode_bindings_[5] = {};
    char pending_mode_[16] = {};
    char code_[13] = {};
    uint8_t code_length_ = 0;

    void request_mode(const char *mode);
    void execute_mode(const char *code = "");
    void show_keypad(const char *mode);
    void show_confirmation(const char *mode);
    void close_overlays();
    void clear_code();
    void update_code_display();
    static void mode_cb(lv_event_t *event);
    static void confirm_cb(lv_event_t *event);
    static void cancel_cb(lv_event_t *event);
    static void keypad_key_cb(lv_event_t *event);
    static void keypad_submit_cb(lv_event_t *event);
};
