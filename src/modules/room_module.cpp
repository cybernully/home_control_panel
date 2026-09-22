#include "room_module.h"

#include "config_service.h"
#include "module_ui.h"

#include <Arduino.h>
#include <string.h>

using namespace module_ui;

namespace {

const char *action_text(const HomeAssistantEntitySnapshot &entity) {
    if (strcmp(entity.domain, "cover") == 0) {
        const bool open = strcmp(entity.state, "open") == 0 ||
                          strcmp(entity.state, "opening") == 0;
        return open ? "CLOSE" : "OPEN";
    }

    return strcmp(entity.state, "on") == 0 ? "TURN OFF" : "TURN ON";
}

bool active_state(const HomeAssistantEntitySnapshot &entity) {
    if (strcmp(entity.domain, "cover") == 0) {
        return strcmp(entity.state, "open") == 0 ||
               strcmp(entity.state, "opening") == 0;
    }
    return strcmp(entity.state, "on") == 0;
}

void format_detail(const HomeAssistantEntitySnapshot &entity, char *out, size_t out_len) {
    if (strcmp(entity.domain, "light") == 0 && entity.supports_brightness) {
        snprintf(out, out_len, "%s | %s | %u%%",
                 entity.domain, entity.state,
                 static_cast<unsigned>(entity.brightness_pct));
    } else if (strcmp(entity.domain, "cover") == 0 && entity.supports_position) {
        snprintf(out, out_len, "%s | %s | %d%%",
                 entity.domain, entity.state,
                 static_cast<int>(entity.position_pct));
    } else {
        snprintf(out, out_len, "%s | %s",
                 entity.domain[0] ? entity.domain : "entity",
                 entity.state[0] ? entity.state : "unknown");
    }
}

}  // namespace

