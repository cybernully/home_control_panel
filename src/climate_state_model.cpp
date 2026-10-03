#include "ui_state_model.h"

#include "config_service.h"
#include "home_assistant.h"

#include <stdio.h>
#include <string.h>

namespace {
void copy_text(char *out, size_t out_len, const char *value) {
    if (out && out_len) snprintf(out, out_len, "%s", value ? value : "");
}

template <size_t N>
void copy_options(char (&out)[N][HA_CLIMATE_OPTION_LEN],
                  const char (&source)[N][HA_CLIMATE_OPTION_LEN]) {
    memcpy(out, source, sizeof(out));
}

template <size_t N>
bool option_exists(const char (&options)[N][HA_CLIMATE_OPTION_LEN],
                   uint8_t count, const char *value) {
    if (!value || !value[0]) return false;
    for (uint8_t i = 0; i < count && i < N; ++i)
        if (strcmp(options[i], value) == 0) return true;
    return false;
}
}

bool ui_state_model_snapshot_climate(uint8_t selected_index, ClimateViewModel &out) {
    memset(&out, 0, sizeof(out));
    const PanelConfig &cfg = config_service_get();
    out.tab_count = cfg.climate_device_count > PANEL_MAX_CLIMATE_DEVICES ?
        PANEL_MAX_CLIMATE_DEVICES : cfg.climate_device_count;
    out.configured = out.tab_count > 0;
    out.show_humidity = cfg.climate_show_humidity;
    out.show_fan = cfg.climate_show_fan;
    out.show_presets = cfg.climate_show_presets;
    if (!out.configured) return false;
    if (selected_index >= out.tab_count) selected_index = 0;
    out.selected_index = selected_index;

    HomeAssistantClimateSnapshot selected = {};
    for (uint8_t i = 0; i < out.tab_count; ++i) {
        ClimateTabViewModel &tab = out.tabs[i];
        const PanelClimateDevice &configured = cfg.climate_devices[i];
        copy_text(tab.entity_id, sizeof(tab.entity_id), configured.entity_id);
        copy_text(tab.label, sizeof(tab.label), configured.label);
        HomeAssistantClimateSnapshot snapshot = {};
        if (home_assistant_get_climate(configured.entity_id, snapshot)) {
            tab.available = snapshot.available;
            if (i == selected_index) selected = snapshot;
        }
    }

    const PanelClimateDevice &configured = cfg.climate_devices[selected_index];
    copy_text(out.entity_id, sizeof(out.entity_id), configured.entity_id);
    copy_text(out.name, sizeof(out.name), configured.label[0] ? configured.label : selected.name);
    copy_text(out.hvac_mode, sizeof(out.hvac_mode), selected.hvac_mode);
    copy_text(out.hvac_action, sizeof(out.hvac_action), selected.hvac_action);
    copy_text(out.fan_mode, sizeof(out.fan_mode), selected.fan_mode);
    copy_text(out.preset_mode, sizeof(out.preset_mode), selected.preset_mode);
    copy_text(out.temperature_unit, sizeof(out.temperature_unit), selected.temperature_unit);
    out.available = selected.available;
    out.command_ready = out.available && home_assistant_commands_ready();
    out.current_temperature = selected.current_temperature;
    out.target_temperature = selected.target_temperature;
    out.target_low = selected.target_low;
    out.target_high = selected.target_high;
    out.min_temperature = selected.min_temperature;
    out.max_temperature = selected.max_temperature;
    out.target_step = selected.target_step;
    out.humidity = selected.humidity;
    out.has_current_temperature = selected.has_current_temperature;
    out.has_target_temperature = selected.has_target_temperature;
    out.has_target_range = selected.has_target_range;
    out.has_humidity = selected.has_humidity;
    if (out.target_step <= 0.0f) out.target_step = strchr(out.temperature_unit, 'F') ? 1.0f : 0.5f;
    if (out.max_temperature <= out.min_temperature) {
        out.min_temperature = strchr(out.temperature_unit, 'F') ? 45.0f : 7.0f;
        out.max_temperature = strchr(out.temperature_unit, 'F') ? 95.0f : 35.0f;
    }
    out.hvac_mode_count = selected.hvac_mode_count;
    out.fan_mode_count = selected.fan_mode_count;
    out.preset_count = selected.preset_count;
    copy_options(out.hvac_modes, selected.hvac_modes);
    copy_options(out.fan_modes, selected.fan_modes);
    copy_options(out.presets, selected.presets);
    return true;
}

bool ui_state_model_climate_adjust_target(const ClimateViewModel &climate, int direction) {
    if (!climate.command_ready || !climate.entity_id[0] || direction == 0) return false;
    float delta = climate.target_step * (direction < 0 ? -1.0f : 1.0f);
    if (climate.has_target_range) {
        if (climate.target_low + delta < climate.min_temperature)
            delta = climate.min_temperature - climate.target_low;
        if (climate.target_high + delta > climate.max_temperature)
            delta = climate.max_temperature - climate.target_high;
        if (delta == 0.0f) return false;
        return home_assistant_queue_climate_temperature(
            climate.entity_id, 0.0f, climate.target_low + delta,
            climate.target_high + delta, true);
    }
    if (!climate.has_target_temperature) return false;
    float target = climate.target_temperature + delta;
    if (target < climate.min_temperature) target = climate.min_temperature;
    if (target > climate.max_temperature) target = climate.max_temperature;
    if (target == climate.target_temperature) return false;
    return home_assistant_queue_climate_temperature(climate.entity_id, target, 0.0f, 0.0f, false);
}

bool ui_state_model_climate_set_hvac_mode(const ClimateViewModel &climate, const char *mode) {
    return climate.command_ready && option_exists(climate.hvac_modes, climate.hvac_mode_count, mode) &&
        home_assistant_queue_climate_hvac_mode(climate.entity_id, mode);
}

bool ui_state_model_climate_set_fan_mode(const ClimateViewModel &climate, const char *mode) {
    return climate.command_ready && option_exists(climate.fan_modes, climate.fan_mode_count, mode) &&
        home_assistant_queue_climate_fan_mode(climate.entity_id, mode);
}

bool ui_state_model_climate_set_preset(const ClimateViewModel &climate, const char *preset) {
    return climate.command_ready && option_exists(climate.presets, climate.preset_count, preset) &&
        home_assistant_queue_climate_preset(climate.entity_id, preset);
}
