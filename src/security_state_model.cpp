#include "ui_state_model.h"

#include "config_service.h"
#include "home_assistant.h"

#include <stdio.h>
#include <string.h>

namespace {
void copy_text(char *out, size_t out_len, const char *value) {
    if (out && out_len) snprintf(out, out_len, "%s", value ? value : "");
}

char lower_ascii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

bool equal_ci(const char *left, const char *right, size_t count) {
    for (size_t i = 0; i < count; ++i)
        if (lower_ascii(left[i]) != lower_ascii(right[i])) return false;
    return true;
}

bool csv_contains(const char *csv, const char *state) {
    if (!csv || !csv[0] || !state || !state[0]) return false;
    const size_t state_len = strlen(state);
    const char *cursor = csv;
    while (*cursor) {
        while (*cursor == ' ' || *cursor == ',') ++cursor;
        const char *end = cursor;
        while (*end && *end != ',') ++end;
        const char *trimmed = end;
        while (trimmed > cursor && trimmed[-1] == ' ') --trimmed;
        const size_t len = static_cast<size_t>(trimmed - cursor);
        if (len == state_len && equal_ci(cursor, state, len)) return true;
        cursor = end;
    }
    return false;
}

void describe_alarm(SecurityViewModel &out) {
    const char *state = out.alarm_state;
    if (!out.configured) {
        copy_text(out.state_label, sizeof(out.state_label), "NOT CONFIGURED");
        copy_text(out.state_detail, sizeof(out.state_detail), "Choose the Alarmo alarm entity in Web Admin");
    } else if (!out.available) {
        copy_text(out.state_label, sizeof(out.state_label), "UNAVAILABLE");
        copy_text(out.state_detail, sizeof(out.state_detail), "Waiting for live Alarmo state");
    } else if (strcmp(state, "disarmed") == 0) {
        copy_text(out.state_label, sizeof(out.state_label), "DISARMED");
        copy_text(out.state_detail, sizeof(out.state_detail), "Ready to arm");
    } else if (strcmp(state, "armed_home") == 0) {
        copy_text(out.state_label, sizeof(out.state_label), "ARMED HOME");
        copy_text(out.state_detail, sizeof(out.state_detail), "Perimeter protection is active");
        out.armed = true;
    } else if (strcmp(state, "armed_away") == 0) {
        copy_text(out.state_label, sizeof(out.state_label), "ARMED AWAY");
        copy_text(out.state_detail, sizeof(out.state_detail), "Full protection is active");
        out.armed = true;
    } else if (strcmp(state, "armed_night") == 0) {
        copy_text(out.state_label, sizeof(out.state_label), "ARMED NIGHT");
        copy_text(out.state_detail, sizeof(out.state_detail), "Night protection is active");
        out.armed = true;
    } else if (strcmp(state, "armed_vacation") == 0) {
        copy_text(out.state_label, sizeof(out.state_label), "ARMED VACATION");
        copy_text(out.state_detail, sizeof(out.state_detail), "Vacation protection is active");
        out.armed = true;
    } else if (strcmp(state, "armed_custom_bypass") == 0) {
        copy_text(out.state_label, sizeof(out.state_label), "ARMED CUSTOM");
        copy_text(out.state_detail, sizeof(out.state_detail), "Custom bypass protection is active");
        out.armed = true;
    } else if (strcmp(state, "triggered") == 0) {
        copy_text(out.state_label, sizeof(out.state_label), "ALARM TRIGGERED");
        copy_text(out.state_detail, sizeof(out.state_detail), "Check the property and disarm when safe");
        out.triggered = true;
        out.armed = true;
    } else if (strcmp(state, "arming") == 0) {
        copy_text(out.state_label, sizeof(out.state_label), "ARMING");
        copy_text(out.state_detail, sizeof(out.state_detail), "Exit delay is in progress");
        out.transitioning = true;
    } else if (strcmp(state, "pending") == 0) {
        copy_text(out.state_label, sizeof(out.state_label), "ENTRY DELAY");
        copy_text(out.state_detail, sizeof(out.state_detail), "Enter the code to disarm");
        out.transitioning = true;
        out.armed = true;
    } else {
        copy_text(out.state_label, sizeof(out.state_label), state[0] ? state : "UNKNOWN");
        copy_text(out.state_detail, sizeof(out.state_detail), "Alarmo reported an unrecognized state");
    }
}
}

bool ui_state_model_security_state_is_abnormal(const char *state,
                                                const char *abnormal_states,
                                                bool reverse_abnormal) {
    const bool listed = csv_contains(abnormal_states, state);
    return reverse_abnormal ? !listed : listed;
}

