#pragma once
#include "app_config.h"
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

struct HomeAssistantStatus {
    bool configured;
    bool connected;
    bool authenticated;
    bool request_in_progress;
    int http_code;
    uint32_t latency_ms;
    uint32_t last_success_ms;
    char message[96];
};

struct HomeAssistantDiscoveryStatus {
    bool websocket_connected;
    bool websocket_authenticated;
    bool discovery_complete;
    bool area_found;
    uint16_t entity_count;
    uint16_t device_count;
    int last_action_http_code;
    uint32_t last_discovery_ms;
    uint32_t last_state_ms;
    uint32_t last_action_ms;
    char area_id[64];
    char area_name[64];
    char message[128];
    char last_action[96];
};

struct HomeAssistantEntitySnapshot {
    char entity_id[96];
    char name[64];
    // alarm_control_panel is 19 characters; leave room for it and future HA
    // domains without truncating discovery or action routing.
    char domain[24];
    char state[32];
    char unit_of_measurement[16];
    uint8_t brightness_pct;
    int16_t position_pct;
    bool available;
    bool supports_brightness;
    bool supports_position;
    bool supports_fan_speed;
    uint8_t fan_speed_pct;
    bool timer_has_remaining;
    uint32_t timer_remaining_seconds;
};

struct HomeAssistantLightStats {
    uint16_t total;
    uint16_t on;
    uint8_t average_brightness_pct;
};

struct HomeAssistantWeatherForecast {
    char datetime[40];
    char condition[32];
    float temperature;
    float temperature_low;
    uint8_t precipitation_probability;
    bool has_temperature;
    bool has_temperature_low;
    bool has_precipitation_probability;
};

struct HomeAssistantWeatherSnapshot {
    char entity_id[96];
    char name[64];
    char condition[32];
    char temperature_unit[12];
    char wind_speed_unit[16];
    float temperature;
    float apparent_temperature;
    float humidity;
    float wind_speed;
    uint16_t pressure;
    bool available;
    bool has_temperature;
    bool has_apparent_temperature;
    bool has_humidity;
    bool has_wind_speed;
    bool has_pressure;
    bool forecasts_loading;
    uint32_t last_forecast_ms;
    uint8_t hourly_count;
    uint8_t daily_count;
    HomeAssistantWeatherForecast hourly[HA_MAX_WEATHER_HOURLY];
    HomeAssistantWeatherForecast daily[HA_MAX_WEATHER_DAILY];
};

struct HomeAssistantClimateSnapshot {
    char entity_id[96];
    char name[64];
    char hvac_mode[HA_CLIMATE_OPTION_LEN];
    char hvac_action[HA_CLIMATE_OPTION_LEN];
    char fan_mode[HA_CLIMATE_OPTION_LEN];
    char preset_mode[HA_CLIMATE_OPTION_LEN];
    char temperature_unit[12];
    float current_temperature;
    float target_temperature;
    float target_low;
    float target_high;
    float min_temperature;
    float max_temperature;
    float target_step;
    float humidity;
    bool available;
    bool has_current_temperature;
    bool has_target_temperature;
    bool has_target_range;
    bool has_humidity;
    uint8_t hvac_mode_count;
    uint8_t fan_mode_count;
    uint8_t preset_count;
    char hvac_modes[HA_MAX_CLIMATE_MODES][HA_CLIMATE_OPTION_LEN];
    char fan_modes[HA_MAX_CLIMATE_FAN_MODES][HA_CLIMATE_OPTION_LEN];
    char presets[HA_MAX_CLIMATE_PRESETS][HA_CLIMATE_OPTION_LEN];
};

struct HomeAssistantCalendarEvent {
    char calendar_entity_id[96];
    char summary[120];
    char description[512];
    char location[120];
    char start[40];
    char end[40];
    bool all_day;
};

struct HomeAssistantCalendarSnapshot {
    char range_start[40];
    char range_end[40];
    uint32_t last_update_ms;
    uint8_t event_count;
    bool loading;
    bool available;
};


enum class HomeAssistantArtworkFormat : uint8_t {
    None = 0,
    Jpeg = 1,
    Png = 2,
};

struct HomeAssistantMediaSnapshot {
    char entity_id[96];
    char name[64];
    char state[32];
    char title[96];
    char artist[96];
    char album[96];
    char playlist[96];
    char source[64];
    char entity_picture[HA_MEDIA_ARTWORK_URL_LEN];
    uint8_t volume_pct;
    bool volume_muted;
    bool available;
    bool supports_volume;
    bool supports_mute;
    uint8_t source_count;
    char sources[HA_MAX_MEDIA_SOURCES][HA_MEDIA_SOURCE_NAME_LEN];
};

struct HomeAssistantMediaFavorite {
    char entity_id[96];
    char title[64];
    char media_content_id[HA_MEDIA_CONTENT_ID_LEN];
    char media_content_type[HA_MEDIA_CONTENT_TYPE_LEN];
};

