#include "config_service.h"
#include "home_assistant.h"
#include "ui_state_model.h"

#include <cassert>
#include <cstring>
#include <iostream>

static PanelConfig config = {};
static HomeAssistantEntitySnapshot entities[8] = {};
static size_t entity_count = 0;
static bool ready = true;
static char queued_mode[16] = {};
static char queued_code[24] = {};

const PanelConfig &config_service_get() { return config; }
bool home_assistant_commands_ready() { return ready; }
bool home_assistant_get_entity(const char *id, HomeAssistantEntitySnapshot &out) {
    for (size_t i = 0; i < entity_count; ++i) {
        if (strcmp(id, entities[i].entity_id) == 0) { out = entities[i]; return true; }
    }
    return false;
}
bool home_assistant_queue_alarm(const char *, const char *mode, const char *code) {
    snprintf(queued_mode, sizeof(queued_mode), "%s", mode ? mode : "");
    snprintf(queued_code, sizeof(queued_code), "%s", code ? code : "");
    return true;
}

static void entity(size_t index, const char *id, const char *name,
                   const char *domain, const char *state, bool available = true) {
    auto &out = entities[index];
    snprintf(out.entity_id, sizeof(out.entity_id), "%s", id);
    snprintf(out.name, sizeof(out.name), "%s", name);
    snprintf(out.domain, sizeof(out.domain), "%s", domain);
    snprintf(out.state, sizeof(out.state), "%s", state);
    out.available = available;
}

