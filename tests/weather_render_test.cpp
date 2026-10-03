#include "weather_module.h"
#include "config_service.h"
#include "ui_state_model.h"

#include <lvgl.h>
#include <cassert>
#include <cstdio>
#include <cstring>

static PanelConfig config = {};
static WeatherViewModel weather = {};

const PanelConfig &config_service_get() { return config; }
bool ui_state_model_snapshot_weather(WeatherViewModel &out) { out = weather; return out.available; }

static unsigned char buffer[1280 * 658 * 4];
static void flush(lv_display_t *display, const lv_area_t *, uint8_t *) { lv_display_flush_ready(display); }
static lv_obj_t *find(lv_obj_t *root, const char *text) {
    if (lv_obj_check_type(root, &lv_label_class) && strcmp(lv_label_get_text(root), text) == 0) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto *result = find(lv_obj_get_child(root, i), text)) return result;
    return nullptr;
}
static void shot(const char *name) {
    lv_refr_now(nullptr); FILE *file = fopen(name, "wb"); assert(file);
    fprintf(file, "P6\n1280 658\n255\n");
    for (int i = 0; i < 1280 * 658; ++i) {
        fputc(buffer[4*i+2], file); fputc(buffer[4*i+1], file); fputc(buffer[4*i], file);
    }
    fclose(file);
}

int main() {
    lv_init(); auto *display = lv_display_create(1280, 658);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display, buffer, nullptr, sizeof(buffer), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    snprintf(config.weather_entity_id, sizeof(config.weather_entity_id), "weather.home");
    snprintf(config.weather_hourly_entity_id, sizeof(config.weather_hourly_entity_id), "weather.hourly_station");
    snprintf(config.weather_daily_entity_id, sizeof(config.weather_daily_entity_id), "weather.daily_station");
    snprintf(config.weather_layout, sizeof(config.weather_layout), "balanced");
    config.weather_show_current = config.weather_show_hourly = config.weather_show_daily = true;
    weather.available = true;
    snprintf(weather.entity_name, sizeof(weather.entity_name), "Home Forecast");
    snprintf(weather.temperature, sizeof(weather.temperature), "72\xC2\xB0");
    snprintf(weather.condition, sizeof(weather.condition), "Partly Cloudy");
    snprintf(weather.detail, sizeof(weather.detail), "Feels 71\xC2\xB0 | 45%% humidity | 8 mph wind");
    weather.hourly_count = HA_MAX_WEATHER_HOURLY;
    for (uint8_t i = 0; i < weather.hourly_count; ++i) {
        snprintf(weather.hourly[i].period, sizeof(weather.hourly[i].period), "%u PM", i + 1);
        snprintf(weather.hourly[i].temperature, sizeof(weather.hourly[i].temperature), "%u\xC2\xB0", 72 + i);
        snprintf(weather.hourly[i].condition, sizeof(weather.hourly[i].condition), "Cloudy");
        snprintf(weather.hourly[i].detail, sizeof(weather.hourly[i].detail), "%u%% rain", i * 5);
    }
    weather.daily_count = HA_MAX_WEATHER_DAILY;
    const char *days[] = {"Today", "Fri", "Sat", "Sun", "Mon"};
    for (uint8_t i = 0; i < weather.daily_count; ++i) {
        snprintf(weather.daily[i].period, sizeof(weather.daily[i].period), "%s", days[i]);
        snprintf(weather.daily[i].temperature, sizeof(weather.daily[i].temperature), "%u\xC2\xB0 / %u\xC2\xB0", 75 + i, 55 + i);
        snprintf(weather.daily[i].condition, sizeof(weather.daily[i].condition), "Mostly Sunny");
        snprintf(weather.daily[i].detail, sizeof(weather.daily[i].detail), "%u%% rain", i * 10);
    }
    WeatherModule module; auto *root = lv_screen_active(); module.create(root); lv_obj_update_layout(root);
    assert(find(root, "Weather") && find(root, "CURRENT") && find(root, "HOURLY FORECAST") && find(root, "MULTI-DAY FORECAST"));
    assert(find(root, "72\xC2\xB0") && find(root, "Partly Cloudy") && find(root, "Today"));
    shot(".test-build/weather-balanced.ppm");

    config.weather_show_current = false;
    snprintf(config.weather_layout, sizeof(config.weather_layout), "forecast_focus");
    module.update(); lv_obj_update_layout(root);
    auto *current_card = lv_obj_get_parent(find(root, "CURRENT"));
    auto *hourly_card = lv_obj_get_parent(find(root, "HOURLY FORECAST"));
    auto *daily_card = lv_obj_get_parent(find(root, "MULTI-DAY FORECAST"));
    assert(lv_obj_has_flag(current_card, LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(hourly_card, LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(daily_card, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_get_width(hourly_card) == 1232 && lv_obj_get_width(daily_card) == 1232);
    shot(".test-build/weather-forecast-focus.ppm");
    return 0;
}
