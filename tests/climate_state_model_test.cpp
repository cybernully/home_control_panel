#include "config_service.h"
#include "home_assistant.h"
#include "ui_state_model.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

static PanelConfig config = {};
static HomeAssistantClimateSnapshot snapshots[2] = {};
static bool ready = true;
static char queued_entity[96] = {};
static char queued_option[HA_CLIMATE_OPTION_LEN] = {};
static float queued_target = 0.0f, queued_low = 0.0f, queued_high = 0.0f;
static bool queued_range = false;
static int temperature_calls = 0, mode_calls = 0, fan_calls = 0, preset_calls = 0;

const PanelConfig &config_service_get() { return config; }
bool home_assistant_commands_ready() { return ready; }
bool home_assistant_get_climate(const char *id, HomeAssistantClimateSnapshot &out) {
    for (auto &snapshot : snapshots) {
        if (strcmp(snapshot.entity_id, id) == 0) { out = snapshot; return true; }
    }
    return false;
}
bool home_assistant_queue_climate_temperature(const char *id, float target, float low,
                                              float high, bool range) {
    snprintf(queued_entity, sizeof(queued_entity), "%s", id);
    queued_target = target; queued_low = low; queued_high = high; queued_range = range;
    ++temperature_calls; return true;
}
bool home_assistant_queue_climate_hvac_mode(const char *id, const char *value) {
    snprintf(queued_entity, sizeof(queued_entity), "%s", id);
    snprintf(queued_option, sizeof(queued_option), "%s", value); ++mode_calls; return true;
}
bool home_assistant_queue_climate_fan_mode(const char *id, const char *value) {
    snprintf(queued_entity, sizeof(queued_entity), "%s", id);
    snprintf(queued_option, sizeof(queued_option), "%s", value); ++fan_calls; return true;
}
bool home_assistant_queue_climate_preset(const char *id, const char *value) {
    snprintf(queued_entity, sizeof(queued_entity), "%s", id);
    snprintf(queued_option, sizeof(queued_option), "%s", value); ++preset_calls; return true;
}

static void configured(uint8_t index, const char *id, const char *label) {
    snprintf(config.climate_devices[index].entity_id, sizeof(config.climate_devices[index].entity_id), "%s", id);
    snprintf(config.climate_devices[index].label, sizeof(config.climate_devices[index].label), "%s", label);
}
static void option(char target[][HA_CLIMATE_OPTION_LEN], uint8_t &count, const char *value) {
    snprintf(target[count++], HA_CLIMATE_OPTION_LEN, "%s", value);
}

int main() {
    config.climate_device_count = 2;
    config.climate_show_humidity = config.climate_show_fan = config.climate_show_presets = true;
    configured(0, "climate.office", "Office"); configured(1, "climate.bedroom", "Bedroom");

    auto &office = snapshots[0];
    snprintf(office.entity_id, sizeof(office.entity_id), "climate.office");
    snprintf(office.name, sizeof(office.name), "Office thermostat");
    snprintf(office.hvac_mode, sizeof(office.hvac_mode), "heat");
    snprintf(office.hvac_action, sizeof(office.hvac_action), "heating");
    snprintf(office.fan_mode, sizeof(office.fan_mode), "auto");
    snprintf(office.preset_mode, sizeof(office.preset_mode), "home");
    snprintf(office.temperature_unit, sizeof(office.temperature_unit), "°F");
    office.available = office.has_current_temperature = office.has_target_temperature = office.has_humidity = true;
    office.current_temperature = 71.5f; office.target_temperature = 72.0f;
    office.min_temperature = 50.0f; office.max_temperature = 90.0f; office.target_step = 0.5f;
    office.humidity = 42.0f;
    option(office.hvac_modes, office.hvac_mode_count, "heat"); option(office.hvac_modes, office.hvac_mode_count, "cool");
    option(office.fan_modes, office.fan_mode_count, "auto"); option(office.fan_modes, office.fan_mode_count, "low");
    option(office.presets, office.preset_count, "home"); option(office.presets, office.preset_count, "away");

    auto &bedroom = snapshots[1]; bedroom = office;
    snprintf(bedroom.entity_id, sizeof(bedroom.entity_id), "climate.bedroom");
    bedroom.has_target_temperature = false; bedroom.has_target_range = true;
    bedroom.target_low = 66.0f; bedroom.target_high = 75.0f; bedroom.target_step = 1.0f;

    ClimateViewModel view = {};
    assert(ui_state_model_snapshot_climate(0, view));
    assert(view.configured && view.available && view.command_ready && view.tab_count == 2);
    assert(strcmp(view.name, "Office") == 0 && view.has_humidity && view.humidity == 42.0f);
    assert(ui_state_model_climate_adjust_target(view, 1));
    assert(temperature_calls == 1 && !queued_range && fabs(queued_target - 72.5f) < 0.01f);
    assert(ui_state_model_climate_set_hvac_mode(view, "cool") && mode_calls == 1);
    assert(!ui_state_model_climate_set_hvac_mode(view, "dry") && mode_calls == 1);
    assert(ui_state_model_climate_set_fan_mode(view, "low") && fan_calls == 1);
    assert(ui_state_model_climate_set_preset(view, "away") && preset_calls == 1);

    office.min_temperature = office.max_temperature = 0.0f;
    assert(ui_state_model_snapshot_climate(0, view));
    assert(view.min_temperature == 45.0f && view.max_temperature == 95.0f);

    assert(ui_state_model_snapshot_climate(1, view));
    assert(view.has_target_range && ui_state_model_climate_adjust_target(view, -1));
    assert(queued_range && fabs(queued_low - 65.0f) < 0.01f && fabs(queued_high - 74.0f) < 0.01f);
    bedroom.available = false;
    assert(ui_state_model_snapshot_climate(1, view) && !view.command_ready);
    assert(!ui_state_model_climate_adjust_target(view, 1));
    ready = false; bedroom.available = true;
    assert(ui_state_model_snapshot_climate(1, view) && !view.command_ready);
    std::cout << "Climate state model and command validation tests passed.\n";
}