int main() {
    assert(ui_state_model_security_state_is_abnormal("OPEN", "on, open, opening", false));
    assert(!ui_state_model_security_state_is_abnormal("closed", "on,open", false));
    assert(!ui_state_model_security_state_is_abnormal("on", "on", true));
    assert(ui_state_model_security_state_is_abnormal("off", "on", true));

    snprintf(config.alarm_entity_id, sizeof(config.alarm_entity_id), "alarm_control_panel.alarmo");
    config.security_show_abnormal_summary = true;
    config.security_confirm_arming = true;
    config.security_arm_home = config.security_arm_away = config.security_arm_night = true;
    config.security_device_count = 3;
    auto &door = config.security_devices[0];
    snprintf(door.entity_id, sizeof(door.entity_id), "binary_sensor.front_door");
    snprintf(door.label, sizeof(door.label), "Front door");
    snprintf(door.icon, sizeof(door.icon), "door");
    snprintf(door.abnormal_states, sizeof(door.abnormal_states), "on,open");
    snprintf(door.normal_label, sizeof(door.normal_label), "Closed");
    snprintf(door.abnormal_label, sizeof(door.abnormal_label), "Open");
    snprintf(door.color, sizeof(door.color), "red");
    auto &network = config.security_devices[1];
    snprintf(network.entity_id, sizeof(network.entity_id), "binary_sensor.alarm_network");
    snprintf(network.label, sizeof(network.label), "Alarm network");
    snprintf(network.icon, sizeof(network.icon), "power");
    snprintf(network.abnormal_states, sizeof(network.abnormal_states), "on");
    snprintf(network.normal_label, sizeof(network.normal_label), "Online");
    snprintf(network.abnormal_label, sizeof(network.abnormal_label), "Offline");
    snprintf(network.color, sizeof(network.color), "yellow");
    network.reverse_abnormal = true;
    auto &garage = config.security_devices[2];
    snprintf(garage.entity_id, sizeof(garage.entity_id), "cover.garage");
    snprintf(garage.label, sizeof(garage.label), "Garage");
    snprintf(garage.icon, sizeof(garage.icon), "garage");
    snprintf(garage.abnormal_states, sizeof(garage.abnormal_states), "open,opening");
    snprintf(garage.color, sizeof(garage.color), "red");
    config.security_dynamic_device_count = 2;
    auto &back_gate = config.security_dynamic_devices[0];
    snprintf(back_gate.entity_id, sizeof(back_gate.entity_id), "binary_sensor.back_gate");
    snprintf(back_gate.label, sizeof(back_gate.label), "Back gate");
    snprintf(back_gate.icon, sizeof(back_gate.icon), "door");
    snprintf(back_gate.abnormal_states, sizeof(back_gate.abnormal_states), "on");
    snprintf(back_gate.normal_label, sizeof(back_gate.normal_label), "Closed");
    snprintf(back_gate.abnormal_label, sizeof(back_gate.abnormal_label), "Open");
    snprintf(back_gate.color, sizeof(back_gate.color), "yellow");
    auto &window = config.security_dynamic_devices[1];
    snprintf(window.entity_id, sizeof(window.entity_id), "binary_sensor.basement_window");
    snprintf(window.label, sizeof(window.label), "Basement window");
    snprintf(window.icon, sizeof(window.icon), "window");
    snprintf(window.abnormal_states, sizeof(window.abnormal_states), "on");
    snprintf(window.normal_label, sizeof(window.normal_label), "Closed");
    snprintf(window.abnormal_label, sizeof(window.abnormal_label), "Open");
    snprintf(window.color, sizeof(window.color), "red");

    entity(0, "alarm_control_panel.alarmo", "Home Alarm", "alarm_control_panel", "disarmed");
    entity(1, "binary_sensor.front_door", "Front door", "binary_sensor", "on");
    entity(2, "binary_sensor.alarm_network", "Alarm network", "binary_sensor", "on");
    entity(3, "binary_sensor.back_gate", "Back gate", "binary_sensor", "on");
    entity(4, "binary_sensor.basement_window", "Basement window", "binary_sensor", "off");
    entity_count = 5;

    SecurityViewModel view = {};
    assert(ui_state_model_snapshot_security(view));
    assert(view.available && strcmp(view.state_label, "DISARMED") == 0);
    assert(view.abnormal_count == 3);  // door + missing garage + attention-only gate
    assert(view.devices[0].abnormal && !view.devices[1].abnormal && view.devices[2].abnormal);
    assert(view.dynamic_device_count == 2);
    assert(view.dynamic_devices[0].abnormal && !view.dynamic_devices[1].abnormal);
    assert(ui_state_model_security_action(view, "away", ""));
    assert(strcmp(queued_mode, "away") == 0 && queued_code[0] == '\0');
    assert(!ui_state_model_security_action(view, "disarm", "1234"));

    snprintf(entities[0].state, sizeof(entities[0].state), "armed_away");
    assert(ui_state_model_snapshot_security(view));
    assert(view.armed && strcmp(view.state_label, "ARMED AWAY") == 0);
    assert(!ui_state_model_security_action(view, "home", ""));
    assert(!ui_state_model_security_action(view, "disarm", ""));
    assert(ui_state_model_security_action(view, "disarm", "1234"));
    assert(strcmp(queued_mode, "disarm") == 0 && strcmp(queued_code, "1234") == 0);

    snprintf(entities[0].state, sizeof(entities[0].state), "pending");
    assert(ui_state_model_snapshot_security(view));
    assert(view.transitioning && view.command_ready);
    assert(ui_state_model_security_action(view, "disarm", "2468"));

    // Dynamic attention must evaluate the expanded collection, not silently
    // truncate it to the original eight-device limit.
    for (uint8_t i = 2; i < 10; ++i) {
        auto &dynamic = config.security_dynamic_devices[i];
        snprintf(dynamic.entity_id, sizeof(dynamic.entity_id),
                 "binary_sensor.dynamic_%u", static_cast<unsigned>(i));
        snprintf(dynamic.label, sizeof(dynamic.label),
                 "Dynamic %u", static_cast<unsigned>(i));
        snprintf(dynamic.icon, sizeof(dynamic.icon), "alert");
        snprintf(dynamic.abnormal_states, sizeof(dynamic.abnormal_states), "on");
        snprintf(dynamic.normal_label, sizeof(dynamic.normal_label), "Normal");
        snprintf(dynamic.abnormal_label, sizeof(dynamic.abnormal_label), "Attention");
        snprintf(dynamic.color, sizeof(dynamic.color), "red");
    }
    config.security_dynamic_device_count = 10;
    assert(ui_state_model_snapshot_security(view));
    assert(view.dynamic_device_count == 10);
    assert(view.abnormal_count == 11);  // prior three plus eight unavailable entries
    std::cout << "Security state model and Alarmo action tests passed.\n";
}
