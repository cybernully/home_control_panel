#include "security_module.h"
#include "ui_state_model.h"

#include <lvgl.h>
#include <cassert>
#include <cstdio>
#include <cstring>

static SecurityViewModel security = {};
static char action_mode[16] = {};
static char action_code[16] = {};

bool ui_state_model_snapshot_security(SecurityViewModel &out) { out = security; return out.configured; }
bool ui_state_model_security_action(const SecurityViewModel &, const char *mode, const char *code) {
    snprintf(action_mode, sizeof(action_mode), "%s", mode ? mode : "");
    snprintf(action_code, sizeof(action_code), "%s", code ? code : "");
    return true;
}

static unsigned char buffer[1280 * 658 * 4];
static void flush(lv_display_t *display, const lv_area_t *, uint8_t *) { lv_display_flush_ready(display); }

static lv_obj_t *find_nth(lv_obj_t *root, const char *text, unsigned &remaining) {
    if (lv_obj_check_type(root, &lv_label_class) && strcmp(lv_label_get_text(root), text) == 0) {
        if (!remaining) return root;
        --remaining;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto *result = find_nth(lv_obj_get_child(root, i), text, remaining)) return result;
    return nullptr;
}
static lv_obj_t *find(lv_obj_t *root, const char *text, unsigned occurrence = 0) {
    return find_nth(root, text, occurrence);
}
static void click_label(lv_obj_t *root, const char *text, unsigned occurrence = 0) {
    lv_obj_t *label = find(root, text, occurrence); assert(label);
    lv_obj_send_event(lv_obj_get_parent(label), LV_EVENT_CLICKED, nullptr);
}
static void shot(const char *name) {
    lv_refr_now(nullptr); FILE *file = fopen(name, "wb"); assert(file);
    fprintf(file, "P6\n1280 658\n255\n");
    for (int i = 0; i < 1280 * 658; ++i) {
        fputc(buffer[4*i+2], file); fputc(buffer[4*i+1], file); fputc(buffer[4*i], file);
    }
    fclose(file);
}

int main() {
    lv_init(); auto *display = lv_display_create(1280, 658);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display, buffer, nullptr, sizeof(buffer), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);

    security.configured = security.available = security.command_ready = true;
    security.show_abnormal_summary = security.confirm_arming = true;
    security.arm_home = security.arm_away = security.arm_night = true;
    snprintf(security.alarm_name, sizeof(security.alarm_name), "Home Alarm");
    snprintf(security.alarm_state, sizeof(security.alarm_state), "disarmed");
    snprintf(security.state_label, sizeof(security.state_label), "DISARMED");
    snprintf(security.state_detail, sizeof(security.state_detail), "Ready to arm");
    snprintf(security.abnormal_summary, sizeof(security.abnormal_summary), "1 device needs attention");
    security.device_count = 4; security.abnormal_count = 1;
    const char *names[] = {"Front door", "Garage", "Alarm network", "Smoke / CO"};
    const char *states[] = {"Open", "Closed", "Online", "Normal"};
    const char *icons[] = {"door", "garage", "power", "alert"};
    for (uint8_t i = 0; i < security.device_count; ++i) {
        auto &device = security.devices[i];
        snprintf(device.entity_id, sizeof(device.entity_id), "binary_sensor.security_%u", i);
        snprintf(device.title, sizeof(device.title), "%s", names[i]);
        snprintf(device.state_text, sizeof(device.state_text), "%s", states[i]);
        snprintf(device.icon, sizeof(device.icon), "%s", icons[i]);
        snprintf(device.color, sizeof(device.color), "%s", "red");
        device.available = true;
        device.abnormal = i == 0;
    }

    SecurityModule module; auto *root = lv_screen_active(); module.create(root); lv_obj_update_layout(root);
    assert(find(root, "Security") && find(root, "DISARMED") && find(root, "Attention needed"));
    assert(find(root, "Front door") && find(root, "Open") && find(root, "Alarm network"));
    shot(".test-build/security-disarmed.ppm");

    click_label(root, "Away");
    lv_obj_t *confirm = find(root, "Confirm arming"); assert(confirm);
    assert(!lv_obj_has_flag(lv_obj_get_parent(lv_obj_get_parent(confirm)), LV_OBJ_FLAG_HIDDEN));
    click_label(root, "Arm");
    assert(strcmp(action_mode, "away") == 0 && action_code[0] == '\0');

    snprintf(security.alarm_state, sizeof(security.alarm_state), "armed_away");
    snprintf(security.state_label, sizeof(security.state_label), "ARMED AWAY");
    snprintf(security.state_detail, sizeof(security.state_detail), "Full protection is active");
    security.armed = true;
    module.update();
    click_label(root, "Disarm", 0);
    assert(find(root, "Enter code to disarm"));
    click_label(root, "1"); click_label(root, "2"); click_label(root, "3"); click_label(root, "4");
    shot(".test-build/security-keypad.ppm");
    click_label(root, "Disarm", 1);
    assert(strcmp(action_mode, "disarm") == 0 && strcmp(action_code, "1234") == 0);

    snprintf(security.alarm_state, sizeof(security.alarm_state), "triggered");
    snprintf(security.state_label, sizeof(security.state_label), "ALARM TRIGGERED");
    snprintf(security.state_detail, sizeof(security.state_detail), "Check the property and disarm when safe");
    security.triggered = true;
    module.update();
    shot(".test-build/security-triggered.ppm");
    std::puts("Security render, confirmation, and keypad interactions passed.");
    return 0;
}