bool ui_state_model_snapshot_security(SecurityViewModel &out) {
    memset(&out, 0, sizeof(out));
    const PanelConfig &cfg = config_service_get();
    copy_text(out.alarm_entity_id, sizeof(out.alarm_entity_id), cfg.alarm_entity_id);
    out.configured = cfg.alarm_entity_id[0] != '\0';
    out.show_abnormal_summary = cfg.security_show_abnormal_summary;
    out.confirm_arming = cfg.security_confirm_arming;
    out.code_to_arm = cfg.security_code_to_arm;
    out.arm_home = cfg.security_arm_home;
    out.arm_away = cfg.security_arm_away;
    out.arm_night = cfg.security_arm_night;
    out.arm_vacation = cfg.security_arm_vacation;

    HomeAssistantEntitySnapshot alarm = {};
    if (out.configured && home_assistant_get_entity(cfg.alarm_entity_id, alarm)) {
        out.available = alarm.available && strcmp(alarm.domain, "alarm_control_panel") == 0;
        copy_text(out.alarm_name, sizeof(out.alarm_name), alarm.name);
        copy_text(out.alarm_state, sizeof(out.alarm_state), alarm.state);
    } else {
        copy_text(out.alarm_name, sizeof(out.alarm_name), "Alarmo");
    }
    describe_alarm(out);
    // Entry-delay/pending states must still accept an immediate disarm. Mode
    // eligibility is enforced separately by ui_state_model_security_action().
    out.command_ready = out.available && home_assistant_commands_ready();

    out.device_count = cfg.security_device_count > PANEL_MAX_SECURITY_DEVICES ?
        PANEL_MAX_SECURITY_DEVICES : cfg.security_device_count;
    for (uint8_t i = 0; i < out.device_count; ++i) {
        const PanelSecurityDevice &source = cfg.security_devices[i];
        SecurityDeviceViewModel &device = out.devices[i];
        copy_text(device.entity_id, sizeof(device.entity_id), source.entity_id);
        copy_text(device.title, sizeof(device.title), source.label);
        copy_text(device.icon, sizeof(device.icon), source.icon);
        copy_text(device.color, sizeof(device.color), source.color);
        HomeAssistantEntitySnapshot entity = {};
        const bool found = home_assistant_get_entity(source.entity_id, entity);
        device.available = found && entity.available;
        copy_text(device.raw_state, sizeof(device.raw_state), found ? entity.state : "unavailable");
        device.abnormal = !device.available ||
            ui_state_model_security_state_is_abnormal(entity.state, source.abnormal_states,
                                                       source.reverse_abnormal);
        if (!device.available) copy_text(device.state_text, sizeof(device.state_text), "Unavailable");
        else if (device.abnormal && source.abnormal_label[0])
            copy_text(device.state_text, sizeof(device.state_text), source.abnormal_label);
        else if (!device.abnormal && source.normal_label[0])
            copy_text(device.state_text, sizeof(device.state_text), source.normal_label);
        else copy_text(device.state_text, sizeof(device.state_text), entity.state);
        if (device.abnormal) ++out.abnormal_count;
    }

    if (!out.device_count)
        copy_text(out.abnormal_summary, sizeof(out.abnormal_summary), "No monitored devices configured");
    else if (!out.abnormal_count)
        copy_text(out.abnormal_summary, sizeof(out.abnormal_summary), "All monitored devices are normal");
    else
        snprintf(out.abnormal_summary, sizeof(out.abnormal_summary), "%u device%s need%s attention",
                 static_cast<unsigned>(out.abnormal_count), out.abnormal_count == 1 ? "" : "s",
                 out.abnormal_count == 1 ? "s" : "");
    return out.configured;
}

bool ui_state_model_security_action(const SecurityViewModel &security,
                                    const char *mode, const char *code) {
    if (!security.command_ready || !mode || !mode[0]) return false;
    const bool disarm = strcmp(mode, "disarm") == 0;
    if (disarm) {
        if (strcmp(security.alarm_state, "disarmed") == 0 || !code || !code[0]) return false;
    } else {
        if (strcmp(security.alarm_state, "disarmed") != 0) return false;
        const bool enabled = (strcmp(mode, "home") == 0 && security.arm_home) ||
                             (strcmp(mode, "away") == 0 && security.arm_away) ||
                             (strcmp(mode, "night") == 0 && security.arm_night) ||
                             (strcmp(mode, "vacation") == 0 && security.arm_vacation);
        if (!enabled || (security.code_to_arm && (!code || !code[0]))) return false;
    }
    return home_assistant_queue_alarm(security.alarm_entity_id, mode, code ? code : "");
}
