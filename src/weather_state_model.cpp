#include "ui_state_model.h"

#include "config_service.h"
#include "home_assistant.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

namespace {
void copy_text(char *out, size_t out_len, const char *value) {
    if (out && out_len) snprintf(out, out_len, "%s", value ? value : "");
}

void friendly_condition(char *out, size_t out_len, const char *condition) {
    struct ConditionLabel { const char *state; const char *label; };
    static const ConditionLabel labels[] = {
        {"clear-night", "Clear Night"}, {"partlycloudy", "Partly Cloudy"},
        {"lightning-rainy", "Thunderstorms"}, {"snowy-rainy", "Wintry Mix"},
        {"windy-variant", "Windy"}, {"exceptional", "Severe Weather"},
        {"pouring", "Heavy Rain"}, {"rainy", "Rainy"}, {"snowy", "Snowy"},
        {"lightning", "Lightning"}, {"cloudy", "Cloudy"}, {"fog", "Fog"},
        {"hail", "Hail"}, {"sunny", "Sunny"}, {"windy", "Windy"}
    };
    for (const auto &entry : labels) {
        if (condition && strcmp(condition, entry.state) == 0) {
            copy_text(out, out_len, entry.label);
            return;
        }
    }
    copy_text(out, out_len, condition && condition[0] ? condition : "Unknown");
    bool word_start = true;
    for (size_t i = 0; out[i]; ++i) {
        if (out[i] == '-' || out[i] == '_') out[i] = ' ';
        if (word_start && out[i] >= 'a' && out[i] <= 'z') out[i] = static_cast<char>(toupper(out[i]));
        word_start = out[i] == ' ';
    }
}

void format_temp(char *out, size_t out_len, bool has_value, float value) {
    if (!has_value) copy_text(out, out_len, "--");
    else snprintf(out, out_len, "%.0f\xC2\xB0", static_cast<double>(value));
}

int64_t days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

bool local_forecast_time(const char *value, struct tm &local) {
    if (!value) return false;
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (sscanf(value, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6 ||
        month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23) return false;
    int64_t epoch = days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * 86400 +
                    hour * 3600 + minute * 60 + second;
    const char *zone = value + 19;
    while (*zone && *zone != 'Z' && *zone != '+' && *zone != '-') ++zone;
    if (*zone == '+' || *zone == '-') {
        int offset_hour = 0, offset_minute = 0;
        if (sscanf(zone + 1, "%d:%d", &offset_hour, &offset_minute) == 2) {
            const int offset = offset_hour * 3600 + offset_minute * 60;
            epoch += *zone == '+' ? -offset : offset;
        }
    }
    const time_t timestamp = static_cast<time_t>(epoch);
    const struct tm *converted = localtime(&timestamp);
    if (!converted) return false;
    local = *converted;
    return true;
}

void format_hour(char *out, size_t out_len, const char *datetime) {
    struct tm local = {};
    int hour = 0;
    if (local_forecast_time(datetime, local)) hour = local.tm_hour;
    else if (datetime && strlen(datetime) >= 13) hour = (datetime[11] - '0') * 10 + (datetime[12] - '0');
    else { copy_text(out, out_len, "Later"); return; }
    if (hour < 0 || hour > 23) { copy_text(out, out_len, "Later"); return; }
    snprintf(out, out_len, "%d %s", hour % 12 ? hour % 12 : 12, hour < 12 ? "AM" : "PM");
}

void format_day(char *out, size_t out_len, const char *datetime, size_t index) {
    static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    struct tm local = {};
    if (local_forecast_time(datetime, local))
        copy_text(out, out_len, index == 0 ? "Today" : days[local.tm_wday]);
    else copy_text(out, out_len, index == 0 ? "Today" : "Later");
}

void build_forecast(WeatherForecastViewModel &out,
                    const HomeAssistantWeatherForecast &source,
                    bool hourly, size_t index) {
    if (hourly) format_hour(out.period, sizeof(out.period), source.datetime);
    else format_day(out.period, sizeof(out.period), source.datetime, index);
    friendly_condition(out.condition, sizeof(out.condition), source.condition);
    if (!hourly && source.has_temperature && source.has_temperature_low)
        snprintf(out.temperature, sizeof(out.temperature), "%.0f\xC2\xB0 / %.0f\xC2\xB0",
                 static_cast<double>(source.temperature), static_cast<double>(source.temperature_low));
    else format_temp(out.temperature, sizeof(out.temperature), source.has_temperature, source.temperature);
    if (source.has_precipitation_probability)
        snprintf(out.detail, sizeof(out.detail), "%u%% rain",
                 static_cast<unsigned>(source.precipitation_probability));
    else out.detail[0] = '\0';
}
}

bool ui_state_model_snapshot_weather(WeatherViewModel &weather) {
    memset(&weather, 0, sizeof(weather));
    const PanelConfig &cfg = config_service_get();
    const char *current_id = cfg.weather_entity_id;
    const char *hourly_id = cfg.weather_hourly_entity_id[0] ?
                            cfg.weather_hourly_entity_id : current_id;
    const char *daily_id = cfg.weather_daily_entity_id[0] ?
                           cfg.weather_daily_entity_id : current_id;
    const char *request_id = current_id[0] ? current_id :
                             hourly_id[0] ? hourly_id : daily_id;
    if (!request_id[0]) {
        copy_text(weather.condition, sizeof(weather.condition), "Select a weather entity");
        return false;
    }
    home_assistant_request_weather_forecasts(request_id);
    HomeAssistantWeatherSnapshot source = {};
    if (!home_assistant_get_weather(current_id, source)) {
        copy_text(weather.entity_name, sizeof(weather.entity_name),
                  current_id[0] ? current_id : request_id);
        copy_text(weather.condition, sizeof(weather.condition), "Waiting for Home Assistant");
        return false;
    }
    weather.available = source.available;
    weather.loading = source.forecasts_loading;
    copy_text(weather.entity_name, sizeof(weather.entity_name),
              source.name[0] ? source.name : current_id[0] ? current_id : request_id);
    friendly_condition(weather.condition, sizeof(weather.condition), source.condition);
    format_temp(weather.temperature, sizeof(weather.temperature), source.has_temperature, source.temperature);
    char feels[24] = {};
    format_temp(feels, sizeof(feels), source.has_apparent_temperature, source.apparent_temperature);
    if (source.has_humidity && source.has_wind_speed)
        snprintf(weather.detail, sizeof(weather.detail), "Feels %s  |  %.0f%% humidity  |  %.0f %s wind",
                 feels, static_cast<double>(source.humidity), static_cast<double>(source.wind_speed),
                 source.wind_speed_unit[0] ? source.wind_speed_unit : "");
    else if (source.has_humidity)
        snprintf(weather.detail, sizeof(weather.detail), "Feels %s  |  %.0f%% humidity",
                 feels, static_cast<double>(source.humidity));
    else if (source.has_wind_speed)
        snprintf(weather.detail, sizeof(weather.detail), "%.0f %s wind",
                 static_cast<double>(source.wind_speed), source.wind_speed_unit);
    else copy_text(weather.detail, sizeof(weather.detail), source.available ? "Live conditions" : "Unavailable");

    weather.hourly_count = source.hourly_count;
    weather.daily_count = source.daily_count;
    for (uint8_t i = 0; i < source.hourly_count; ++i) build_forecast(weather.hourly[i], source.hourly[i], true, i);
    for (uint8_t i = 0; i < source.daily_count; ++i) build_forecast(weather.daily[i], source.daily[i], false, i);
    return true;
}
