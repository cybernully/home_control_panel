#include "overview_module.h"
#include "config_service.h"
#include "home_assistant.h"
#include <lvgl.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

static PanelConfig config = {};
static HomeAssistantEntitySnapshot entities[8] = {};
static size_t entity_count = 0;
static std::string action_target;
static int toggle_count = 0;
static int all_lights_count = 0;
static HomeAssistantWeatherSnapshot weather_snapshot = {};

const PanelConfig &config_service_get() { return config; }
bool network_service_connected() { return true; }
int network_service_rssi() { return -52; }
void home_assistant_get_status(HomeAssistantStatus &out) { out = {}; out.configured = true; out.authenticated = true; }
void home_assistant_get_discovery_status(HomeAssistantDiscoveryStatus &out) { out = {}; out.websocket_authenticated = true; out.discovery_complete = true; }
void home_assistant_get_light_stats(HomeAssistantLightStats &out) { out = {}; out.total = 4; out.on = 1; }
bool home_assistant_get_entity(const char *id, HomeAssistantEntitySnapshot &out) {
    for (size_t i = 0; i < entity_count; ++i) if (strcmp(id, entities[i].entity_id) == 0) { out = entities[i]; return true; }
    return false;
}
bool home_assistant_request_weather_forecasts(const char *) { return true; }
bool home_assistant_get_weather(const char *, HomeAssistantWeatherSnapshot &out) { out = weather_snapshot; return out.available; }
bool ui_state_model_snapshot_calendar(int16_t, uint8_t, CalendarViewModel &out) {
    memset(&out, 0, sizeof(out));
    snprintf(out.status, sizeof(out.status), "No events");
    out.available = true;
    return true;
}
bool home_assistant_queue_toggle(const char *id) { action_target = id; ++toggle_count; return true; }
bool home_assistant_queue_scene(const char *) { return true; }
bool home_assistant_queue_all_lights(bool) { ++all_lights_count; return true; }

static unsigned char buffer[1280 * 658 * 4];
static void flush(lv_display_t *display, const lv_area_t *, uint8_t *) { lv_display_flush_ready(display); }
static lv_obj_t *find(lv_obj_t *root, const char *text) {
    if (lv_obj_check_type(root, &lv_label_class) && strcmp(lv_label_get_text(root), text) == 0) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) if (auto *result = find(lv_obj_get_child(root, i), text)) return result;
    return nullptr;
}
static void shot(const char *name) {
    lv_refr_now(nullptr); FILE *file = fopen(name, "wb"); assert(file);
    fprintf(file, "P6\n1280 658\n255\n");
    for (int i = 0; i < 1280 * 658; ++i) { fputc(buffer[4*i+2], file); fputc(buffer[4*i+1], file); fputc(buffer[4*i], file); }
    fclose(file);
}
static void add_entity(const char *id, const char *name, const char *domain, const char *state) {
    auto &entity = entities[entity_count++];
    snprintf(entity.entity_id, sizeof(entity.entity_id), "%s", id);
    snprintf(entity.name, sizeof(entity.name), "%s", name);
    snprintf(entity.domain, sizeof(entity.domain), "%s", domain);
    snprintf(entity.state, sizeof(entity.state), "%s", state);
    entity.available = true;
}
static void add_card(const char *type, const char *id, const char *label, const char *icon,
                     const char *action, const char *states, const char *active, const char *inactive,
                     const char *color, int span, bool confirm, const char *action_target = "") {
    auto &item = config.overview_items[config.overview_item_count++];
    snprintf(item.type, sizeof(item.type), "%s", type); snprintf(item.entity_id, sizeof(item.entity_id), "%s", id);
    snprintf(item.action_entity_id, sizeof(item.action_entity_id), "%s", action_target);
    snprintf(item.label, sizeof(item.label), "%s", label); snprintf(item.icon, sizeof(item.icon), "%s", icon);
    snprintf(item.action, sizeof(item.action), "%s", action); snprintf(item.active_states, sizeof(item.active_states), "%s", states);
    snprintf(item.active_label, sizeof(item.active_label), "%s", active); snprintf(item.inactive_label, sizeof(item.inactive_label), "%s", inactive);
    snprintf(item.color, sizeof(item.color), "%s", color); item.span = span; item.confirm = confirm;
}

