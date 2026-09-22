#include "overview_module.h"

#include "config_service.h"
#include "home_assistant.h"
#include "module_ui.h"

#include <Arduino.h>

using namespace module_ui;

namespace {

void add_metric(lv_obj_t *parent, int x, const char *name, const char *value,
                lv_obj_t **value_out) {
    lv_obj_t *card_obj = card(parent, x, 92, 286, 145);
    lv_obj_t *n = label(card_obj, name, &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(n, 18, 18);
    lv_obj_t *v = label(card_obj, value, &lv_font_montserrat_24, TEXT);
    lv_obj_set_pos(v, 18, 52);
    lv_obj_set_width(v, 250);
    lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
    if (value_out) *value_out = v;
}

}  // namespace

void OverviewModule::create(lv_obj_t *parent) {
    box(parent, BG, 0, 0);

    const PanelConfig &cfg = config_service_get();
    char sub[120];
    snprintf(sub, sizeof(sub), "At-a-glance status for %s", cfg.display_name);
    module_ui::title(parent, "Overview", sub);
    add_live_badge(parent);

    add_metric(parent, 24,  "AREA", cfg.area_id[0] ? cfg.area_id : "Not set", &area_value_);
    add_metric(parent, 322, "LIGHTS", "Discovering...", &lights_value_);
    add_metric(parent, 620, "CLIMATE", "Planned 1.5", &climate_value_);
    add_metric(parent, 918, "HOME ASSISTANT", "Connecting...", &ha_value_);

    lv_obj_t *quick = card(parent, 24, 258, 1228, 250);
    lv_obj_t *qh = label(quick, "Quick actions", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(qh, 18, 16);

    lv_obj_t *qs = label(
        quick,
        "Area lights and discovered Home Assistant scenes are live in 1.2.0.",
        &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(qs, 18, 46);

    for (int i = 0; i < 4; ++i) {
        actions_[i].owner = this;
        actions_[i].is_all_lights = i == 0;
        const char *initial = i == 0 ? "All Lights" : "Waiting...";
        actions_[i].button = button(quick, initial, 18 + i * 295, 86, 276, 82,
                                    i == 0 ? ACCENT : CARD_ALT);
        actions_[i].label = lv_obj_get_child(actions_[i].button, 0);
        lv_obj_add_event_cb(actions_[i].button, action_cb, LV_EVENT_CLICKED, &actions_[i]);
        if (i > 0) set_enabled(actions_[i].button, false);
    }

    action_status_ = label(quick, "Waiting for Home Assistant discovery.",
                           &lv_font_montserrat_16, MUTED);
    lv_obj_set_width(action_status_, 1180);
    lv_label_set_long_mode(action_status_, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(action_status_, 18, 194);
}

void OverviewModule::update() {
    const PanelConfig &cfg = config_service_get();
    if (area_value_) lv_label_set_text(area_value_, cfg.area_id[0] ? cfg.area_id : "Not set");

    HomeAssistantLightStats lights = {};
    home_assistant_get_light_stats(lights);

    if (lights_value_) {
        char text[48];
        if (lights.total == 0) {
            snprintf(text, sizeof(text), "None found");
        } else {
            snprintf(text, sizeof(text), "%u on / %u",
                     static_cast<unsigned>(lights.on),
                     static_cast<unsigned>(lights.total));
        }
        lv_label_set_text(lights_value_, text);
    }

    QuickAction &all_lights = actions_[0];
    all_lights.bound = lights.total > 0;
    set_enabled(all_lights.button, all_lights.bound);
    if (all_lights.bound) {
        lv_label_set_text(all_lights.label, lights.on > 0 ? "Turn All Off" : "Turn All On");
        lv_obj_set_style_bg_color(all_lights.button,
                                  lv_color_hex(lights.on > 0 ? ACCENT : CARD_ALT),
                                  LV_PART_MAIN);
    } else {
        lv_label_set_text(all_lights.label, "No Lights");
        lv_obj_set_style_bg_color(all_lights.button, lv_color_hex(CARD_ALT), LV_PART_MAIN);
    }

    HomeAssistantEntitySnapshot scenes[HA_MAX_AREA_SCENES] = {};
    const size_t scene_count = home_assistant_get_area_scenes(scenes, HA_MAX_AREA_SCENES);

    for (size_t i = 0; i < HA_MAX_AREA_SCENES; ++i) {
        QuickAction &action = actions_[i + 1];
        if (i < scene_count) {
            action.bound = true;
            snprintf(action.entity_id, sizeof(action.entity_id), "%s", scenes[i].entity_id);
            lv_label_set_text(action.label, scenes[i].name);
            set_enabled(action.button, scenes[i].available);
        } else {
            action.bound = false;
            action.entity_id[0] = '\0';
            lv_label_set_text(action.label, i == 0 ? "No Scenes" : "Available Slot");
            set_enabled(action.button, false);
        }
    }

    HomeAssistantStatus health = {};
    home_assistant_get_status(health);
    HomeAssistantDiscoveryStatus discovery = {};
    home_assistant_get_discovery_status(discovery);

    if (ha_value_) {
        char text[64];
        if (discovery.discovery_complete) {
            snprintf(text, sizeof(text), "Live | %u entities",
                     static_cast<unsigned>(discovery.entity_count));
        } else if (discovery.websocket_authenticated) {
            snprintf(text, sizeof(text), "Discovering...");
        } else if (health.configured) {
            snprintf(text, sizeof(text), "Connecting...");
        } else {
            snprintf(text, sizeof(text), "Not configured");
        }
        lv_label_set_text(ha_value_, text);
    }

    if (action_status_) {
        char text[196];
        if (discovery.last_action_ms && discovery.last_action[0]) {
            snprintf(text, sizeof(text), "%s", discovery.last_action);
        } else {
            snprintf(text, sizeof(text), "%s",
                     discovery.message[0] ? discovery.message : "Waiting for Home Assistant.");
        }
        lv_label_set_text(action_status_, text);
    }
}

void OverviewModule::action_cb(lv_event_t *e) {
    auto *action = static_cast<QuickAction *>(lv_event_get_user_data(e));
    if (!action || !action->owner || !action->bound) return;

    bool queued = false;

    if (action->is_all_lights) {
        HomeAssistantLightStats lights = {};
        home_assistant_get_light_stats(lights);
        queued = lights.total > 0 && home_assistant_queue_all_lights(lights.on == 0);
    } else if (action->entity_id[0]) {
        queued = home_assistant_queue_scene(action->entity_id);
    }

    if (action->owner->action_status_) {
        lv_label_set_text(action->owner->action_status_,
                          queued ? "Command queued; waiting for Home Assistant state update."
                                 : "Could not queue Home Assistant command.");
    }
}
