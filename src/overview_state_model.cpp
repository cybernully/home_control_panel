#include "ui_state_model.h"

#include "config_service.h"
#include "home_assistant.h"
#include "network_service.h"

#include <algorithm>
#include <stdio.h>
#include <string.h>

namespace {
void copy_text(char *out, size_t out_len, const char *value) {
    if (out && out_len) snprintf(out, out_len, "%s", value ? value : "");
}

bool equal_ci(const char *left, const char *right, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        char a = left[i], b = right[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

bool csv_contains_state(const char *csv, const char *state) {
    if (!csv || !csv[0] || !state) return false;
    const char *cursor = csv;
    while (*cursor) {
        while (*cursor == ' ' || *cursor == ',') ++cursor;
        const char *end = cursor;
        while (*end && *end != ',') ++end;
        const char *trimmed_end = end;
        while (trimmed_end > cursor && trimmed_end[-1] == ' ') --trimmed_end;
        const size_t len = static_cast<size_t>(trimmed_end - cursor);
        if (len == strlen(state) && equal_ci(cursor, state, len)) return true;
        cursor = end;
    }
    return false;
}

void build_entity(OverviewCardViewModel &out, const PanelOverviewItem &item) {
    HomeAssistantEntitySnapshot entity = {};
    const bool found = home_assistant_get_entity(item.entity_id, entity);
    out.available = found && entity.available;
    out.active = out.available && csv_contains_state(item.active_states, entity.state);
    out.actionable = out.available && strcmp(item.action, "none") != 0;
    if (!out.available) copy_text(out.state_text, sizeof(out.state_text), "Unavailable");
    else if (out.active && item.active_label[0]) copy_text(out.state_text, sizeof(out.state_text), item.active_label);
    else if (!out.active && item.inactive_label[0]) copy_text(out.state_text, sizeof(out.state_text), item.inactive_label);
    else if (strcmp(entity.domain, "scene") == 0) copy_text(out.state_text, sizeof(out.state_text), "Ready");
    else copy_text(out.state_text, sizeof(out.state_text), entity.state);
}
}

size_t ui_state_model_snapshot_overview(OverviewCardViewModel *cards, size_t capacity) {
    if (!cards || !capacity) return 0;
    const PanelConfig &cfg = config_service_get();
    const size_t count = std::min<size_t>(cfg.overview_item_count, capacity);
    HomeAssistantLightStats lights = {};
    HomeAssistantStatus health = {};
    HomeAssistantDiscoveryStatus discovery = {};
    home_assistant_get_light_stats(lights);
    home_assistant_get_status(health);
    home_assistant_get_discovery_status(discovery);
    for (size_t i = 0; i < count; ++i) {
        const PanelOverviewItem &item = cfg.overview_items[i];
        OverviewCardViewModel &out = cards[i];
        memset(&out, 0, sizeof(out));
        copy_text(out.type, sizeof(out.type), item.type);
        copy_text(out.entity_id, sizeof(out.entity_id), item.entity_id);
        copy_text(out.title, sizeof(out.title), item.label);
        copy_text(out.icon, sizeof(out.icon), item.icon);
        copy_text(out.color, sizeof(out.color), item.color);
        copy_text(out.action, sizeof(out.action), item.action);
        out.span = item.span;
        out.confirm = item.confirm;
        if (strcmp(item.type, "entity") == 0) {
            build_entity(out, item);
        } else if (strcmp(item.type, "home_status") == 0) {
            out.available = health.configured;
            out.active = discovery.discovery_complete && discovery.websocket_authenticated;
            copy_text(out.state_text, sizeof(out.state_text), out.active ? "All systems ready" :
                      health.configured ? "Connecting to Home Assistant" : "Home Assistant not configured");
        } else if (strcmp(item.type, "lights") == 0) {
            out.available = lights.total > 0;
            out.active = lights.on > 0;
            snprintf(out.state_text, sizeof(out.state_text), lights.total ? "%u on / %u total" : "No selected lights",
                     static_cast<unsigned>(lights.on), static_cast<unsigned>(lights.total));
        } else if (strcmp(item.type, "network") == 0) {
            out.available = true;
            out.active = network_service_connected();
            if (out.active) snprintf(out.state_text, sizeof(out.state_text), "Online  |  %d dBm", network_service_rssi());
            else copy_text(out.state_text, sizeof(out.state_text), "Offline");
        } else if (strcmp(item.type, "all_lights") == 0) {
            out.available = lights.total > 0;
            out.active = lights.on > 0;
            out.actionable = out.available;
            copy_text(out.state_text, sizeof(out.state_text), out.active ?
                      (item.active_label[0] ? item.active_label : "Turn all off") :
                      (item.inactive_label[0] ? item.inactive_label : "Turn all on"));
        } else if (strcmp(item.type, "weather") == 0 || strcmp(item.type, "calendar") == 0) {
            PanelOverviewItem entity_item = item;
            const char *fallback_id = strcmp(item.type, "weather") == 0 ? cfg.weather_entity_id : cfg.calendar_entity_id;
            if (!entity_item.entity_id[0]) copy_text(entity_item.entity_id, sizeof(entity_item.entity_id), fallback_id);
            build_entity(out, entity_item);
        } else {
            out.available = true;
            copy_text(out.state_text, sizeof(out.state_text), "Customize cards in Web Admin");
        }
    }
    return count;
}

bool ui_state_model_activate_overview(const OverviewCardViewModel &card) {
    if (!card.available || !card.actionable) return false;
    if (strcmp(card.action, "all_lights") == 0) {
        HomeAssistantLightStats lights = {};
        home_assistant_get_light_stats(lights);
        return lights.total && home_assistant_queue_all_lights(lights.on == 0);
    }
    HomeAssistantEntitySnapshot current = {};
    if (!home_assistant_get_entity(card.entity_id, current) || !current.available) return false;
    if (strcmp(card.action, "scene") == 0) return home_assistant_queue_scene(card.entity_id);
    if (strcmp(card.action, "toggle") == 0) return home_assistant_queue_toggle(card.entity_id);
    return false;
}