struct HomeAssistantMediaArtworkInfo {
    uint32_t generation;
    uint32_t data_size;
    uint16_t width;
    uint16_t height;
    HomeAssistantArtworkFormat format;
    char entity_id[96];
    char picture_url[HA_MEDIA_ARTWORK_URL_LEN];
};

void home_assistant_begin();
void home_assistant_loop();

bool home_assistant_request_health_check();
void home_assistant_get_status(HomeAssistantStatus &out);

bool home_assistant_request_discovery();
// A temporary whole-home entity search used by the web layout editor. It is
// never the normal startup mode once an explicit layout has been saved.
bool home_assistant_request_full_discovery();
void home_assistant_get_discovery_status(HomeAssistantDiscoveryStatus &out);

size_t home_assistant_get_room_controls(HomeAssistantEntitySnapshot *out, size_t max_count);
size_t home_assistant_get_area_scenes(HomeAssistantEntitySnapshot *out, size_t max_count);
void home_assistant_get_light_stats(HomeAssistantLightStats &out);
bool home_assistant_request_weather_forecasts(const char *entity_id);
bool home_assistant_get_weather(const char *entity_id, HomeAssistantWeatherSnapshot &out);
bool home_assistant_get_climate(const char *entity_id, HomeAssistantClimateSnapshot &out);
bool home_assistant_request_calendar_events(const char *range_start, const char *range_end);
void home_assistant_get_calendar(HomeAssistantCalendarSnapshot &out);
bool home_assistant_get_calendar_event(size_t index, HomeAssistantCalendarEvent &out);

size_t home_assistant_get_media_players(HomeAssistantMediaSnapshot *out, size_t max_count);
size_t home_assistant_get_media_favorites(HomeAssistantMediaFavorite *out, size_t max_count);
bool home_assistant_request_media_browse(const char *entity_id);
bool home_assistant_request_media_artwork(const char *entity_id);
void home_assistant_get_media_artwork_info(HomeAssistantMediaArtworkInfo &out);
bool home_assistant_copy_media_artwork(uint8_t *dest, size_t capacity,
                                       HomeAssistantMediaArtworkInfo &out);

bool home_assistant_queue_toggle(const char *entity_id);
bool home_assistant_queue_area_brightness(uint8_t brightness_pct);
bool home_assistant_queue_all_lights(bool turn_on);
bool home_assistant_queue_scene(const char *entity_id);
bool home_assistant_queue_media_play_pause(const char *entity_id);
bool home_assistant_queue_media_previous(const char *entity_id);
bool home_assistant_queue_media_next(const char *entity_id);
bool home_assistant_queue_media_volume(const char *entity_id, uint8_t volume_pct);
bool home_assistant_queue_media_volume_step(const char *entity_id, bool increase);
bool home_assistant_queue_media_mute(const char *entity_id, bool muted);
bool home_assistant_queue_media_source(const char *entity_id, const char *source);
bool home_assistant_queue_media_favorite(const HomeAssistantMediaFavorite &favorite);

// A command is only accepted after the live Home Assistant session is ready.
// This prevents a tap during reconnect from being reported as a queued action.
bool home_assistant_commands_ready();

bool home_assistant_set_credentials(const char *base_url, const char *token);
String home_assistant_base_url();
bool home_assistant_token_configured();

// All discovered controllable room entities and scenes, without UI slot limits.
size_t home_assistant_get_room_entities(HomeAssistantEntitySnapshot *out, size_t max_count);
// All supported entities currently in the HA discovery cache, including media
// players. Intended for the authenticated web layout editor.
size_t home_assistant_get_layout_entities(HomeAssistantEntitySnapshot *out, size_t max_count);

// Brightness-capable light and switch entities share this legacy entry point.
bool home_assistant_queue_light_brightness(const char *entity_id, uint8_t brightness_pct);
bool home_assistant_queue_fan_speed(const char *entity_id, uint8_t percentage);
bool home_assistant_queue_climate_temperature(const char *entity_id, float temperature,
                                              float target_low, float target_high,
                                              bool use_range);
bool home_assistant_queue_climate_hvac_mode(const char *entity_id, const char *mode);
bool home_assistant_queue_climate_fan_mode(const char *entity_id, const char *mode);
bool home_assistant_queue_climate_preset(const char *entity_id, const char *preset);
// mode is one of home, away, night, vacation, or disarm. Code is optional for
// arming and required by the Security UI for every disarm attempt.
bool home_assistant_queue_alarm(const char *entity_id, const char *mode, const char *code);

bool home_assistant_get_room_entity(const char *entity_id, HomeAssistantEntitySnapshot &out);
// UI-neutral lookup for configurable Overview status and action cards.
bool home_assistant_get_entity(const char *entity_id, HomeAssistantEntitySnapshot &out);
