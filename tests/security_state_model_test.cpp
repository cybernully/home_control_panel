#include "config_service.h"
#include "home_assistant.h"
#include "ui_state_model.h"

#include <cassert>
#include <cstring>
#include <iostream>

static PanelConfig config = {};
static HomeAssistantEntitySnapshot entities[4] = {};
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

    entity(0, "alarm_control_panel.alarmo", "Home Alarm", "alarm_control_panel", "disarmed");
    entity(1, "binary_sensor.front_door", "Front door", "binary_sensor", "on");
    entity(2, "binary_sensor.alarm_network", "Alarm network", "binary_sensor", "on");
    entity_count = 3;

    SecurityViewModel view = {};
    assert(ui_state_model_snapshot_security(view));
    assert(view.available && strcmp(view.state_label, "DISARMED") == 0);
    assert(view.abnormal_count == 2);  // open door + missing/unavailable garage
    assert(view.devices[0].abnormal && !view.devices[1].abnormal && view.devices[2].abnormal);
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
    std::cout << "Security state model and Alarmo action tests passed.\n";
}
