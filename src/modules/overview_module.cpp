#include "overview_module.h"
#include "config_service.h"
#include "home_assistant.h"
#include "module_ui.h"
#include <Arduino.h>

using namespace module_ui;

namespace {
void add_metric(lv_obj_t *parent, int x, const char *name, const char *value, lv_obj_t **value_out) {
    lv_obj_t *c = card(parent, x, 92, 286, 145);
    lv_obj_t *n = label(c, name, &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(n, 18, 18);
    lv_obj_t *v = label(c, value, &lv_font_montserrat_24, TEXT);
    lv_obj_set_pos(v, 18, 52);
    if (value_out) *value_out = v;
}
}

void OverviewModule::create(lv_obj_t *parent) {
    box(parent, BG, 0, 0);
    const PanelConfig &cfg = config_service_get();
    char sub[120];
    snprintf(sub, sizeof(sub), "At-a-glance status for %s", cfg.display_name);
    module_ui::title(parent, "Overview", sub);

    add_metric(parent, 24,  "AREA", cfg.area_id[0] ? cfg.area_id : "Not set", &area_value_);
    add_metric(parent, 322, "LIGHTS", "3 active", nullptr);
    add_metric(parent, 620, "CLIMATE", "72 F", nullptr);
    add_metric(parent, 918, "HOME ASSISTANT", "Checking...", &ha_value_);

    lv_obj_t *quick = card(parent, 24, 258, 1228, 250);
    lv_obj_t *qh = label(quick, "Quick actions", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(qh, 18, 16);
    lv_obj_t *qs = label(quick, "Touch controls are live locally; Home Assistant entity binding comes next.", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(qs, 18, 46);

    const char *names[] = {"All Lights", "Movie", "Good Night", "Away"};
    for (int i = 0; i < 4; ++i) {
        lv_obj_t *b = button(quick, names[i], 18 + i * 295, 86, 276, 82, i == 0 ? ACCENT : CARD_ALT);
        lv_obj_add_event_cb(b, action_cb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(b, const_cast<char *>(names[i]));
    }
    action_status_ = label(quick, "Ready", &lv_font_montserrat_16, MUTED);
    lv_obj_set_pos(action_status_, 18, 194);
}

void OverviewModule::action_cb(lv_event_t *e) {
    auto *self = static_cast<OverviewModule *>(lv_event_get_user_data(e));
    if (!self || !self->action_status_) return;
    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    const char *name = static_cast<const char *>(lv_obj_get_user_data(target));
    char msg[128];
    snprintf(msg, sizeof(msg), "%s selected - UI confirmed; HA service binding is not enabled yet.", name ? name : "Action");
    lv_label_set_text(self->action_status_, msg);
}

void OverviewModule::update() {
    if (area_value_) {
        const PanelConfig &cfg = config_service_get();
        lv_label_set_text(area_value_, cfg.area_id[0] ? cfg.area_id : "Not set");
    }
    if (ha_value_) {
        HomeAssistantStatus ha = {};
        home_assistant_get_status(ha);
        if (ha.authenticated) lv_label_set_text(ha_value_, "Connected");
        else if (ha.configured) lv_label_set_text(ha_value_, "Offline");
        else lv_label_set_text(ha_value_, "Not configured");
    }
}
