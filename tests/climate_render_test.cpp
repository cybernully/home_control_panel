#include "climate_module.h"
#include "ui_state_model.h"

#include <lvgl.h>
#include <cassert>
#include <cstdio>
#include <cstring>

static ClimateViewModel climate = {};
static int adjusted = 0;
static char selected_mode[HA_CLIMATE_OPTION_LEN] = {};
static char selected_fan[HA_CLIMATE_OPTION_LEN] = {};
static char selected_preset[HA_CLIMATE_OPTION_LEN] = {};

bool ui_state_model_snapshot_climate(uint8_t index, ClimateViewModel &out) {
    out = climate; out.selected_index = index < out.tab_count ? index : 0;
    if (out.selected_index == 1) {
        snprintf(out.entity_id, sizeof(out.entity_id), "climate.bedroom");
        snprintf(out.name, sizeof(out.name), "Bedroom Climate");
    }
    return out.configured;
}
bool ui_state_model_climate_adjust_target(const ClimateViewModel &, int direction) { adjusted = direction; return true; }
bool ui_state_model_climate_set_hvac_mode(const ClimateViewModel &, const char *mode) { snprintf(selected_mode, sizeof(selected_mode), "%s", mode); return true; }
bool ui_state_model_climate_set_fan_mode(const ClimateViewModel &, const char *mode) { snprintf(selected_fan, sizeof(selected_fan), "%s", mode); return true; }
bool ui_state_model_climate_set_preset(const ClimateViewModel &, const char *preset) { snprintf(selected_preset, sizeof(selected_preset), "%s", preset); return true; }
void ui_shell_report_status(const char *) {}

static unsigned char buffer[1280 * 658 * 4];
static void flush(lv_display_t *display, const lv_area_t *, uint8_t *) { lv_display_flush_ready(display); }
static lv_obj_t *find_containing(lv_obj_t *root, const char *text) {
    if (lv_obj_check_type(root, &lv_label_class) && strstr(lv_label_get_text(root), text)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto *result = find_containing(lv_obj_get_child(root, i), text)) return result;
    return nullptr;
}
static lv_obj_t *find(lv_obj_t *root, const char *text) {
    if (lv_obj_check_type(root, &lv_label_class) && strcmp(lv_label_get_text(root), text) == 0) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto *result = find(lv_obj_get_child(root, i), text)) return result;
    return nullptr;
}
static void click(lv_obj_t *root, const char *text) { auto *item = find(root, text); assert(item); lv_obj_send_event(lv_obj_get_parent(item), LV_EVENT_CLICKED, nullptr); }
static void shot(const char *name) {
    lv_refr_now(nullptr); FILE *file = fopen(name, "wb"); assert(file);
    fprintf(file, "P6\n1280 658\n255\n");
    for (int i = 0; i < 1280 * 658; ++i) { fputc(buffer[4*i+2], file); fputc(buffer[4*i+1], file); fputc(buffer[4*i], file); }
    fclose(file);
}
static void add_option(char values[][HA_CLIMATE_OPTION_LEN], uint8_t &count, const char *value) { snprintf(values[count++], HA_CLIMATE_OPTION_LEN, "%s", value); }

int main() {
    lv_init(); auto *display = lv_display_create(1280, 658);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display, buffer, nullptr, sizeof(buffer), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);

    climate.configured = climate.available = climate.command_ready = true;
    climate.show_humidity = climate.show_fan = climate.show_presets = true;
    climate.tab_count = 2; climate.has_current_temperature = climate.has_target_temperature = climate.has_humidity = true;
    climate.current_temperature = 71.5f; climate.target_temperature = 72.0f; climate.target_step = 0.5f; climate.humidity = 42.0f;
    snprintf(climate.entity_id, sizeof(climate.entity_id), "climate.office");
    snprintf(climate.name, sizeof(climate.name), "Office Climate");
    snprintf(climate.temperature_unit, sizeof(climate.temperature_unit), "°F");
    snprintf(climate.hvac_mode, sizeof(climate.hvac_mode), "heat");
    snprintf(climate.hvac_action, sizeof(climate.hvac_action), "heating");
    snprintf(climate.fan_mode, sizeof(climate.fan_mode), "auto");
    snprintf(climate.preset_mode, sizeof(climate.preset_mode), "home");
    snprintf(climate.tabs[0].label, sizeof(climate.tabs[0].label), "Office"); climate.tabs[0].available = true;
    snprintf(climate.tabs[1].label, sizeof(climate.tabs[1].label), "Bedroom"); climate.tabs[1].available = true;
    add_option(climate.hvac_modes, climate.hvac_mode_count, "heat"); add_option(climate.hvac_modes, climate.hvac_mode_count, "cool"); add_option(climate.hvac_modes, climate.hvac_mode_count, "auto"); add_option(climate.hvac_modes, climate.hvac_mode_count, "off");
    add_option(climate.fan_modes, climate.fan_mode_count, "auto"); add_option(climate.fan_modes, climate.fan_mode_count, "low");
    add_option(climate.presets, climate.preset_count, "home"); add_option(climate.presets, climate.preset_count, "away");

    ClimateModule module; auto *root = lv_screen_active(); module.create(root); lv_obj_update_layout(root);
    assert(find(root, "Climate") && find(root, "Office") && find(root, "Bedroom"));
    auto *subtitle = find(root, "Live thermostats, comfort and HVAC controls");
    auto *office_button = lv_obj_get_parent(find(root, "Office"));
    auto *current_state = find(root, "CURRENT STATE");
    auto *thermostat_card = lv_obj_get_parent(current_state);
    assert(subtitle && office_button && thermostat_card);
    assert(lv_obj_get_y(office_button) >= lv_obj_get_y(subtitle) + lv_obj_get_height(subtitle) + 8);
    assert(lv_obj_get_y(thermostat_card) >= lv_obj_get_y(office_button) + lv_obj_get_height(office_button) + 12);
    assert(lv_label_get_long_mode(lv_obj_get_child(office_button, 0)) == LV_LABEL_LONG_DOT);
    assert(!find(root, "HA LIVE") && !find(root, "SYSTEM STATUS"));
    assert(find(root, "CURRENT STATE") && find(root, "TEMPERATURE") && find(root, "HVAC MODE"));
    assert(find_containing(root, "71.5") && find_containing(root, "72.0") && find(root, "Humidity 42%"));
    assert(find(root, "Heat") && find(root, "Cool") && find(root, "Auto") && find(root, "Off"));
    assert(find_containing(root, "Fan") && find_containing(root, "Preset"));
    shot(".test-build/climate-live.ppm");
    click(root, "+"); assert(adjusted == 1);
    click(root, "Cool"); assert(strcmp(selected_mode, "cool") == 0);
    auto *fan = find_containing(root, "Fan"); assert(fan); lv_obj_send_event(lv_obj_get_parent(fan), LV_EVENT_CLICKED, nullptr); assert(strcmp(selected_fan, "low") == 0);
    auto *preset = find_containing(root, "Preset"); assert(preset); lv_obj_send_event(lv_obj_get_parent(preset), LV_EVENT_CLICKED, nullptr); assert(strcmp(selected_preset, "away") == 0);
    click(root, "Bedroom");

    climate.available = climate.command_ready = false; module.update();
    assert(find_containing(root, "Unavailable")); shot(".test-build/climate-unavailable.ppm");
    climate.configured = false; climate.tab_count = 0; module.update();
    assert(find(root, "No Climate devices configured")); shot(".test-build/climate-empty.ppm");
    std::puts("Climate render and interaction tests passed.");
}