void RoomModule::create(lv_obj_t *parent) {
    box(parent, BG, 0, 0);

    const PanelConfig &cfg = config_service_get();
    char sub[112];
    snprintf(sub, sizeof(sub), "Live Home Assistant controls for area: %s",
             cfg.area_id[0] ? cfg.area_id : "not configured");
    module_ui::title(parent, "Room Controls", sub);
    add_live_badge(parent);

    for (int i = 0; i < HA_MAX_ROOM_CONTROLS; ++i) {
        controls_[i].owner = this;
        const int x = 24 + i * 306;
        lv_obj_t *card_obj = card(parent, x, 92, 286, 176);

        controls_[i].name = label(card_obj, "Discovering...", &lv_font_montserrat_18, TEXT);
        lv_obj_set_pos(controls_[i].name, 16, 16);

        controls_[i].detail = label(card_obj, "Waiting for Home Assistant", &lv_font_montserrat_12, MUTED);
        lv_obj_set_pos(controls_[i].detail, 16, 47);

        controls_[i].button = button(card_obj, "WAIT", 16, 92, 254, 58, CARD_ALT);
        controls_[i].button_label = lv_obj_get_child(controls_[i].button, 0);
        set_enabled(controls_[i].button, false);
        lv_obj_add_event_cb(controls_[i].button, toggle_cb, LV_EVENT_CLICKED, &controls_[i]);
    }

    lv_obj_t *brightness = card(parent, 24, 288, 590, 198);
    lv_obj_t *bt = label(brightness, "Area light brightness", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(bt, 18, 18);

    brightness_label_ = label(brightness, "--", &lv_font_montserrat_24, TEXT);
    lv_obj_align(brightness_label_, LV_ALIGN_TOP_RIGHT, -18, 14);

    brightness_slider_ = lv_slider_create(brightness);
    lv_obj_set_pos(brightness_slider_, 24, 82);
    lv_obj_set_size(brightness_slider_, 542, 28);
    lv_slider_set_range(brightness_slider_, 0, 100);
    lv_slider_set_value(brightness_slider_, 0, LV_ANIM_OFF);
    style_slider(brightness_slider_);
    set_enabled(brightness_slider_, false);
    lv_obj_add_event_cb(brightness_slider_, brightness_pressed_cb, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(brightness_slider_, brightness_changed_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(brightness_slider_, brightness_released_cb, LV_EVENT_RELEASED, this);

    status_label_ = label(brightness, "Connecting to Home Assistant...", &lv_font_montserrat_12, MUTED);
    lv_obj_set_width(status_label_, 548);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(status_label_, 18, 142);

    lv_obj_t *scenes_card = card(parent, 634, 288, 618, 198);
    lv_obj_t *st = label(scenes_card, "Area scenes", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(st, 18, 18);

    for (int i = 0; i < HA_MAX_AREA_SCENES; ++i) {
        scenes_[i].owner = this;
        scenes_[i].button = button(scenes_card, "Waiting...", 18 + i * 194, 62, 178, 60, CARD_ALT);
        scenes_[i].label = lv_obj_get_child(scenes_[i].button, 0);
        set_enabled(scenes_[i].button, false);
        lv_obj_add_event_cb(scenes_[i].button, scene_cb, LV_EVENT_CLICKED, &scenes_[i]);
    }

    lv_obj_t *hint = label(scenes_card,
                           "Scenes assigned to this Home Assistant area appear automatically.",
                           &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(hint, 18, 150);
}

void RoomModule::update() {
    HomeAssistantEntitySnapshot entities[HA_MAX_ROOM_CONTROLS] = {};
    const size_t count = home_assistant_get_room_controls(entities, HA_MAX_ROOM_CONTROLS);

    for (size_t i = 0; i < HA_MAX_ROOM_CONTROLS; ++i) {
        ControlCard &control = controls_[i];

        if (i >= count) {
            control.bound = false;
            control.entity_id[0] = '\0';
            control.domain[0] = '\0';
            lv_label_set_text(control.name, i == 0 ? "No controls yet" : "Available slot");
            lv_label_set_text(control.detail, "Waiting for area discovery");
            lv_label_set_text(control.button_label, "WAIT");
            set_enabled(control.button, false);
            lv_obj_set_style_bg_color(control.button, lv_color_hex(CARD_ALT), LV_PART_MAIN);
            continue;
        }

        const HomeAssistantEntitySnapshot &entity = entities[i];
        control.bound = true;
        snprintf(control.entity_id, sizeof(control.entity_id), "%s", entity.entity_id);
        snprintf(control.domain, sizeof(control.domain), "%s", entity.domain);

        lv_label_set_text(control.name, entity.name);

        char detail[112];
        format_detail(entity, detail, sizeof(detail));
        lv_label_set_text(control.detail, detail);

        lv_label_set_text(control.button_label, entity.available ? action_text(entity) : "UNAVAILABLE");
        set_enabled(control.button, entity.available);
        lv_obj_set_style_bg_color(control.button,
                                  lv_color_hex(active_state(entity) ? ACCENT : CARD_ALT),
                                  LV_PART_MAIN);
    }

    HomeAssistantLightStats lights = {};
    home_assistant_get_light_stats(lights);
    const bool lights_available = lights.total > 0;
    set_enabled(brightness_slider_, lights_available);

    if (!brightness_dragging_) {
        const int value = lights_available ? lights.average_brightness_pct : 0;
        lv_slider_set_value(brightness_slider_, value, LV_ANIM_OFF);

        char text[20];
        if (lights_available) {
            snprintf(text, sizeof(text), "%u%%", static_cast<unsigned>(value));
        } else {
            snprintf(text, sizeof(text), "--");
        }
        lv_label_set_text(brightness_label_, text);
    }

    HomeAssistantEntitySnapshot scenes[HA_MAX_AREA_SCENES] = {};
    const size_t scene_count = home_assistant_get_area_scenes(scenes, HA_MAX_AREA_SCENES);

    for (size_t i = 0; i < HA_MAX_AREA_SCENES; ++i) {
        SceneControl &scene = scenes_[i];
        if (i < scene_count) {
            scene.bound = true;
            snprintf(scene.entity_id, sizeof(scene.entity_id), "%s", scenes[i].entity_id);
            lv_label_set_text(scene.label, scenes[i].name);
            set_enabled(scene.button, scenes[i].available);
        } else {
            scene.bound = false;
            scene.entity_id[0] = '\0';
            lv_label_set_text(scene.label, i == 0 ? "No scenes" : "Available slot");
            set_enabled(scene.button, false);
        }
    }

    if (status_label_) {
        HomeAssistantDiscoveryStatus discovery = {};
        home_assistant_get_discovery_status(discovery);

        char status[196];
        if (discovery.last_action_ms) {
            snprintf(status, sizeof(status), "%s | Last: %s",
                     discovery.message[0] ? discovery.message : "Home Assistant",
                     discovery.last_action[0] ? discovery.last_action : "none");
        } else {
            snprintf(status, sizeof(status), "%s",
                     discovery.message[0] ? discovery.message : "Waiting for Home Assistant");
        }
        lv_label_set_text(status_label_, status);
    }
}

void RoomModule::toggle_cb(lv_event_t *e) {
    auto *control = static_cast<ControlCard *>(lv_event_get_user_data(e));
    if (!control || !control->owner || !control->bound || !control->entity_id[0]) return;

    const bool queued = home_assistant_queue_toggle(control->entity_id);
    if (control->owner->status_label_) {
        lv_label_set_text(control->owner->status_label_,
                          queued ? "Command queued; waiting for Home Assistant state update."
                                 : "Could not queue Home Assistant command.");
    }
}

void RoomModule::brightness_pressed_cb(lv_event_t *e) {
    auto *self = static_cast<RoomModule *>(lv_event_get_user_data(e));
    if (self) self->brightness_dragging_ = true;
}

void RoomModule::brightness_changed_cb(lv_event_t *e) {
    auto *self = static_cast<RoomModule *>(lv_event_get_user_data(e));
    if (!self || !self->brightness_label_) return;

    const int value = static_cast<int>(
        lv_slider_get_value(static_cast<lv_obj_t *>(lv_event_get_target(e))));
    char text[16];
    snprintf(text, sizeof(text), "%d%%", value);
    lv_label_set_text(self->brightness_label_, text);
}

void RoomModule::brightness_released_cb(lv_event_t *e) {
    auto *self = static_cast<RoomModule *>(lv_event_get_user_data(e));
    if (!self) return;

    self->brightness_dragging_ = false;
    const int value = static_cast<int>(
        lv_slider_get_value(static_cast<lv_obj_t *>(lv_event_get_target(e))));

    const bool queued = home_assistant_queue_area_brightness(
        static_cast<uint8_t>(constrain(value, 0, 100)));

    if (self->status_label_) {
        lv_label_set_text(self->status_label_,
                          queued ? "Brightness command queued; waiting for Home Assistant."
                                 : "Could not queue brightness command.");
    }
}

void RoomModule::scene_cb(lv_event_t *e) {
    auto *scene = static_cast<SceneControl *>(lv_event_get_user_data(e));
    if (!scene || !scene->owner || !scene->bound || !scene->entity_id[0]) return;

    const bool queued = home_assistant_queue_scene(scene->entity_id);
    if (scene->owner->status_label_) {
        lv_label_set_text(scene->owner->status_label_,
                          queued ? "Scene command queued; waiting for Home Assistant."
                                 : "Could not queue scene command.");
    }
}