int main() {
    lv_init(); auto *display = lv_display_create(1280, 658); lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display, buffer, nullptr, sizeof(buffer), LV_DISPLAY_RENDER_MODE_FULL); lv_display_set_flush_cb(display, flush);
    add_entity("cover.garage", "Garage door", "cover", "open");
    add_entity("binary_sensor.side_door", "Side door", "binary_sensor", "off");
    add_entity("switch.hall_light", "Hall light", "switch", "off");
    add_entity("lock.front", "Front lock", "lock", "locked");
    add_entity("timer.office_energy_saver_countdown", "Office energy saver", "timer", "active");
    entities[entity_count - 1].timer_has_remaining = true;
    entities[entity_count - 1].timer_remaining_seconds = 754;
    snprintf(config.weather_entity_id, sizeof(config.weather_entity_id), "weather.home");
    weather_snapshot.available = true; weather_snapshot.has_temperature = true; weather_snapshot.temperature = 72;
    snprintf(weather_snapshot.name, sizeof(weather_snapshot.name), "Home Forecast");
    snprintf(weather_snapshot.condition, sizeof(weather_snapshot.condition), "partlycloudy");
    weather_snapshot.hourly_count = 2;
    for (int i = 0; i < 2; ++i) {
        snprintf(weather_snapshot.hourly[i].datetime, sizeof(weather_snapshot.hourly[i].datetime), "2026-10-01T%02d:00:00-05:00", 13 + i);
        weather_snapshot.hourly[i].has_temperature = true; weather_snapshot.hourly[i].temperature = 72 + i;
        snprintf(weather_snapshot.hourly[i].condition, sizeof(weather_snapshot.hourly[i].condition), "cloudy");
    }
    weather_snapshot.daily_count = 2;
    for (int i = 0; i < 2; ++i) {
        snprintf(weather_snapshot.daily[i].datetime, sizeof(weather_snapshot.daily[i].datetime), "2026-10-%02dT00:00:00-05:00", 1 + i);
        weather_snapshot.daily[i].has_temperature = true; weather_snapshot.daily[i].temperature = 75 + i;
        weather_snapshot.daily[i].has_temperature_low = true; weather_snapshot.daily[i].temperature_low = 55 + i;
        snprintf(weather_snapshot.daily[i].condition, sizeof(weather_snapshot.daily[i].condition), "sunny");
    }
    add_card("home_status", "", "Home", "shield", "none", "", "All good", "Attention", "green", 1, false);
    add_card("entity", "cover.garage", "Garage Door", "garage", "toggle", "open,opening", "Open", "Closed", "yellow", 2, true);
    add_card("entity", "binary_sensor.side_door", "Side Door", "door", "toggle", "on", "Open", "Closed", "yellow", 1, true, "switch.hall_light");
    add_card("entity", "lock.front", "Front Lock", "lock", "toggle", "unlocked,unlocking", "Unlocked", "Locked", "red", 1, true);
    add_card("all_lights", "", "All Lights", "light", "all_lights", "", "Turn all off", "Turn all on", "yellow", 1, true);
    add_card("entity", "timer.office_energy_saver_countdown", "Office energy saver", "timer", "none", "active,paused", "", "", "cyan", 1, false);
    add_card("weather_current", "", "Current Weather", "weather", "none", "", "", "", "cyan", 2, false);
    add_card("weather_hourly", "", "Next Hours", "weather", "none", "", "", "", "cyan", 2, false);
    add_card("weather_daily", "", "Next Days", "weather", "none", "", "", "", "cyan", 2, false);

    OverviewCardViewModel model[12] = {};
    assert(ui_state_model_snapshot_overview(model, 12) == 9);
    assert(model[1].active && strcmp(model[1].state_text, "Open") == 0 && model[1].confirm);
    assert(!model[2].active && model[2].actionable && strcmp(model[2].state_text, "Closed") == 0);
    assert(strcmp(model[2].action_entity_id, "switch.hall_light") == 0);
    assert(model[5].active && strcmp(model[5].state_text, "00:12:34") == 0);
    assert(model[6].active && strstr(model[6].state_text, "72") && strstr(model[6].state_text, "Partly Cloudy"));
    assert(model[7].active && strstr(model[7].state_text, "1 PM") && strstr(model[7].state_text, "2 PM"));
    assert(model[8].active && strstr(model[8].state_text, "Today"));

    auto *root = lv_screen_active(); OverviewModule overview; overview.create(root); lv_obj_update_layout(root);
    assert(find(root, "At a glance") && find(root, "Garage Door") && find(root, "Open") && find(root, "Closed"));
    assert(find(root, "Office energy saver") && find(root, "00:12:34"));
    assert(find(root, "Current Weather") && find(root, "Next Hours") && find(root, "Next Days"));
    auto *garage_card = lv_obj_get_parent(find(root, "Garage Door")); assert(lv_obj_get_height(garage_card) == 120);
    lv_obj_send_event(garage_card, LV_EVENT_CLICKED, nullptr);
    assert(find(root, "Garage Door?") && toggle_count == 0);
    snprintf(entities[0].state, sizeof(entities[0].state), "closed"); overview.update();
    assert(find(root, "Current status: Open\n\nDo you want to continue with this Home Assistant action?"));
    auto *confirm = find(root, "Confirm"); assert(confirm); lv_obj_send_event(lv_obj_get_parent(confirm), LV_EVENT_CLICKED, nullptr);
    assert(toggle_count == 1 && action_target == "cover.garage");
    assert(find(root, "Command queued - waiting for Home Assistant"));
    lv_obj_send_event(lv_obj_get_parent(find(root, "Side Door")), LV_EVENT_CLICKED, nullptr);
    assert(toggle_count == 1);
    assert(find(root, "Current status: Closed\nAction target: switch.hall_light\n\nContinue?"));
    confirm = find(root, "Confirm"); assert(confirm); lv_obj_send_event(lv_obj_get_parent(confirm), LV_EVENT_CLICKED, nullptr);
    assert(toggle_count == 2 && action_target == "switch.hall_light");
    entities[4].timer_remaining_seconds = 733; overview.update();
    assert(find(root, "00:12:13"));
    snprintf(entities[4].state, sizeof(entities[4].state), "idle"); overview.update();
    assert(find(root, "Idle"));
    shot(".test-build/overview-cards.ppm");
    return 0;
}
