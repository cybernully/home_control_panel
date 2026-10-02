#include "home_assistant.h"

#include "app_config.h"
#include "config_service.h"
#include "network_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebSocketsClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace {

struct HaEntityModel {
    char entity_id[96];
    char name[64];
    char domain[16];
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
    uint32_t timer_sample_ms;
    int64_t timer_finishes_at_epoch;

    // Current weather attributes. Forecast collections are kept separately
    // because only one configured weather provider is displayed at a time.
    float weather_temperature;
    float weather_apparent_temperature;
    float weather_humidity;
    float weather_wind_speed;
    float weather_pressure;
    bool weather_has_temperature;
    bool weather_has_apparent_temperature;
    bool weather_has_humidity;
    bool weather_has_wind_speed;
    bool weather_has_pressure;
    char weather_temperature_unit[12];
    char weather_wind_speed_unit[16];

    // media_player state retained in the same PSRAM model so there is still a
    // single Home Assistant subscription/cache.
    uint8_t volume_pct;
    bool volume_muted;
    bool supports_volume;
    bool supports_mute;
    char media_title[96];
    char media_artist[96];
    char media_album[96];
    char media_playlist[96];
    char media_source[64];
    char entity_picture[HA_MEDIA_ARTWORK_URL_LEN];
    uint8_t media_source_count;
    char media_sources[HA_MAX_MEDIA_SOURCES][HA_MEDIA_SOURCE_NAME_LEN];
};

enum class HaActionType : uint8_t {
    Toggle,
    AreaBrightness,
    LightBrightness,
    FanSpeed,
    AllLights,
    Scene,
    MediaPlayPause,
    MediaPrevious,
    MediaNext,
    MediaVolume,
    MediaVolumeUp,
    MediaVolumeDown,
    MediaMute,
    MediaSource,
    MediaFavorite,
};

struct HaAction {
    HaActionType type;
    char entity_id[96];
    char text[HA_MEDIA_CONTENT_ID_LEN];
    char aux[HA_MEDIA_CONTENT_TYPE_LEN];
    uint8_t value;
    bool flag;
};

struct HaEndpoint {
    bool secure = false;
    String host;
    uint16_t port = 0;
    String websocket_path;
};

TaskHandle_t g_worker = nullptr;
QueueHandle_t g_action_queue = nullptr;
SemaphoreHandle_t g_artwork_mutex = nullptr;
WebSocketsClient g_ws;

portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

char g_base_url[160] = {};
char g_token[256] = {};
HomeAssistantStatus g_status = {};
HomeAssistantDiscoveryStatus g_discovery = {};

HaEntityModel *g_entities = nullptr;
size_t g_entity_count = 0;

bool g_ws_started = false;
bool g_ws_authenticated = false;
bool g_health_requested = false;
bool g_health_in_progress = false;
bool g_discovery_requested = false;
bool g_full_discovery_requested = false;
// The web editor's whole-home picker deliberately uses the REST states
// endpoint.  It must remain usable while the live WSS subscription is
// reconnecting, and the browser never receives the Home Assistant token.
bool g_rest_discovery_requested = false;
bool g_reconnect_requested = false;
bool g_network_ready = false;
bool g_action_in_flight = false;
bool g_resume_entities_after_reconnect = false;
bool g_resubscribe_requested = false;

uint32_t g_connected_since_ms = 0;
uint32_t g_last_health_request_ms = 0;
uint32_t g_last_http_ms = 0;
uint32_t g_ws_restart_not_before_ms = 0;
uint32_t g_last_entity_subscription_refresh_ms = 0;
uint32_t g_discovery_started_ms = 0;
constexpr uint32_t HA_REST_DISCOVERY_RECONNECT_DELAY_MS = 15000UL;
uint32_t g_next_ws_id = 1;
uint32_t g_area_request_id = 0;
uint32_t g_extract_request_id = 0;
uint32_t g_entity_registry_request_id = 0;
uint32_t g_subscribe_request_id = 0;
uint32_t g_action_request_id = 0;
uint32_t g_action_started_ms = 0;
char g_action_description[96] = {};

HomeAssistantWeatherSnapshot g_weather_cache = {};
bool g_weather_forecast_requested = false;
uint32_t g_weather_hourly_request_id = 0;
uint32_t g_weather_daily_request_id = 0;
uint32_t g_weather_forecast_started_ms = 0;
uint32_t g_weather_forecast_retry_not_before_ms = 0;
char g_weather_forecast_entity_id[96] = {};

HomeAssistantCalendarSnapshot g_calendar_cache = {};
HomeAssistantCalendarEvent *g_calendar_events = nullptr;
bool g_calendar_requested = false;
uint32_t g_calendar_request_ids[PANEL_MAX_CALENDARS] = {};
uint32_t g_calendar_request_started_ms = 0;
uint32_t g_calendar_retry_not_before_ms = 0;
bool g_calendar_any_success = false;
char g_calendar_requested_start[40] = {};
char g_calendar_requested_end[40] = {};
char g_calendar_next_start[40] = {};
char g_calendar_next_end[40] = {};

char g_resolved_area_id[64] = {};
char g_resolved_area_name[64] = {};

void record_action_result(const char *description, int code);
void send_subscribe_entities_worker();
void refresh_entity_subscription_worker();
void populate_configured_layout_entities_worker();
void send_weather_forecasts_worker();
void send_calendar_events_worker();

HomeAssistantMediaFavorite g_media_favorites[HA_MAX_MEDIA_FAVORITES] = {};
size_t g_media_favorite_count = 0;
bool g_media_browse_requested = false;
uint32_t g_media_browse_request_id = 0;
uint8_t g_media_browse_depth = 0;
char g_media_browse_entity_id[96] = {};
char g_media_browse_active_entity_id[96] = {};
char g_media_browse_content_id[HA_MEDIA_CONTENT_ID_LEN] = {};
char g_media_browse_content_type[HA_MEDIA_CONTENT_TYPE_LEN] = {};

bool g_artwork_requested = false;
char g_artwork_request_entity_id[96] = {};
char g_artwork_request_url[HA_MEDIA_ARTWORK_URL_LEN] = {};
char g_artwork_last_failed_url[HA_MEDIA_ARTWORK_URL_LEN] = {};
uint32_t g_artwork_retry_not_before_ms = 0;
uint8_t *g_artwork_data = nullptr;
HomeAssistantMediaArtworkInfo g_artwork_info = {};

void copy_text(char *dst, size_t len, const char *src) {
    if (!dst || len == 0) return;
    snprintf(dst, len, "%s", src ? src : "");
}

String normalized_base() {
    char local[sizeof(g_base_url)] = {};
    portENTER_CRITICAL(&g_mux);
    copy_text(local, sizeof(local), g_base_url);
    portEXIT_CRITICAL(&g_mux);

    String base(local);
    base.trim();
    while (base.endsWith("/")) base.remove(base.length() - 1);
    return base;
}

void credentials_snapshot(char *base, size_t base_len, char *token, size_t token_len) {
    portENTER_CRITICAL(&g_mux);
    copy_text(base, base_len, g_base_url);
    copy_text(token, token_len, g_token);
    portEXIT_CRITICAL(&g_mux);
}

bool configured_snapshot() {
    bool configured = false;
    portENTER_CRITICAL(&g_mux);
    configured = g_base_url[0] != '\0' && g_token[0] != '\0';
    portEXIT_CRITICAL(&g_mux);
    return configured;
}

bool network_ready_snapshot() {
    portENTER_CRITICAL(&g_mux);
    const bool ready = g_network_ready;
    portEXIT_CRITICAL(&g_mux);
    return ready;
}

bool take_flag(bool &flag) {
    bool value = false;
    portENTER_CRITICAL(&g_mux);
    value = flag;
    flag = false;
    portEXIT_CRITICAL(&g_mux);
    return value;
}

void set_health_message(const char *message) {
    portENTER_CRITICAL(&g_mux);
    copy_text(g_status.message, sizeof(g_status.message), message);
    portEXIT_CRITICAL(&g_mux);
}

void set_discovery_message(const char *message) {
    portENTER_CRITICAL(&g_mux);
    copy_text(g_discovery.message, sizeof(g_discovery.message), message);
    portEXIT_CRITICAL(&g_mux);
}

void reset_discovery_state(const char *message) {
    portENTER_CRITICAL(&g_mux);
    g_discovery.websocket_connected = false;
    g_discovery.websocket_authenticated = false;
    g_discovery.discovery_complete = false;
    g_discovery.area_found = false;
    g_discovery.entity_count = 0;
    g_discovery.device_count = 0;
    g_discovery.last_discovery_ms = 0;
    g_discovery.last_state_ms = 0;
    g_discovery.area_id[0] = '\0';
    g_discovery.area_name[0] = '\0';
    copy_text(g_discovery.message, sizeof(g_discovery.message), message);
    g_entity_count = 0;
    memset(g_media_favorites, 0, sizeof(g_media_favorites));
    g_media_favorite_count = 0;
    g_media_browse_requested = false;
    g_artwork_requested = false;
    g_artwork_last_failed_url[0] = '\0';
    g_artwork_retry_not_before_ms = 0;
    portEXIT_CRITICAL(&g_mux);

    g_resolved_area_id[0] = '\0';
    g_resolved_area_name[0] = '\0';
}

bool parse_endpoint(HaEndpoint &out) {
    String base = normalized_base();
    if (base.startsWith("https://")) {
        out.secure = true;
        base.remove(0, 8);
        out.port = 443;
    } else if (base.startsWith("http://")) {
        out.secure = false;
        base.remove(0, 7);
        out.port = 80;
    } else {
        return false;
    }

    const int slash = base.indexOf('/');
    String authority = slash >= 0 ? base.substring(0, slash) : base;
    String prefix = slash >= 0 ? base.substring(slash) : String();

    if (authority.startsWith("[")) {
        const int close = authority.indexOf(']');
        if (close <= 1) return false;
        out.host = authority.substring(1, close);
        if (close + 1 < static_cast<int>(authority.length()) && authority[close + 1] == ':') {
            const long port = authority.substring(close + 2).toInt();
            if (port <= 0 || port > 65535) return false;
            out.port = static_cast<uint16_t>(port);
        }
    } else {
        const int colon = authority.lastIndexOf(':');
        if (colon > 0 && authority.indexOf(':') == colon) {
            const long port = authority.substring(colon + 1).toInt();
            if (port <= 0 || port > 65535) return false;
            out.port = static_cast<uint16_t>(port);
            out.host = authority.substring(0, colon);
        } else {
            out.host = authority;
        }
    }

    out.host.trim();
    if (out.host.isEmpty()) return false;

    while (prefix.endsWith("/")) prefix.remove(prefix.length() - 1);
    out.websocket_path = prefix + "/api/websocket";
    return true;
}

const char *domain_from_entity_id(const char *entity_id, char *out, size_t out_len) {
    if (!entity_id || !out || out_len == 0) return "";
    const char *dot = strchr(entity_id, '.');
    if (!dot) {
        out[0] = '\0';
        return out;
    }
    size_t n = static_cast<size_t>(dot - entity_id);
    if (n >= out_len) n = out_len - 1;
    memcpy(out, entity_id, n);
    out[n] = '\0';
    return out;
}

bool is_control_domain(const char *domain) {
    return strcmp(domain, "light") == 0 ||
           strcmp(domain, "switch") == 0 ||
           strcmp(domain, "fan") == 0 ||
           strcmp(domain, "cover") == 0;
}

bool is_supported_domain(const char *domain) {
    return is_control_domain(domain) ||
           strcmp(domain, "scene") == 0 ||
           strcmp(domain, "media_player") == 0 ||
           strcmp(domain, "weather") == 0 ||
           strcmp(domain, "calendar") == 0 ||
           strcmp(domain, "timer") == 0 ||
           strcmp(domain, "sensor") == 0 ||
           strcmp(domain, "binary_sensor") == 0 ||
           strcmp(domain, "lock") == 0 ||
           strcmp(domain, "climate") == 0 ||
           strcmp(domain, "alarm_control_panel") == 0 ||
           strcmp(domain, "vacuum") == 0 ||
           strcmp(domain, "device_tracker") == 0 ||
           strcmp(domain, "person") == 0 ||
           strcmp(domain, "input_boolean") == 0;
}

void fallback_name_from_id(const char *entity_id, char *out, size_t out_len) {
    const char *name = entity_id ? strchr(entity_id, '.') : nullptr;
    name = name ? name + 1 : entity_id;
    if (!name) name = "Entity";

    size_t j = 0;
    bool upper = true;
    for (size_t i = 0; name[i] && j + 1 < out_len; ++i) {
        char c = name[i];
        if (c == '_') {
            out[j++] = ' ';
            upper = true;
        } else {
            if (upper && c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
            out[j++] = c;
            upper = false;
        }
    }
    out[j] = '\0';
}

HaEntityModel *find_entity_worker(const char *entity_id) {
    if (!entity_id) return nullptr;
    for (size_t i = 0; i < g_entity_count; ++i) {
        if (strcmp(g_entities[i].entity_id, entity_id) == 0) return &g_entities[i];
    }
    return nullptr;
}

void update_discovery_counts_locked() {
    g_discovery.entity_count = static_cast<uint16_t>(g_entity_count);
}

bool parse_timer_duration(const char *value, uint32_t &seconds) {
    if (!value || !value[0]) return false;
    unsigned long hours = 0, minutes = 0, secs = 0;
    char trailing = '\0';
    if (sscanf(value, "%lu:%lu:%lu%c", &hours, &minutes, &secs, &trailing) != 3 ||
        minutes >= 60 || secs >= 60 || hours > 1193046UL) return false;
    seconds = static_cast<uint32_t>(hours * 3600UL + minutes * 60UL + secs);
    return true;
}

int64_t days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
    const unsigned adjusted_month = month > 2 ? month - 3U : month + 9U;
    const unsigned day_of_year = (153U * adjusted_month + 2U) / 5U + day - 1U;
    const unsigned day_of_era = year_of_era * 365U + year_of_era / 4U - year_of_era / 100U + day_of_year;
    return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(day_of_era) - 719468;
}

bool parse_timer_finish(const char *value, int64_t &epoch) {
    if (!value) return false;
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (sscanf(value, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6 ||
        month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 || second < 0 || second > 60) return false;
    epoch = days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * 86400 +
            hour * 3600 + minute * 60 + second;
    const char *zone = value + 19;
    while (*zone && *zone != 'Z' && *zone != '+' && *zone != '-') ++zone;
    if (*zone == '+' || *zone == '-') {
        int offset_hour = 0, offset_minute = 0;
        if (sscanf(zone + 1, "%d:%d", &offset_hour, &offset_minute) != 2 ||
            offset_hour > 23 || offset_minute > 59) return false;
        const int offset = offset_hour * 3600 + offset_minute * 60;
        epoch += *zone == '+' ? -offset : offset;
    }
    return true;
}

void apply_attributes_locked(HaEntityModel &model, JsonObjectConst attrs) {
    if (attrs.isNull()) return;

    const char *friendly = attrs["friendly_name"].as<const char *>();
    if (friendly && friendly[0]) copy_text(model.name, sizeof(model.name), friendly);
    const char *unit = attrs["unit_of_measurement"].as<const char *>();
    if (unit) copy_text(model.unit_of_measurement, sizeof(model.unit_of_measurement), unit);

    if (!attrs["brightness"].isNull()) {
        int brightness = attrs["brightness"].as<int>();
        brightness = constrain(brightness, 0, 255);
        model.brightness_pct = static_cast<uint8_t>((brightness * 100 + 127) / 255);
        model.supports_brightness = true;
    }

    if (!attrs["current_position"].isNull()) {
        int position = attrs["current_position"].as<int>();
        model.position_pct = static_cast<int16_t>(constrain(position, 0, 100));
        model.supports_position = true;
    }

    if (strcmp(model.domain, "fan") == 0 && !attrs["percentage"].isNull()) {
        model.fan_speed_pct = static_cast<uint8_t>(constrain(attrs["percentage"].as<int>(), 0, 100));
        model.supports_fan_speed = true;
    }

    if (strcmp(model.domain, "timer") == 0) {
        if (!attrs["remaining"].isNull()) {
            uint32_t seconds = 0;
            model.timer_has_remaining = parse_timer_duration(attrs["remaining"] | "", seconds);
            model.timer_remaining_seconds = model.timer_has_remaining ? seconds : 0;
            model.timer_sample_ms = millis();
        }
        if (!attrs["finishes_at"].isNull()) {
            int64_t finish = 0;
            model.timer_finishes_at_epoch = parse_timer_finish(attrs["finishes_at"] | "", finish) ? finish : 0;
        }
    }

    if (strcmp(model.domain, "weather") == 0) {
        if (!attrs["temperature"].isNull()) {
            model.weather_temperature = attrs["temperature"].as<float>();
            model.weather_has_temperature = true;
        }
        if (!attrs["apparent_temperature"].isNull()) {
            model.weather_apparent_temperature = attrs["apparent_temperature"].as<float>();
            model.weather_has_apparent_temperature = true;
        }
        if (!attrs["humidity"].isNull()) {
            model.weather_humidity = attrs["humidity"].as<float>();
            model.weather_has_humidity = true;
        }
        if (!attrs["wind_speed"].isNull()) {
            model.weather_wind_speed = attrs["wind_speed"].as<float>();
            model.weather_has_wind_speed = true;
        }
        if (!attrs["pressure"].isNull()) {
            model.weather_pressure = attrs["pressure"].as<float>();
            model.weather_has_pressure = true;
        }
        const char *temperature_unit = attrs["temperature_unit"] | "";
        if (temperature_unit[0]) copy_text(model.weather_temperature_unit,
                                           sizeof(model.weather_temperature_unit), temperature_unit);
        const char *wind_unit = attrs["wind_speed_unit"] | "";
        if (wind_unit[0]) copy_text(model.weather_wind_speed_unit,
                                    sizeof(model.weather_wind_speed_unit), wind_unit);
    }

    if (strcmp(model.domain, "media_player") == 0) {
        if (!attrs["volume_level"].isNull()) {
            float level = attrs["volume_level"].as<float>();
            if (level < 0.0f) level = 0.0f;
            if (level > 1.0f) level = 1.0f;
            model.volume_pct = static_cast<uint8_t>(level * 100.0f + 0.5f);
            model.supports_volume = true;
        }

        if (!attrs["is_volume_muted"].isNull()) {
            model.volume_muted = attrs["is_volume_muted"].as<bool>();
            model.supports_mute = true;
        }

        const char *title = attrs["media_title"].as<const char *>();
        if (title) copy_text(model.media_title, sizeof(model.media_title), title);
        const char *artist = attrs["media_artist"].as<const char *>();
        if (artist) copy_text(model.media_artist, sizeof(model.media_artist), artist);
        const char *album = attrs["media_album_name"].as<const char *>();
        if (album) copy_text(model.media_album, sizeof(model.media_album), album);
        const char *playlist = attrs["media_playlist"].as<const char *>();
        if (playlist) copy_text(model.media_playlist, sizeof(model.media_playlist), playlist);
        const char *source = attrs["source"].as<const char *>();
        if (source) copy_text(model.media_source, sizeof(model.media_source), source);
        const char *picture = attrs["entity_picture"].as<const char *>();
        if (picture) copy_text(model.entity_picture, sizeof(model.entity_picture), picture);

        JsonArrayConst sources = attrs["source_list"].as<JsonArrayConst>();
        if (!sources.isNull()) {
            model.media_source_count = 0;
            memset(model.media_sources, 0, sizeof(model.media_sources));
            for (JsonVariantConst source_item : sources) {
                if (model.media_source_count >= HA_MAX_MEDIA_SOURCES) break;
                const char *source_name = source_item.as<const char *>();
                if (!source_name || !source_name[0]) continue;
                copy_text(model.media_sources[model.media_source_count],
                          HA_MEDIA_SOURCE_NAME_LEN, source_name);
                ++model.media_source_count;
            }
        }
    }
}

void apply_full_state_worker(const char *entity_id, JsonObjectConst compressed) {
    const char *state = compressed["s"] | "";
    JsonObjectConst attrs = compressed["a"].as<JsonObjectConst>();

    portENTER_CRITICAL(&g_mux);
    HaEntityModel *model = find_entity_worker(entity_id);
    if (model) {
        copy_text(model->state, sizeof(model->state), state);
        model->available = state[0] != '\0' && strcmp(state, "unavailable") != 0;
        apply_attributes_locked(*model, attrs);
        if (!model->name[0]) {
            fallback_name_from_id(model->entity_id, model->name, sizeof(model->name));
        }
    }
    portEXIT_CRITICAL(&g_mux);
}

void apply_diff_worker(const char *entity_id, JsonObjectConst diff) {
    JsonObjectConst additions = diff["+"].as<JsonObjectConst>();
    JsonObjectConst removals = diff["-"].as<JsonObjectConst>();

    portENTER_CRITICAL(&g_mux);
    HaEntityModel *model = find_entity_worker(entity_id);
    if (model) {
        if (!additions.isNull()) {
            if (!additions["s"].isNull()) {
                const char *state = additions["s"] | "";
                copy_text(model->state, sizeof(model->state), state);
                model->available = state[0] != '\0' && strcmp(state, "unavailable") != 0;
            }
            JsonObjectConst attrs = additions["a"].as<JsonObjectConst>();
            apply_attributes_locked(*model, attrs);
        }

        JsonArrayConst removed_attrs = removals["a"].as<JsonArrayConst>();
        if (!removed_attrs.isNull()) {
            for (JsonVariantConst item : removed_attrs) {
                const char *key = item.as<const char *>();
                if (!key) continue;
                if (strcmp(key, "brightness") == 0) {
                    model->brightness_pct = 0;
                    model->supports_brightness = false;
                } else if (strcmp(key, "current_position") == 0) {
                    model->position_pct = -1;
                    model->supports_position = false;
                } else if (strcmp(key, "percentage") == 0) {
                    model->fan_speed_pct = 0;
                    model->supports_fan_speed = false;
                } else if (strcmp(key, "remaining") == 0) {
                    model->timer_has_remaining = false;
                    model->timer_remaining_seconds = 0;
                    model->timer_sample_ms = 0;
                } else if (strcmp(key, "finishes_at") == 0) {
                    model->timer_finishes_at_epoch = 0;
                } else if (strcmp(key, "friendly_name") == 0) {
                    fallback_name_from_id(model->entity_id, model->name, sizeof(model->name));
                } else if (strcmp(key, "unit_of_measurement") == 0) {
                    model->unit_of_measurement[0] = '\0';
                } else if (strcmp(key, "volume_level") == 0) {
                    model->volume_pct = 0;
                    model->supports_volume = false;
                } else if (strcmp(key, "is_volume_muted") == 0) {
                    model->volume_muted = false;
                    model->supports_mute = false;
                } else if (strcmp(key, "media_title") == 0) {
                    model->media_title[0] = '\0';
                } else if (strcmp(key, "media_artist") == 0) {
                    model->media_artist[0] = '\0';
                } else if (strcmp(key, "media_album_name") == 0) {
                    model->media_album[0] = '\0';
                } else if (strcmp(key, "media_playlist") == 0) {
                    model->media_playlist[0] = '\0';
                } else if (strcmp(key, "source") == 0) {
                    model->media_source[0] = '\0';
                } else if (strcmp(key, "source_list") == 0) {
                    model->media_source_count = 0;
                    memset(model->media_sources, 0, sizeof(model->media_sources));
                } else if (strcmp(key, "entity_picture") == 0) {
                    model->entity_picture[0] = '\0';
                }
            }
        }
    }
    portEXIT_CRITICAL(&g_mux);
}

void mark_state_update_worker() {
    const uint32_t now = millis();
    portENTER_CRITICAL(&g_mux);
    g_discovery.last_state_ms = now;
    portEXIT_CRITICAL(&g_mux);
}

bool send_json(JsonDocument &doc) {
    if (!g_ws.isConnected()) return false;
    String payload;
    serializeJson(doc, payload);
    return g_ws.sendTXT(payload);
}

uint32_t next_ws_id() {
    if (g_next_ws_id == 0) g_next_ws_id = 1;
    return g_next_ws_id++;
}

void send_area_lookup_worker() {
    if (g_full_discovery_requested) {
        JsonDocument doc;
        g_entity_registry_request_id = next_ws_id();
        doc["id"] = g_entity_registry_request_id;
        // This compact registry call is intended for device-picker clients.
        // It is deliberately separate from the area-targeted live subscription.
        doc["type"] = "config/entity_registry/list_for_display";
        if (send_json(doc)) {
            g_discovery_started_ms = millis();
            set_discovery_message("Searching all Home Assistant entities...");
        } else {
            set_discovery_message("Could not send Home Assistant device search request.");
        }
        return;
    }

    // The live panel is driven exclusively by the entities selected in the
    // web manager. An area is no longer needed to discover or subscribe to
    // controls at startup.
    populate_configured_layout_entities_worker();
    return;

    const PanelConfig &cfg = config_service_get();
    if (!cfg.area_id[0]) {
        portENTER_CRITICAL(&g_mux);
        g_discovery.discovery_complete = true;
        g_discovery.area_found = false;
        copy_text(g_discovery.message, sizeof(g_discovery.message), "No Home Assistant area is configured.");
        portEXIT_CRITICAL(&g_mux);
        return;
    }

    JsonDocument doc;
    g_area_request_id = next_ws_id();
    doc["id"] = g_area_request_id;
    doc["type"] = "config/area_registry/list";
    if (send_json(doc)) {
        g_discovery_started_ms = millis();
        set_discovery_message("Resolving Home Assistant area...");
    } else {
        set_discovery_message("Could not send area discovery request.");
    }
}

void send_extract_target_worker() {
    if (!g_resolved_area_id[0]) return;

    JsonDocument doc;
    g_extract_request_id = next_ws_id();
    doc["id"] = g_extract_request_id;
    doc["type"] = "extract_from_target";
    doc["expand_group"] = false;
    doc["primary_entities_only"] = false;
    JsonObject target = doc["target"].to<JsonObject>();
    JsonArray areas = target["area_id"].to<JsonArray>();
    areas.add(g_resolved_area_id);

    if (send_json(doc)) {
        set_discovery_message("Resolving devices and entities in area...");
    } else {
        set_discovery_message("Could not send area target request.");
    }
}

void handle_entity_registry_result_worker(JsonDocument &doc) {
    const bool success = doc["success"] | false;
    if (!success) {
        g_full_discovery_requested = false;
        set_discovery_message("Home Assistant rejected the global device search.");
        return;
    }

    JsonArrayConst entities = doc["result"]["entities"].as<JsonArrayConst>();
    if (entities.isNull()) {
        // Older Home Assistant versions return the uncompressed list format.
        entities = doc["result"].as<JsonArrayConst>();
    }

    portENTER_CRITICAL(&g_mux);
    g_entity_count = 0;
    g_calendar_cache.last_update_ms = 0;
    g_calendar_cache.available = false;
    g_calendar_cache.event_count = 0;
    g_calendar_next_start[0] = '\0';
    g_calendar_next_end[0] = '\0';
    portEXIT_CRITICAL(&g_mux);
    bool truncated = false;
    for (JsonObjectConst item : entities) {
        const char *entity_id = item["ei"] | "";
        if (!entity_id[0]) entity_id = item["entity_id"] | "";
        if (!entity_id[0]) continue;

        char domain[16] = {};
        domain_from_entity_id(entity_id, domain, sizeof(domain));
        if (!is_supported_domain(domain)) continue;

        portENTER_CRITICAL(&g_mux);
        if (g_entity_count >= HA_MAX_AREA_ENTITIES) {
            truncated = true;
            portEXIT_CRITICAL(&g_mux);
            break;
        }
        HaEntityModel &model = g_entities[g_entity_count++];
        memset(&model, 0, sizeof(model));
        copy_text(model.entity_id, sizeof(model.entity_id), entity_id);
        copy_text(model.domain, sizeof(model.domain), domain);
        const char *name = item["en"] | "";
        if (!name[0]) name = item["name"] | "";
        if (!name[0]) name = item["original_name"] | "";
        if (name[0]) copy_text(model.name, sizeof(model.name), name);
        else fallback_name_from_id(entity_id, model.name, sizeof(model.name));
        copy_text(model.state, sizeof(model.state), "unknown");
        model.position_pct = -1;
        portEXIT_CRITICAL(&g_mux);
    }

    portENTER_CRITICAL(&g_mux);
    g_discovery.area_found = true;
    g_discovery.device_count = static_cast<uint16_t>(entities.size());
    update_discovery_counts_locked();
    copy_text(g_discovery.area_id, sizeof(g_discovery.area_id), "all");
    copy_text(g_discovery.area_name, sizeof(g_discovery.area_name), "All Home Assistant");
    if (truncated) {
        copy_text(g_discovery.message, sizeof(g_discovery.message),
                  "Device search is limited to the first supported entities.");
    }
    portEXIT_CRITICAL(&g_mux);

    // A full scan is for the editor only. Future reconnects must return to the
    // smaller configured layout subscription.
    g_full_discovery_requested = false;
    send_subscribe_entities_worker();
}

bool is_layout_entity(const char *entity_id) {
    if (!entity_id || !entity_id[0]) return false;
    const PanelConfig &cfg = config_service_get();
    if (!cfg.explicit_layout) return true;
    for (uint8_t i = 0; i < cfg.room_control_count; ++i)
        if (strcmp(cfg.room_controls[i].entity_id, entity_id) == 0) return true;
    for (uint8_t i = 0; i < cfg.room_count; ++i)
        for (uint8_t slot = 0; slot < PANEL_ROOM_STATUS_SLOTS; ++slot)
            if (cfg.rooms[i].status_slots[slot].entity_id[0] &&
                strcmp(cfg.rooms[i].status_slots[slot].entity_id, entity_id) == 0) return true;
    for (uint8_t i = 0; i < cfg.media_player_count; ++i)
        if (strcmp(cfg.media_players[i], entity_id) == 0) return true;
    for (uint8_t i = 0; i < cfg.media_shortcut_count; ++i)
        if (strcmp(cfg.media_shortcuts[i].entity_id, entity_id) == 0) return true;
    for (uint8_t i = 0; i < cfg.media_favorite_count; ++i)
        if (strcmp(cfg.media_favorites[i].entity_id, entity_id) == 0) return true;
    for (uint8_t i = 0; i < cfg.overview_quick_action_count; ++i)
        if (cfg.overview_quick_actions[i].entity_id[0] &&
            strcmp(cfg.overview_quick_actions[i].entity_id, entity_id) == 0) return true;
    for (uint8_t i = 0; i < cfg.overview_item_count; ++i) {
        if ((cfg.overview_items[i].entity_id[0] &&
             strcmp(cfg.overview_items[i].entity_id, entity_id) == 0) ||
            (cfg.overview_items[i].action_entity_id[0] &&
             strcmp(cfg.overview_items[i].action_entity_id, entity_id) == 0)) return true;
    }
    if (cfg.weather_entity_id[0] && strcmp(cfg.weather_entity_id, entity_id) == 0) return true;
    for (uint8_t i = 0; i < cfg.calendar_count; ++i)
        if (strcmp(cfg.calendars[i].entity_id, entity_id) == 0) return true;
    const char *dot = strchr(entity_id, '.');
    const size_t domain_len = dot ? static_cast<size_t>(dot - entity_id) : 0;
    for (uint8_t i = 0; i < cfg.overview_widget_count; ++i) {
        const char *widget = cfg.overview_widgets[i].type;
        if ((domain_len == 7 && strncmp(entity_id, "weather", 7) == 0 && strcmp(widget, "weather") == 0) ||
            (domain_len == 8 && strncmp(entity_id, "calendar", 8) == 0 && strcmp(widget, "calendar") == 0)) return true;
    }
    return false;
}

void populate_configured_layout_entities_worker() {
    const PanelConfig &cfg = config_service_get();
    portENTER_CRITICAL(&g_mux);
    g_entity_count = 0;
    g_calendar_cache.last_update_ms = 0;
    g_calendar_cache.available = false;
    g_calendar_cache.event_count = 0;
    g_calendar_next_start[0] = '\0';
    g_calendar_next_end[0] = '\0';
    portEXIT_CRITICAL(&g_mux);

    auto add_entity = [](const char *entity_id, bool allow_generic_status = false) {
        if (!entity_id || !entity_id[0]) return;
        char domain[16] = {};
        domain_from_entity_id(entity_id, domain, sizeof(domain));
        if (!is_supported_domain(domain) && !allow_generic_status) return;

        portENTER_CRITICAL(&g_mux);
        for (size_t i = 0; i < g_entity_count; ++i) {
            if (strcmp(g_entities[i].entity_id, entity_id) == 0) {
                portEXIT_CRITICAL(&g_mux);
                return;
            }
        }
        if (g_entity_count < HA_MAX_AREA_ENTITIES) {
            HaEntityModel &model = g_entities[g_entity_count++];
            memset(&model, 0, sizeof(model));
            copy_text(model.entity_id, sizeof(model.entity_id), entity_id);
            copy_text(model.domain, sizeof(model.domain), domain);
            fallback_name_from_id(entity_id, model.name, sizeof(model.name));
            copy_text(model.state, sizeof(model.state), "unknown");
            model.position_pct = -1;
        }
        portEXIT_CRITICAL(&g_mux);
    };

    for (uint8_t i = 0; i < cfg.room_control_count; ++i) add_entity(cfg.room_controls[i].entity_id);
    for (uint8_t i = 0; i < cfg.room_count; ++i)
        for (uint8_t slot = 0; slot < PANEL_ROOM_STATUS_SLOTS; ++slot)
            add_entity(cfg.rooms[i].status_slots[slot].entity_id, true);
    for (uint8_t i = 0; i < cfg.media_player_count; ++i) add_entity(cfg.media_players[i]);
    for (uint8_t i = 0; i < cfg.media_shortcut_count; ++i) add_entity(cfg.media_shortcuts[i].entity_id);
    for (uint8_t i = 0; i < cfg.media_favorite_count; ++i) add_entity(cfg.media_favorites[i].entity_id);
    for (uint8_t i = 0; i < cfg.overview_quick_action_count; ++i)
        add_entity(cfg.overview_quick_actions[i].entity_id);
    for (uint8_t i = 0; i < cfg.overview_item_count; ++i) {
        add_entity(cfg.overview_items[i].entity_id, true);
        add_entity(cfg.overview_items[i].action_entity_id);
    }
    add_entity(cfg.weather_entity_id);
    for (uint8_t i = 0; i < cfg.calendar_count; ++i) add_entity(cfg.calendars[i].entity_id, true);

    portENTER_CRITICAL(&g_mux);
    g_discovery.area_found = true;
    g_discovery.device_count = 0;
    update_discovery_counts_locked();
    g_discovery.area_id[0] = '\0';
    copy_text(g_discovery.area_name, sizeof(g_discovery.area_name), "Configured layout");
    portEXIT_CRITICAL(&g_mux);

    if (g_entity_count == 0) {
        portENTER_CRITICAL(&g_mux);
        g_discovery.discovery_complete = true;
        g_discovery.last_discovery_ms = millis();
        copy_text(g_discovery.message, sizeof(g_discovery.message),
                  "No Home Assistant controls selected. Use the web manager to add devices.");
        portEXIT_CRITICAL(&g_mux);
        return;
    }
    set_discovery_message("Subscribing to configured Home Assistant controls...");
    send_subscribe_entities_worker();
}

void send_subscribe_entities_worker() {
    if (g_entity_count == 0) {
        portENTER_CRITICAL(&g_mux);
        g_discovery.discovery_complete = true;
        g_discovery.last_discovery_ms = millis();
        copy_text(g_discovery.message, sizeof(g_discovery.message), "Area found; no supported entities were discovered.");
        portEXIT_CRITICAL(&g_mux);
        return;
    }

    JsonDocument doc;
    g_subscribe_request_id = next_ws_id();
    doc["id"] = g_subscribe_request_id;
    doc["type"] = "subscribe_entities";
    JsonArray ids = doc["entity_ids"].to<JsonArray>();
    for (size_t i = 0; i < g_entity_count; ++i) ids.add(g_entities[i].entity_id);

    if (send_json(doc)) {
        g_last_entity_subscription_refresh_ms = millis();
        set_discovery_message("Subscribing to live Home Assistant state...");
        const char *weather_id = config_service_get().weather_entity_id;
        if (weather_id[0]) home_assistant_request_weather_forecasts(weather_id);
    } else {
        set_discovery_message("Could not start live state subscription.");
    }
}


void clear_media_favorites_worker() {
    portENTER_CRITICAL(&g_mux);
    memset(g_media_favorites, 0, sizeof(g_media_favorites));
    g_media_favorite_count = 0;
    portEXIT_CRITICAL(&g_mux);
}

void send_media_browse_worker(const char *entity_id,
                              const char *content_id,
                              const char *content_type,
                              uint8_t depth) {
    if (!entity_id || !entity_id[0] || !g_ws_authenticated) return;

    JsonDocument doc;
    g_media_browse_request_id = next_ws_id();
    g_media_browse_depth = depth;
    copy_text(g_media_browse_active_entity_id, sizeof(g_media_browse_active_entity_id), entity_id);
    doc["id"] = g_media_browse_request_id;
    doc["type"] = "media_player/browse_media";
    doc["entity_id"] = entity_id;
    if (content_id && content_id[0]) doc["media_content_id"] = content_id;
    if (content_type && content_type[0]) doc["media_content_type"] = content_type;

    if (!send_json(doc)) {
        set_discovery_message("Could not browse media for selected player.");
    }
}

void send_weather_forecast_worker(const char *forecast_type, uint32_t &request_id) {
    if (!g_weather_forecast_entity_id[0] || !g_ws_authenticated) return;
    JsonDocument doc;
    request_id = next_ws_id();
    doc["id"] = request_id;
    doc["type"] = "call_service";
    doc["domain"] = "weather";
    doc["service"] = "get_forecasts";
    doc["service_data"]["type"] = forecast_type;
    doc["target"]["entity_id"] = g_weather_forecast_entity_id;
    doc["return_response"] = true;
    if (!send_json(doc)) request_id = 0;
}

void send_weather_forecasts_worker() {
    const PanelConfig &cfg = config_service_get();
    bool need_hourly = cfg.weather_show_hourly;
    bool need_daily = cfg.weather_show_daily;
    for (uint8_t i = 0; i < cfg.overview_item_count; ++i) {
        if (strcmp(cfg.overview_items[i].type, "weather_hourly") == 0) need_hourly = true;
        if (strcmp(cfg.overview_items[i].type, "weather_daily") == 0) need_daily = true;
    }
    if (need_hourly) send_weather_forecast_worker("hourly", g_weather_hourly_request_id);
    if (need_daily) send_weather_forecast_worker("daily", g_weather_daily_request_id);
    if (!g_weather_hourly_request_id && !g_weather_daily_request_id) {
        portENTER_CRITICAL(&g_mux);
        g_weather_cache.forecasts_loading = false;
        g_weather_cache.last_forecast_ms = millis();
        portEXIT_CRITICAL(&g_mux);
    } else g_weather_forecast_started_ms = millis();
}

void handle_weather_forecast_result_worker(JsonDocument &doc, bool hourly) {
    HomeAssistantWeatherForecast parsed[HA_MAX_WEATHER_HOURLY] = {};
    const size_t capacity = hourly ? HA_MAX_WEATHER_HOURLY : HA_MAX_WEATHER_DAILY;
    size_t count = 0;
    if (doc["success"] | false) {
        JsonObjectConst response = doc["result"]["response"].as<JsonObjectConst>();
        JsonObjectConst provider = response[g_weather_forecast_entity_id].as<JsonObjectConst>();
        JsonArrayConst forecasts = provider["forecast"].as<JsonArrayConst>();
        // Keep compatibility with integrations that return the forecast body
        // without the provider wrapper.
        if (forecasts.isNull()) forecasts = response["forecast"].as<JsonArrayConst>();
        if (!forecasts.isNull()) {
            for (JsonObjectConst item : forecasts) {
                if (count >= capacity) break;
                HomeAssistantWeatherForecast &out = parsed[count++];
                copy_text(out.datetime, sizeof(out.datetime), item["datetime"] | "");
                copy_text(out.condition, sizeof(out.condition), item["condition"] | "unknown");
                if (!item["temperature"].isNull()) {
                    out.temperature = item["temperature"].as<float>();
                    out.has_temperature = true;
                }
                if (!item["templow"].isNull()) {
                    out.temperature_low = item["templow"].as<float>();
                    out.has_temperature_low = true;
                }
                if (!item["precipitation_probability"].isNull()) {
                    out.precipitation_probability = static_cast<uint8_t>(
                        constrain(item["precipitation_probability"].as<int>(), 0, 100));
                    out.has_precipitation_probability = true;
                }
            }
        }
    }

    portENTER_CRITICAL(&g_mux);
    if (hourly) {
        memset(g_weather_cache.hourly, 0, sizeof(g_weather_cache.hourly));
        memcpy(g_weather_cache.hourly, parsed,
               count * sizeof(HomeAssistantWeatherForecast));
        g_weather_cache.hourly_count = static_cast<uint8_t>(count);
        g_weather_hourly_request_id = 0;
    } else {
        memset(g_weather_cache.daily, 0, sizeof(g_weather_cache.daily));
        memcpy(g_weather_cache.daily, parsed,
               count * sizeof(HomeAssistantWeatherForecast));
        g_weather_cache.daily_count = static_cast<uint8_t>(count);
        g_weather_daily_request_id = 0;
    }
    if (!g_weather_hourly_request_id && !g_weather_daily_request_id) {
        g_weather_cache.forecasts_loading = false;
        g_weather_cache.last_forecast_ms = millis();
        g_weather_forecast_started_ms = 0;
        g_weather_forecast_retry_not_before_ms = 0;
    }
    portEXIT_CRITICAL(&g_mux);
}

bool calendar_requests_active() {
    for (uint8_t i = 0; i < PANEL_MAX_CALENDARS; ++i)
        if (g_calendar_request_ids[i]) return true;
    return false;
}

void send_calendar_events_worker() {
    const PanelConfig &cfg = config_service_get();
    if (!g_ws_authenticated || !cfg.calendar_count ||
        !g_calendar_next_start[0] || !g_calendar_next_end[0]) return;

    portENTER_CRITICAL(&g_mux);
    copy_text(g_calendar_requested_start, sizeof(g_calendar_requested_start), g_calendar_next_start);
    copy_text(g_calendar_requested_end, sizeof(g_calendar_requested_end), g_calendar_next_end);
    portEXIT_CRITICAL(&g_mux);

    memset(g_calendar_request_ids, 0, sizeof(g_calendar_request_ids));
    g_calendar_any_success = false;
    portENTER_CRITICAL(&g_mux);
    g_calendar_cache.loading = true;
    g_calendar_cache.event_count = 0;
    g_calendar_cache.available = false;
    if (g_calendar_events)
        memset(g_calendar_events, 0, HA_MAX_CALENDAR_EVENTS * sizeof(HomeAssistantCalendarEvent));
    portEXIT_CRITICAL(&g_mux);

    bool sent = false;
    for (uint8_t i = 0; i < cfg.calendar_count; ++i) {
        JsonDocument doc;
        g_calendar_request_ids[i] = next_ws_id();
        doc["id"] = g_calendar_request_ids[i];
        doc["type"] = "call_service";
        doc["domain"] = "calendar";
        doc["service"] = "get_events";
        doc["service_data"]["start_date_time"] = g_calendar_requested_start;
        doc["service_data"]["end_date_time"] = g_calendar_requested_end;
        doc["target"]["entity_id"] = cfg.calendars[i].entity_id;
        doc["return_response"] = true;
        if (send_json(doc)) sent = true;
        else g_calendar_request_ids[i] = 0;
    }
    if (sent) g_calendar_request_started_ms = millis();
    else {
        portENTER_CRITICAL(&g_mux);
        g_calendar_cache.loading = false;
        portEXIT_CRITICAL(&g_mux);
    }
}

void handle_calendar_result_worker(JsonDocument &doc, uint8_t calendar_index) {
    HomeAssistantCalendarEvent *parsed = static_cast<HomeAssistantCalendarEvent *>(
        heap_caps_calloc(HA_MAX_CALENDAR_EVENTS, sizeof(HomeAssistantCalendarEvent),
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!parsed) parsed = static_cast<HomeAssistantCalendarEvent *>(
        heap_caps_calloc(HA_MAX_CALENDAR_EVENTS, sizeof(HomeAssistantCalendarEvent), MALLOC_CAP_8BIT));
    size_t count = 0;
    bool success = (doc["success"] | false) && parsed && g_calendar_events;
    if (success) {
        const PanelConfig &cfg = config_service_get();
        JsonObjectConst response = doc["result"]["response"].as<JsonObjectConst>();
        if (response.isNull()) response = doc["result"].as<JsonObjectConst>();
        const char *entity_id = calendar_index < cfg.calendar_count ?
            cfg.calendars[calendar_index].entity_id : "";
        JsonArrayConst events = response[entity_id]["events"].as<JsonArrayConst>();
        if (events.isNull()) events = response["events"].as<JsonArrayConst>();
        if (events.isNull()) success = false;
        for (JsonObjectConst item : events) {
            if (count >= HA_MAX_CALENDAR_EVENTS) break;
            HomeAssistantCalendarEvent &event = parsed[count++];
            copy_text(event.calendar_entity_id, sizeof(event.calendar_entity_id), entity_id);
            copy_text(event.summary, sizeof(event.summary), item["summary"] | "Untitled event");
            copy_text(event.description, sizeof(event.description), item["description"] | "");
            copy_text(event.location, sizeof(event.location), item["location"] | "");
            copy_text(event.start, sizeof(event.start), item["start"] | "");
            copy_text(event.end, sizeof(event.end), item["end"] | "");
            event.all_day = strlen(event.start) == 10 && event.start[4] == '-' && event.start[7] == '-';
        }
    }

    portENTER_CRITICAL(&g_mux);
    if (success) {
        const size_t remaining = HA_MAX_CALENDAR_EVENTS - g_calendar_cache.event_count;
        const size_t append = count < remaining ? count : remaining;
        memcpy(g_calendar_events + g_calendar_cache.event_count, parsed,
               append * sizeof(HomeAssistantCalendarEvent));
        g_calendar_cache.event_count += static_cast<uint8_t>(append);
        g_calendar_any_success = true;
    }
    if (calendar_index < PANEL_MAX_CALENDARS) g_calendar_request_ids[calendar_index] = 0;
    bool complete = true;
    for (uint8_t i = 0; i < PANEL_MAX_CALENDARS; ++i)
        if (g_calendar_request_ids[i]) { complete = false; break; }
    if (complete) {
        const bool newer_range_pending = g_calendar_requested &&
            (strcmp(g_calendar_next_start, g_calendar_requested_start) != 0 ||
             strcmp(g_calendar_next_end, g_calendar_requested_end) != 0);
        g_calendar_cache.loading = newer_range_pending;
        g_calendar_cache.available = !newer_range_pending && g_calendar_any_success;
        if (newer_range_pending) {
            g_calendar_cache.event_count = 0;
        } else if (g_calendar_any_success) {
            copy_text(g_calendar_cache.range_start, sizeof(g_calendar_cache.range_start), g_calendar_requested_start);
            copy_text(g_calendar_cache.range_end, sizeof(g_calendar_cache.range_end), g_calendar_requested_end);
            g_calendar_cache.last_update_ms = millis();
            g_calendar_retry_not_before_ms = 0;
        } else g_calendar_retry_not_before_ms = millis() + HA_CALENDAR_RETRY_MS;
        g_calendar_request_started_ms = 0;
    }
    portEXIT_CRITICAL(&g_mux);
    if (parsed) heap_caps_free(parsed);
}

void handle_media_browse_result_worker(JsonDocument &doc) {
    const bool success = doc["success"] | false;
    if (!success) {
        clear_media_favorites_worker();
        return;
    }

    JsonObjectConst result = doc["result"].as<JsonObjectConst>();
    JsonArrayConst children = result["children"].as<JsonArrayConst>();

    HomeAssistantMediaFavorite found[HA_MAX_MEDIA_FAVORITES] = {};
    size_t found_count = 0;

    char expandable_id[HA_MEDIA_CONTENT_ID_LEN] = {};
    char expandable_type[HA_MEDIA_CONTENT_TYPE_LEN] = {};
    bool preferred_expandable = false;

    if (!children.isNull()) {
        for (JsonObjectConst child : children) {
            const bool can_play = child["can_play"] | false;
            const bool can_expand = child["can_expand"] | false;
            const char *title = child["title"] | "";
            const char *content_id = child["media_content_id"] | "";
            const char *content_type = child["media_content_type"] | "";

            if (can_play && content_id[0] && content_type[0] &&
                found_count < HA_MAX_MEDIA_FAVORITES) {
                HomeAssistantMediaFavorite &item = found[found_count++];
                copy_text(item.entity_id, sizeof(item.entity_id), g_media_browse_active_entity_id);
                copy_text(item.title, sizeof(item.title), title[0] ? title : "Media");
                copy_text(item.media_content_id, sizeof(item.media_content_id), content_id);
                copy_text(item.media_content_type, sizeof(item.media_content_type), content_type);
            }

            if (g_media_browse_depth == 0 && can_expand && content_id[0] && content_type[0]) {
                String lower(title);
                lower.toLowerCase();
                const bool preferred = lower.indexOf("favorite") >= 0 ||
                                       lower.indexOf("playlist") >= 0;
                if (!expandable_id[0] || (preferred && !preferred_expandable)) {
                    copy_text(expandable_id, sizeof(expandable_id), content_id);
                    copy_text(expandable_type, sizeof(expandable_type), content_type);
                    preferred_expandable = preferred;
                }
            }
        }
    }

    if (found_count == 0 && g_media_browse_depth == 0 && expandable_id[0]) {
        copy_text(g_media_browse_content_id, sizeof(g_media_browse_content_id), expandable_id);
        copy_text(g_media_browse_content_type, sizeof(g_media_browse_content_type), expandable_type);
        send_media_browse_worker(g_media_browse_active_entity_id,
                                 g_media_browse_content_id,
                                 g_media_browse_content_type,
                                 1);
        return;
    }

    portENTER_CRITICAL(&g_mux);
    memset(g_media_favorites, 0, sizeof(g_media_favorites));
    g_media_favorite_count = found_count;
    for (size_t i = 0; i < found_count; ++i) g_media_favorites[i] = found[i];
    portEXIT_CRITICAL(&g_mux);
}

void handle_area_result_worker(JsonDocument &doc) {
    const bool success = doc["success"] | false;
    const PanelConfig &cfg = config_service_get();

    if (!success) {
        // The configured value may already be a valid HA area ID. Try it directly
        // so a non-admin/read-restricted registry response does not block controls.
        copy_text(g_resolved_area_id, sizeof(g_resolved_area_id), cfg.area_id);
        copy_text(g_resolved_area_name, sizeof(g_resolved_area_name), cfg.area_id);
        send_extract_target_worker();
        return;
    }

    JsonArrayConst areas = doc["result"].as<JsonArrayConst>();
    String requested(cfg.area_id);
    bool found = false;

    for (JsonObjectConst area : areas) {
        const char *area_id = area["area_id"] | "";
        const char *name = area["name"] | "";
        bool match = requested == area_id || requested.equalsIgnoreCase(name);

        if (!match) {
            JsonArrayConst aliases = area["aliases"].as<JsonArrayConst>();
            for (JsonVariantConst alias : aliases) {
                const char *value = alias.as<const char *>();
                if (value && requested.equalsIgnoreCase(value)) {
                    match = true;
                    break;
                }
            }
        }

        if (match) {
            copy_text(g_resolved_area_id, sizeof(g_resolved_area_id), area_id);
            copy_text(g_resolved_area_name, sizeof(g_resolved_area_name), name[0] ? name : area_id);
            found = true;
            break;
        }
    }

    if (!found) {
        portENTER_CRITICAL(&g_mux);
        g_discovery.discovery_complete = true;
        g_discovery.area_found = false;
        g_discovery.last_discovery_ms = millis();
        copy_text(g_discovery.message, sizeof(g_discovery.message), "Configured area was not found in Home Assistant.");
        portEXIT_CRITICAL(&g_mux);
        return;
    }

    portENTER_CRITICAL(&g_mux);
    g_discovery.area_found = true;
    copy_text(g_discovery.area_id, sizeof(g_discovery.area_id), g_resolved_area_id);
    copy_text(g_discovery.area_name, sizeof(g_discovery.area_name), g_resolved_area_name);
    portEXIT_CRITICAL(&g_mux);

    send_extract_target_worker();
}

void handle_extract_result_worker(JsonDocument &doc) {
    const bool success = doc["success"] | false;
    if (!success) {
        portENTER_CRITICAL(&g_mux);
        g_discovery.discovery_complete = true;
        g_discovery.last_discovery_ms = millis();
        copy_text(g_discovery.message, sizeof(g_discovery.message), "Home Assistant could not resolve entities for this area.");
        portEXIT_CRITICAL(&g_mux);
        return;
    }

    JsonObjectConst result = doc["result"].as<JsonObjectConst>();
    JsonArrayConst missing_areas = result["missing_areas"].as<JsonArrayConst>();
    if (!missing_areas.isNull() && missing_areas.size() > 0) {
        portENTER_CRITICAL(&g_mux);
        g_discovery.discovery_complete = true;
        g_discovery.area_found = false;
        g_discovery.last_discovery_ms = millis();
        copy_text(g_discovery.message, sizeof(g_discovery.message), "Configured Home Assistant area is missing.");
        portEXIT_CRITICAL(&g_mux);
        return;
    }

    portENTER_CRITICAL(&g_mux);
    g_entity_count = 0;
    portEXIT_CRITICAL(&g_mux);
    bool truncated = false;
    JsonArrayConst entities = result["referenced_entities"].as<JsonArrayConst>();
    for (JsonVariantConst item : entities) {
        const char *entity_id = item.as<const char *>();
        if (!entity_id || !entity_id[0]) continue;

        char domain[16] = {};
        domain_from_entity_id(entity_id, domain, sizeof(domain));
        if (!is_supported_domain(domain)) continue;
        if (!g_full_discovery_requested && !is_layout_entity(entity_id)) continue;

        portENTER_CRITICAL(&g_mux);
        if (g_entity_count >= HA_MAX_AREA_ENTITIES) {
            truncated = true;
            portEXIT_CRITICAL(&g_mux);
            break;
        }

        HaEntityModel &model = g_entities[g_entity_count++];
        memset(&model, 0, sizeof(model));
        copy_text(model.entity_id, sizeof(model.entity_id), entity_id);
        copy_text(model.domain, sizeof(model.domain), domain);
        fallback_name_from_id(entity_id, model.name, sizeof(model.name));
        copy_text(model.state, sizeof(model.state), "unknown");
        model.position_pct = -1;
        model.available = false;
        portEXIT_CRITICAL(&g_mux);
    }

    JsonArrayConst devices = result["referenced_devices"].as<JsonArrayConst>();
    const uint16_t device_count = devices.isNull() ? 0 : static_cast<uint16_t>(devices.size());

    portENTER_CRITICAL(&g_mux);
    g_discovery.area_found = true;
    g_discovery.device_count = device_count;
    update_discovery_counts_locked();
    copy_text(g_discovery.area_id, sizeof(g_discovery.area_id), g_resolved_area_id);
    copy_text(g_discovery.area_name, sizeof(g_discovery.area_name), g_resolved_area_name);
    if (truncated) {
        copy_text(g_discovery.message, sizeof(g_discovery.message), "Entity list truncated; subscribing to first supported controls.");
    }
    portEXIT_CRITICAL(&g_mux);

    send_subscribe_entities_worker();
}

void handle_entity_event_worker(JsonDocument &doc) {
    JsonObjectConst event = doc["event"].as<JsonObjectConst>();
    if (event.isNull()) return;

    JsonObjectConst additions = event["a"].as<JsonObjectConst>();
    if (!additions.isNull()) {
        for (JsonPairConst pair : additions) {
            apply_full_state_worker(pair.key().c_str(), pair.value().as<JsonObjectConst>());
        }
    }

    JsonObjectConst changes = event["c"].as<JsonObjectConst>();
    if (!changes.isNull()) {
        for (JsonPairConst pair : changes) {
            apply_diff_worker(pair.key().c_str(), pair.value().as<JsonObjectConst>());
        }
    }

    JsonArrayConst removals = event["r"].as<JsonArrayConst>();
    if (!removals.isNull()) {
        for (JsonVariantConst item : removals) {
            const char *entity_id = item.as<const char *>();
            portENTER_CRITICAL(&g_mux);
            HaEntityModel *model = find_entity_worker(entity_id);
            if (model) {
                copy_text(model->state, sizeof(model->state), "unavailable");
                model->available = false;
            }
            portEXIT_CRITICAL(&g_mux);
        }
    }

    const uint32_t now = millis();
    portENTER_CRITICAL(&g_mux);
    g_discovery.discovery_complete = true;
    g_discovery.area_found = true;
    g_discovery.last_discovery_ms = now;
    g_discovery.last_state_ms = now;
    update_discovery_counts_locked();
    snprintf(g_discovery.message, sizeof(g_discovery.message), "Live: %u supported entities in %s",
             static_cast<unsigned>(g_entity_count),
             g_resolved_area_name[0] ? g_resolved_area_name : g_resolved_area_id);
    portEXIT_CRITICAL(&g_mux);
}

void handle_action_result_worker(JsonDocument &doc) {
    const bool success = doc["success"] | false;
    record_action_result(g_action_description[0] ? g_action_description : "Home Assistant action",
                         success ? 200 : 500);
    g_action_in_flight = false;
    g_action_request_id = 0;
    g_action_started_ms = 0;
    g_action_description[0] = '\0';
}

void refresh_entity_subscription_worker() {
    if (!g_ws_authenticated || !g_subscribe_request_id || g_entity_count == 0) return;

    JsonDocument unsubscribe;
    unsubscribe["id"] = next_ws_id();
    unsubscribe["type"] = "unsubscribe_events";
    unsubscribe["subscription"] = g_subscribe_request_id;
    send_json(unsubscribe);
    Serial0.println("[HA] Refreshing live entity state subscription after idle interval");
    send_subscribe_entities_worker();
}

void handle_ws_text_worker(uint8_t *payload, size_t length) {
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        set_discovery_message("Home Assistant WebSocket returned invalid JSON.");
        return;
    }

    const char *type = doc["type"] | "";

    if (strcmp(type, "auth_required") == 0) {
        char token[sizeof(g_token)] = {};
        char base_unused[sizeof(g_base_url)] = {};
        credentials_snapshot(base_unused, sizeof(base_unused), token, sizeof(token));

        JsonDocument auth;
        auth["type"] = "auth";
        auth["access_token"] = token;
        send_json(auth);
        return;
    }

    if (strcmp(type, "auth_ok") == 0) {
        g_ws_authenticated = true;
        portENTER_CRITICAL(&g_mux);
        g_discovery.websocket_authenticated = true;
        g_status.authenticated = true;
        copy_text(g_status.message, sizeof(g_status.message), "Home Assistant WebSocket authenticated");
        if (g_resume_entities_after_reconnect && g_entity_count) {
            g_resubscribe_requested = true;
        } else {
            g_discovery_requested = true;
        }
        portEXIT_CRITICAL(&g_mux);
        return;
    }

    if (strcmp(type, "auth_invalid") == 0) {
        g_ws_authenticated = false;
        portENTER_CRITICAL(&g_mux);
        g_discovery.websocket_authenticated = false;
        g_discovery.discovery_complete = false;
        g_status.authenticated = false;
        copy_text(g_status.message, sizeof(g_status.message), "Home Assistant token rejected");
        copy_text(g_discovery.message, sizeof(g_discovery.message), "WebSocket authentication failed.");
        portEXIT_CRITICAL(&g_mux);
        return;
    }

    if (strcmp(type, "result") == 0) {
        const uint32_t id = doc["id"] | 0U;
        if (id == g_area_request_id) {
            handle_area_result_worker(doc);
        } else if (id == g_extract_request_id) {
            handle_extract_result_worker(doc);
        } else if (id == g_entity_registry_request_id) {
            handle_entity_registry_result_worker(doc);
        } else if (id == g_subscribe_request_id) {
            const bool success = doc["success"] | false;
            if (!success) set_discovery_message("Home Assistant rejected live entity subscription.");
        } else if (id == g_media_browse_request_id) {
            handle_media_browse_result_worker(doc);
        } else if (id == g_weather_hourly_request_id) {
            handle_weather_forecast_result_worker(doc, true);
        } else if (id == g_weather_daily_request_id) {
            handle_weather_forecast_result_worker(doc, false);
        } else if (id == g_action_request_id && g_action_in_flight) {
            handle_action_result_worker(doc);
        } else {
            for (uint8_t i = 0; i < PANEL_MAX_CALENDARS; ++i) {
                if (id && id == g_calendar_request_ids[i]) {
                    handle_calendar_result_worker(doc, i);
                    break;
                }
            }
        }
        return;
    }

    if (strcmp(type, "event") == 0) {
        const uint32_t id = doc["id"] | 0U;
        if (id == g_subscribe_request_id) handle_entity_event_worker(doc);
    }
}

void ws_event_worker(WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            portENTER_CRITICAL(&g_mux);
            g_discovery.websocket_connected = true;
            copy_text(g_discovery.message, sizeof(g_discovery.message), "WebSocket connected; authenticating...");
            portEXIT_CRITICAL(&g_mux);
            break;
        case WStype_DISCONNECTED:
            g_ws_authenticated = false;
            g_weather_hourly_request_id = 0;
            g_weather_daily_request_id = 0;
            g_weather_forecast_started_ms = 0;
            g_weather_forecast_retry_not_before_ms = millis() + HA_WEATHER_FORECAST_RETRY_MS;
            memset(g_calendar_request_ids, 0, sizeof(g_calendar_request_ids));
            g_calendar_request_started_ms = 0;
            g_calendar_retry_not_before_ms = millis() + HA_CALENDAR_RETRY_MS;
            portENTER_CRITICAL(&g_mux);
            g_weather_cache.forecasts_loading = false;
            g_calendar_cache.loading = false;
            portEXIT_CRITICAL(&g_mux);
            if (g_action_in_flight) {
                record_action_result(g_action_description[0] ? g_action_description : "Home Assistant action", -102);
                g_action_in_flight = false;
                g_action_request_id = 0;
                g_action_started_ms = 0;
                g_action_description[0] = '\0';
            }
            portENTER_CRITICAL(&g_mux);
            g_discovery.websocket_connected = false;
            g_discovery.websocket_authenticated = false;
            g_discovery.discovery_complete = false;
            copy_text(g_discovery.message, sizeof(g_discovery.message), "WebSocket disconnected; reconnecting...");
            portEXIT_CRITICAL(&g_mux);
            break;
        case WStype_TEXT:
            handle_ws_text_worker(payload, length);
            break;
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            // The Arduino WebSockets library delivers frames beyond its normal
            // buffer as fragments. Do not parse partial JSON or keep an
            // unbounded assembly buffer on this display controller.
            set_discovery_message("Home Assistant response exceeded the panel WebSocket buffer.");
            break;
        default:
            break;
    }
}

void start_websocket_worker() {
    if (!configured_snapshot()) return;

    HaEndpoint endpoint;
    if (!parse_endpoint(endpoint)) {
        set_discovery_message("Invalid Home Assistant base URL.");
        return;
    }

    g_ws.disconnect();
    g_ws_started = true;
    g_ws_authenticated = false;
    if (g_resume_entities_after_reconnect && g_entity_count) {
        portENTER_CRITICAL(&g_mux);
        g_discovery.websocket_connected = false;
        g_discovery.websocket_authenticated = false;
        copy_text(g_discovery.message, sizeof(g_discovery.message),
                  "Reconnecting Home Assistant after media artwork...");
        portEXIT_CRITICAL(&g_mux);
    } else {
        reset_discovery_state("Connecting to Home Assistant WebSocket...");
    }

    g_ws.onEvent(ws_event_worker);
    g_ws.setReconnectInterval(HA_WS_RECONNECT_MS);
    g_ws.enableHeartbeat(30000, 5000, 2);

    // Home Assistant does not require a WebSocket subprotocol, so pass "".
    if (endpoint.secure) {
        g_ws.beginSSL(endpoint.host.c_str(), endpoint.port, endpoint.websocket_path.c_str(), nullptr, "");
    } else {
        g_ws.begin(endpoint.host.c_str(), endpoint.port, endpoint.websocket_path.c_str(), "");
    }

    Serial0.printf("[HA] WebSocket %s://%s:%u%s\n",
                   endpoint.secure ? "wss" : "ws",
                   endpoint.host.c_str(),
                   static_cast<unsigned>(endpoint.port),
                   endpoint.websocket_path.c_str());
}

void stop_websocket_worker(const char *message) {
    if (g_ws_started) g_ws.disconnect();
    g_ws_started = false;
    g_ws_authenticated = false;
    reset_discovery_state(message);
}

int http_get_api(String &payload) {
    payload = "";
    char base[sizeof(g_base_url)] = {};
    char token[sizeof(g_token)] = {};
    credentials_snapshot(base, sizeof(base), token, sizeof(token));
    if (!base[0] || !token[0]) return -100;

    String base_url(base);
    base_url.trim();
    while (base_url.endsWith("/")) base_url.remove(base_url.length() - 1);
    const String url = base_url + "/api/";

    HTTPClient http;
    http.setConnectTimeout(HA_HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(HA_HTTP_TIMEOUT_MS);

    int code = -1;
    if (url.startsWith("https://")) {
        WiFiClientSecure client;
#if HA_TLS_ALLOW_INSECURE
        client.setInsecure();
#endif
        if (!http.begin(client, url)) return -101;
        http.addHeader("Authorization", String("Bearer ") + token);
        code = http.GET();
        if (code > 0) payload = http.getString();
        http.end();
    } else {
        WiFiClient client;
        if (!http.begin(client, url)) return -101;
        http.addHeader("Authorization", String("Bearer ") + token);
        code = http.GET();
        if (code > 0) payload = http.getString();
        http.end();
    }
    return code;
}

int http_post_template_worker(const char *ha_template, String &payload) {
    payload = "";
    if (!ha_template) return -100;

    char base[sizeof(g_base_url)] = {};
    char token[sizeof(g_token)] = {};
    credentials_snapshot(base, sizeof(base), token, sizeof(token));
    if (!base[0] || !token[0]) return -100;

    String base_url(base);
    base_url.trim();
    while (base_url.endsWith("/")) base_url.remove(base_url.length() - 1);
    const String url = base_url + "/api/template";

    JsonDocument request_doc;
    request_doc["template"] = ha_template;
    String request_body;
    serializeJson(request_doc, request_body);

    HTTPClient http;
    http.setConnectTimeout(HA_HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(HA_HTTP_TIMEOUT_MS);

    int code = -1;
    if (url.startsWith("https://")) {
        WiFiClientSecure client;
#if HA_TLS_ALLOW_INSECURE
        client.setInsecure();
#endif
        if (!http.begin(client, url)) return -101;
        http.addHeader("Authorization", String("Bearer ") + token);
        http.addHeader("Content-Type", "application/json");
        code = http.POST(request_body);
        if (code >= 200 && code < 300) payload = http.getString();
        http.end();
    } else {
        WiFiClient client;
        if (!http.begin(client, url)) return -101;
        http.addHeader("Authorization", String("Bearer ") + token);
        http.addHeader("Content-Type", "application/json");
        code = http.POST(request_body);
        if (code >= 200 && code < 300) payload = http.getString();
        http.end();
    }
    return code;
}

void run_rest_discovery_worker() {
    // Home Assistant renders the compact picker result.  Fetching /api/states
    // directly required the panel to allocate every state on the installation,
    // which is why large installations reported the opaque -103 failure.
    static const char entity_picker_template[] =
        "{% set ns = namespace(items=[]) %}"
        "{% for s in states if s.domain in ['light','switch','fan','cover','lock','binary_sensor','sensor','scene','media_player','weather','calendar','timer','climate','alarm_control_panel','vacuum','device_tracker','person','input_boolean'] %}"
        "{% if ns.items | length < 161 %}"
        "{% set ns.items = ns.items + [{'entity_id': s.entity_id, 'name': s.name, 'state': s.state}] %}"
        "{% endif %}"
        "{% endfor %}{{ ns.items | to_json }}";

    set_discovery_message("Searching Home Assistant directly...");
    String payload;
    const uint32_t started = millis();
    const int code = http_post_template_worker(entity_picker_template, payload);
    if (code < 200 || code >= 300) {
        char message[128] = {};
        snprintf(message, sizeof(message), "Direct Home Assistant search failed (%d).", code);
        set_discovery_message(message);
        return;
    }

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        char message[128] = {};
        snprintf(message, sizeof(message), "Direct search response could not be read: %s.", error.c_str());
        set_discovery_message(message);
        Serial0.printf("[HA] REST entity search JSON error: %s\n", error.c_str());
        return;
    }

    JsonArrayConst states = doc.as<JsonArrayConst>();
    if (states.isNull()) {
        set_discovery_message("Direct Home Assistant search returned an invalid control list.");
        return;
    }

    size_t count = 0;
    bool truncated = false;
    portENTER_CRITICAL(&g_mux);
    g_entity_count = 0;
    for (JsonObjectConst item : states) {
        const char *entity_id = item["entity_id"] | "";
        if (!entity_id[0]) continue;
        char domain[16] = {};
        domain_from_entity_id(entity_id, domain, sizeof(domain));
        if (!is_supported_domain(domain)) continue;
        if (g_entity_count >= HA_MAX_AREA_ENTITIES) {
            truncated = true;
            break;
        }
        HaEntityModel &model = g_entities[g_entity_count++];
        memset(&model, 0, sizeof(model));
        copy_text(model.entity_id, sizeof(model.entity_id), entity_id);
        copy_text(model.domain, sizeof(model.domain), domain);
        const char *name = item["name"] | "";
        if (name[0]) copy_text(model.name, sizeof(model.name), name);
        else fallback_name_from_id(entity_id, model.name, sizeof(model.name));
        const char *state = item["state"] | "unknown";
        copy_text(model.state, sizeof(model.state), state);
        model.available = strcmp(state, "unavailable") != 0 && strcmp(state, "unknown") != 0;
        model.position_pct = -1;
        ++count;
    }
    g_discovery.discovery_complete = true;
    g_discovery.area_found = true;
    g_discovery.device_count = static_cast<uint16_t>(states.size());
    update_discovery_counts_locked();
    g_discovery.last_discovery_ms = millis();
    copy_text(g_discovery.area_id, sizeof(g_discovery.area_id), "all");
    copy_text(g_discovery.area_name, sizeof(g_discovery.area_name), "All Home Assistant");
    snprintf(g_discovery.message, sizeof(g_discovery.message),
             truncated ? "Direct search found %u supported controls (limited to %u)." :
                         "Direct search found %u supported controls.",
             static_cast<unsigned>(count), static_cast<unsigned>(HA_MAX_AREA_ENTITIES));
    portEXIT_CRITICAL(&g_mux);
    Serial0.printf("[HA] REST entity search: %u supported controls in %lums\n",
                   static_cast<unsigned>(count), static_cast<unsigned long>(millis() - started));
}

int http_post_service(const char *domain, const char *service, const String &body) {
    char base[sizeof(g_base_url)] = {};
    char token[sizeof(g_token)] = {};
    credentials_snapshot(base, sizeof(base), token, sizeof(token));
    if (!base[0] || !token[0] || !domain || !service) return -100;

    String base_url(base);
    base_url.trim();
    while (base_url.endsWith("/")) base_url.remove(base_url.length() - 1);
    const String url = base_url + "/api/services/" + domain + "/" + service;

    HTTPClient http;
    http.setConnectTimeout(HA_HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(HA_HTTP_TIMEOUT_MS);

    int code = -1;
    if (url.startsWith("https://")) {
        WiFiClientSecure client;
#if HA_TLS_ALLOW_INSECURE
        client.setInsecure();
#endif
        if (!http.begin(client, url)) return -101;
        http.addHeader("Authorization", String("Bearer ") + token);
        http.addHeader("Content-Type", "application/json");
        code = http.POST(body);
        http.end();
    } else {
        WiFiClient client;
        if (!http.begin(client, url)) return -101;
        http.addHeader("Authorization", String("Bearer ") + token);
        http.addHeader("Content-Type", "application/json");
        code = http.POST(body);
        http.end();
    }
    return code;
}


HomeAssistantArtworkFormat detect_artwork_format(const uint8_t *data, size_t size,
                                                   uint16_t &width, uint16_t &height) {
    width = 0;
    height = 0;
    if (!data || size < 16) return HomeAssistantArtworkFormat::None;

    static const uint8_t png_magic[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (size >= 24 && memcmp(data, png_magic, sizeof(png_magic)) == 0) {
        const uint32_t w = (static_cast<uint32_t>(data[16]) << 24) |
                           (static_cast<uint32_t>(data[17]) << 16) |
                           (static_cast<uint32_t>(data[18]) << 8) |
                           static_cast<uint32_t>(data[19]);
        const uint32_t h = (static_cast<uint32_t>(data[20]) << 24) |
                           (static_cast<uint32_t>(data[21]) << 16) |
                           (static_cast<uint32_t>(data[22]) << 8) |
                           static_cast<uint32_t>(data[23]);
        if (w > 0 && h > 0 && w <= 2048 && h <= 2048) {
            width = static_cast<uint16_t>(w);
            height = static_cast<uint16_t>(h);
            return HomeAssistantArtworkFormat::Png;
        }
        return HomeAssistantArtworkFormat::None;
    }

    if (data[0] == 0xFF && data[1] == 0xD8) {
        size_t i = 2;
        while (i + 8 < size) {
            if (data[i] != 0xFF) {
                ++i;
                continue;
            }
            while (i < size && data[i] == 0xFF) ++i;
            if (i >= size) break;
            const uint8_t marker = data[i++];
            if (marker == 0xD8 || marker == 0xD9 || (marker >= 0xD0 && marker <= 0xD7)) continue;
            if (i + 1 >= size) break;
            const uint16_t segment_len = (static_cast<uint16_t>(data[i]) << 8) | data[i + 1];
            if (segment_len < 2 || i + segment_len > size) break;

            const bool sof = (marker >= 0xC0 && marker <= 0xC3) ||
                             (marker >= 0xC5 && marker <= 0xC7) ||
                             (marker >= 0xC9 && marker <= 0xCB) ||
                             (marker >= 0xCD && marker <= 0xCF);
            if (sof && segment_len >= 7) {
                const uint16_t h = (static_cast<uint16_t>(data[i + 3]) << 8) | data[i + 4];
                const uint16_t w = (static_cast<uint16_t>(data[i + 5]) << 8) | data[i + 6];
                if (w > 0 && h > 0 && w <= 4096 && h <= 4096) {
                    width = w;
                    height = h;
                    return HomeAssistantArtworkFormat::Jpeg;
                }
                break;
            }
            i += segment_len;
        }
    }

    return HomeAssistantArtworkFormat::None;
}

bool read_artwork_response(HTTPClient &http, uint8_t *&data, size_t &size) {
    data = nullptr;
    size = 0;
    const int declared = http.getSize();
    if (declared > static_cast<int>(HA_MEDIA_ARTWORK_MAX_BYTES)) {
        Serial0.printf("[HA] Media artwork rejected: Content-Length %d exceeds %u bytes\n",
                       declared, static_cast<unsigned>(HA_MEDIA_ARTWORK_MAX_BYTES));
        return false;
    }

    const size_t capacity = declared > 0 ? static_cast<size_t>(declared)
                                         : static_cast<size_t>(HA_MEDIA_ARTWORK_MAX_BYTES);
    data = static_cast<uint8_t *>(heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!data) data = static_cast<uint8_t *>(malloc(capacity));
    if (!data) return false;

    auto *stream = http.getStreamPtr();
    int remaining = declared;
    const uint32_t started = millis();

    // HTTPClient can report the socket closed while bytes from that response
    // are still buffered in its stream.  Drain available bytes first; testing
    // connected() in the loop condition used to discard intermittent media
    // proxy responses just before they became complete.
    while (size < capacity && (remaining > 0 || declared < 0)) {
        const size_t available = stream->available();
        if (available) {
            size_t chunk = available;
            if (chunk > capacity - size) chunk = capacity - size;
            if (remaining > 0 && chunk > static_cast<size_t>(remaining)) {
                chunk = static_cast<size_t>(remaining);
            }
            const size_t read = stream->readBytes(data + size, chunk);
            if (!read) break;
            size += read;
            if (remaining > 0) remaining -= static_cast<int>(read);
        } else {
            if (remaining == 0) break;
            if (!http.connected()) break;
            if (millis() - started >= HA_MEDIA_ARTWORK_TIMEOUT_MS) break;
            delay(1);
        }
    }

    const bool complete = size > 0 &&
                          (declared < 0 || remaining == 0) &&
                          !(size == capacity && stream->available() > 0);
    if (!complete) {
        Serial0.printf("[HA] Media artwork read incomplete: declared=%d received=%u\n",
                       declared, static_cast<unsigned>(size));
        free(data);
        data = nullptr;
        size = 0;
    }
    return complete;
}

int download_artwork_worker(const char *entity_id, const char *picture_url) {
    if (!entity_id || !entity_id[0] || !picture_url || !picture_url[0]) return -100;

    char token[sizeof(g_token)] = {};
    char base_buf[sizeof(g_base_url)] = {};
    credentials_snapshot(base_buf, sizeof(base_buf), token, sizeof(token));
    String base(base_buf);
    base.trim();
    while (base.endsWith("/")) base.remove(base.length() - 1);

    String url(picture_url);
    bool add_auth = false;
    if (url.startsWith("/")) {
        url = base + url;
        add_auth = true;
    } else if (url.startsWith(base)) {
        const size_t base_len = base.length();
        const bool exact_base = url.length() == base_len;
        const char next = exact_base ? '\0' : url[base_len];
        add_auth = exact_base || next == '/' || next == '?' || next == '#';
    }
    if (!url.startsWith("http://") && !url.startsWith("https://")) return -101;

    HTTPClient http;
    http.setConnectTimeout(HA_HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(HA_MEDIA_ARTWORK_TIMEOUT_MS);
    http.setReuse(false);
    // Arduino's redirect implementation reuses request headers.  Follow
    // redirects only for unauthenticated external artwork so an HA bearer
    // token can never be forwarded to a different redirect host.
    http.setFollowRedirects(add_auth ? HTTPC_DISABLE_FOLLOW_REDIRECTS
                                     : HTTPC_STRICT_FOLLOW_REDIRECTS);

    Serial0.printf("[HA] Media artwork download: entity=%s url_chars=%u auth=%s\n",
                   entity_id, static_cast<unsigned>(url.length()),
                   add_auth ? "yes" : "no");

    uint8_t *downloaded = nullptr;
    size_t downloaded_size = 0;
    int code = -1;

    if (url.startsWith("https://")) {
        WiFiClientSecure client;
#if HA_TLS_ALLOW_INSECURE
        client.setInsecure();
#endif
        if (!http.begin(client, url)) {
            Serial0.printf("[HA] Media artwork HTTP begin failed for %s\n", entity_id);
            return -101;
        }
        if (add_auth && token[0]) http.addHeader("Authorization", String("Bearer ") + token);
        code = http.GET();
        if (code >= 200 && code < 300) read_artwork_response(http, downloaded, downloaded_size);
        http.end();
    } else {
        WiFiClient client;
        if (!http.begin(client, url)) {
            Serial0.printf("[HA] Media artwork HTTP begin failed for %s\n", entity_id);
            return -101;
        }
        if (add_auth && token[0]) http.addHeader("Authorization", String("Bearer ") + token);
        code = http.GET();
        if (code >= 200 && code < 300) read_artwork_response(http, downloaded, downloaded_size);
        http.end();
    }

    if (code < 200 || code >= 300 || !downloaded || downloaded_size == 0) {
        Serial0.printf("[HA] Media artwork download failed: entity=%s http=%d bytes=%u\n",
                       entity_id, code, static_cast<unsigned>(downloaded_size));
        if (downloaded) free(downloaded);
        return code > 0 ? code : -102;
    }

    uint16_t width = 0, height = 0;
    const HomeAssistantArtworkFormat format =
        detect_artwork_format(downloaded, downloaded_size, width, height);
    if (format == HomeAssistantArtworkFormat::None) {
        Serial0.printf("[HA] Media artwork format unsupported: %u bytes, magic="
                       "%02X %02X %02X %02X %02X %02X %02X %02X\n",
                       static_cast<unsigned>(downloaded_size),
                       downloaded_size > 0 ? downloaded[0] : 0,
                       downloaded_size > 1 ? downloaded[1] : 0,
                       downloaded_size > 2 ? downloaded[2] : 0,
                       downloaded_size > 3 ? downloaded[3] : 0,
                       downloaded_size > 4 ? downloaded[4] : 0,
                       downloaded_size > 5 ? downloaded[5] : 0,
                       downloaded_size > 6 ? downloaded[6] : 0,
                       downloaded_size > 7 ? downloaded[7] : 0);
        free(downloaded);
        return -104;
    }

    if (!g_artwork_mutex ||
        xSemaphoreTake(g_artwork_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        free(downloaded);
        return -105;
    }

    uint8_t *old = g_artwork_data;
    g_artwork_data = downloaded;
    ++g_artwork_info.generation;
    if (g_artwork_info.generation == 0) g_artwork_info.generation = 1;
    g_artwork_info.data_size = static_cast<uint32_t>(downloaded_size);
    g_artwork_info.width = width;
    g_artwork_info.height = height;
    g_artwork_info.format = format;
    copy_text(g_artwork_info.entity_id, sizeof(g_artwork_info.entity_id), entity_id);
    copy_text(g_artwork_info.picture_url, sizeof(g_artwork_info.picture_url), picture_url);
    xSemaphoreGive(g_artwork_mutex);

    if (old) free(old);
    Serial0.printf("[HA] Media artwork cached: %s, %u bytes, %ux%u\n",
                   format == HomeAssistantArtworkFormat::Jpeg ? "JPEG" : "PNG",
                   static_cast<unsigned>(downloaded_size),
                   static_cast<unsigned>(width), static_cast<unsigned>(height));
    return code;
}

String entity_target_body(const char *entity_id) {
    JsonDocument doc;
    doc["entity_id"] = entity_id;
    String body;
    serializeJson(doc, body);
    return body;
}

String area_light_body(bool include_brightness, uint8_t brightness_pct) {
    JsonDocument doc;
    JsonArray ids = doc["entity_id"].to<JsonArray>();
    for (size_t i = 0; i < g_entity_count; ++i) {
        if (strcmp(g_entities[i].domain, "light") == 0 && g_entities[i].available) {
            ids.add(g_entities[i].entity_id);
        }
    }
    if (include_brightness) doc["brightness_pct"] = brightness_pct;
    String body;
    serializeJson(doc, body);
    return body;
}

void record_action_result(const char *description, int code) {
    portENTER_CRITICAL(&g_mux);
    g_discovery.last_action_http_code = code;
    g_discovery.last_action_ms = millis();
    snprintf(g_discovery.last_action, sizeof(g_discovery.last_action), "%s | HTTP %d",
             description ? description : "Home Assistant action", code);
    portEXIT_CRITICAL(&g_mux);
}

bool action_is_idempotent(const HaAction &action) {
    switch (action.type) {
        case HaActionType::Toggle:
        case HaActionType::AreaBrightness:
        case HaActionType::LightBrightness:
        case HaActionType::FanSpeed:
        case HaActionType::AllLights:
        case HaActionType::Scene:
        case HaActionType::MediaVolume:
        case HaActionType::MediaMute:
        case HaActionType::MediaSource:
            return true;
        case HaActionType::MediaPlayPause:
        case HaActionType::MediaPrevious:
        case HaActionType::MediaNext:
        case HaActionType::MediaVolumeUp:
        case HaActionType::MediaVolumeDown:
        case HaActionType::MediaFavorite:
            return false;
    }
    return false;
}

bool is_transient_http_error(int code) {
    // HTTPClient negative values are connection/read failures.  Do not retry
    // authorization and validation failures, which require user correction.
    return (code < 0 && code >= -11) || code == 408 || code == 429 ||
           (code >= 500 && code <= 599);
}

int call_service_with_recovery(const HaAction &action, const char *domain,
                               const char *service, const String &body,
                               bool &retried) {
    retried = false;
    if (!g_ws_authenticated || g_action_in_flight) return -102;

    JsonDocument service_data;
    if (deserializeJson(service_data, body)) return -104;

    JsonDocument request;
    g_action_request_id = next_ws_id();
    request["id"] = g_action_request_id;
    request["type"] = "call_service";
    request["domain"] = domain;
    request["service"] = service;
    request["service_data"] = service_data.as<JsonObjectConst>();
    if (!send_json(request)) {
        g_action_request_id = 0;
        return -101;
    }
    g_action_in_flight = true;
    g_action_started_ms = millis();
    return 202;
}

void process_action_worker(const HaAction &action) {
    int code = -100;
    char description[96] = {};
    bool retried = false;

    if (action.type == HaActionType::Toggle) {
        HaEntityModel *model = find_entity_worker(action.entity_id);
        if (!model || (!is_control_domain(model->domain) && strcmp(model->domain, "lock") != 0)) {
            record_action_result("Entity no longer available", -103);
            return;
        }

        const char *service = nullptr;
        if (strcmp(model->domain, "cover") == 0) {
            const bool open = strcmp(model->state, "open") == 0 || strcmp(model->state, "opening") == 0;
            service = open ? "close_cover" : "open_cover";
        } else if (strcmp(model->domain, "lock") == 0) {
            service = strcmp(model->state, "locked") == 0 ? "unlock" : "lock";
        } else {
            service = strcmp(model->state, "on") == 0 ? "turn_off" : "turn_on";
        }

        code = call_service_with_recovery(action, model->domain, service,
                                          entity_target_body(model->entity_id), retried);
        snprintf(description, sizeof(description), "%s %s", model->name, service);
    } else if (action.type == HaActionType::LightBrightness) {
        HaEntityModel *model = find_entity_worker(action.entity_id);
        if (!model || !model->available || strcmp(model->domain, "light") != 0 || !model->supports_brightness) {
            record_action_result("Light no longer available", -103); return;
        }
        JsonDocument doc;
        doc["entity_id"] = model->entity_id;
        if (action.value) doc["brightness_pct"] = action.value;
        String body; serializeJson(doc, body);
        code = call_service_with_recovery(action, "light", action.value ? "turn_on" : "turn_off",
                                          body, retried);
        snprintf(description, sizeof(description), "%s brightness", model->name);
    } else if (action.type == HaActionType::FanSpeed) {
        HaEntityModel *model = find_entity_worker(action.entity_id);
        if (!model || !model->available || strcmp(model->domain, "fan") != 0) {
            record_action_result("Fan no longer available", -103); return;
        }
        JsonDocument doc;
        doc["entity_id"] = model->entity_id;
        if (action.value) doc["percentage"] = action.value;
        String body; serializeJson(doc, body);
        code = call_service_with_recovery(action, "fan", action.value ? "set_percentage" : "turn_off",
                                          body, retried);
        snprintf(description, sizeof(description), "%s fan speed", model->name);
    } else if (action.type == HaActionType::AreaBrightness) {
        const bool turn_off = action.value == 0;
        code = call_service_with_recovery(action, "light", turn_off ? "turn_off" : "turn_on",
                                          area_light_body(!turn_off, action.value), retried);
        snprintf(description, sizeof(description), "Area lights %s",
                 turn_off ? "off" : "brightness");
    } else if (action.type == HaActionType::AllLights) {
        code = call_service_with_recovery(action, "light", action.flag ? "turn_on" : "turn_off",
                                          area_light_body(false, 0), retried);
        snprintf(description, sizeof(description), "All lights %s", action.flag ? "on" : "off");
    } else if (action.type == HaActionType::Scene) {
        HaEntityModel *model = find_entity_worker(action.entity_id);
        if (!model || strcmp(model->domain, "scene") != 0) {
            record_action_result("Scene no longer available", -103);
            return;
        }
        code = call_service_with_recovery(action, "scene", "turn_on",
                                          entity_target_body(model->entity_id), retried);
        snprintf(description, sizeof(description), "Scene %s", model->name);
    } else {
        HaEntityModel *model = find_entity_worker(action.entity_id);
        if (!model || strcmp(model->domain, "media_player") != 0 || !model->available) {
            record_action_result("Media player no longer available", -103);
            return;
        }

        JsonDocument doc;
        doc["entity_id"] = model->entity_id;
        const char *service = nullptr;

        switch (action.type) {
            case HaActionType::MediaPlayPause:
                service = "media_play_pause";
                break;
            case HaActionType::MediaPrevious:
                service = "media_previous_track";
                break;
            case HaActionType::MediaNext:
                service = "media_next_track";
                break;
            case HaActionType::MediaVolume:
                service = "volume_set";
                doc["volume_level"] = static_cast<float>(action.value) / 100.0f;
                break;
            case HaActionType::MediaVolumeUp:
                service = "volume_up";
                break;
            case HaActionType::MediaVolumeDown:
                service = "volume_down";
                break;
            case HaActionType::MediaMute:
                service = "volume_mute";
                doc["is_volume_muted"] = action.flag;
                break;
            case HaActionType::MediaSource:
                service = "select_source";
                doc["source"] = action.text;
                break;
            case HaActionType::MediaFavorite:
                service = "play_media";
                {
                    JsonObject media = doc["media"].to<JsonObject>();
                    media["media_content_id"] = action.text;
                    media["media_content_type"] = action.aux;
                    media["metadata"].to<JsonObject>();
                }
                break;
            default:
                record_action_result("Unsupported media action", -103);
                return;
        }

        String body;
        serializeJson(doc, body);
        code = call_service_with_recovery(action, "media_player", service, body, retried);
        snprintf(description, sizeof(description), "%s %s", model->name, service);
    }

    if (code == 202) copy_text(g_action_description, sizeof(g_action_description), description);
    record_action_result(description, code);
}

void run_health_check_worker() {
    portENTER_CRITICAL(&g_mux);
    g_health_in_progress = true;
    g_status.request_in_progress = true;
    portEXIT_CRITICAL(&g_mux);

    HomeAssistantStatus result = {};
    result.configured = configured_snapshot();
    result.connected = network_service_connected();

    const uint32_t started = millis();
    String payload;
    result.http_code = result.connected ? http_get_api(payload) : -102;
    result.latency_ms = millis() - started;
    result.authenticated = result.http_code >= 200 && result.http_code < 300;

    if (result.authenticated) {
        result.last_success_ms = millis();
        snprintf(result.message, sizeof(result.message), "connected | HTTP %d | %lums",
                 result.http_code, static_cast<unsigned long>(result.latency_ms));
    } else if (!result.configured) {
        copy_text(result.message, sizeof(result.message), "not configured");
    } else if (!result.connected) {
        copy_text(result.message, sizeof(result.message), "Wi-Fi offline");
    } else if (result.http_code == 401) {
        copy_text(result.message, sizeof(result.message), "token rejected (401)");
    } else {
        snprintf(result.message, sizeof(result.message), "connection error %d", result.http_code);
    }

    portENTER_CRITICAL(&g_mux);
    const uint32_t previous_success = g_status.last_success_ms;
    const bool ws_auth = g_discovery.websocket_authenticated;
    g_status = result;
    if (!g_status.last_success_ms) g_status.last_success_ms = previous_success;
    if (ws_auth) g_status.authenticated = true;
    g_health_in_progress = false;
    g_status.request_in_progress = false;
    portEXIT_CRITICAL(&g_mux);
}

void load_preferences() {
    Preferences prefs;
    if (!prefs.begin("panel_ha", true)) return;
    const String url = prefs.getString("url", "");
    const String token = prefs.getString("token", "");
    prefs.end();

    portENTER_CRITICAL(&g_mux);
    copy_text(g_base_url, sizeof(g_base_url), url.c_str());
    copy_text(g_token, sizeof(g_token), token.c_str());
    portEXIT_CRITICAL(&g_mux);
}


bool take_media_browse_request_worker(char *entity_id, size_t entity_len,
                                      char *content_id, size_t content_len,
                                      char *content_type, size_t type_len) {
    bool requested = false;
    portENTER_CRITICAL(&g_mux);
    if (g_media_browse_requested) {
        g_media_browse_requested = false;
        copy_text(entity_id, entity_len, g_media_browse_entity_id);
        copy_text(content_id, content_len, g_media_browse_content_id);
        copy_text(content_type, type_len, g_media_browse_content_type);
        requested = true;
    }
    portEXIT_CRITICAL(&g_mux);
    return requested;
}

bool take_artwork_request_worker(char *entity_id, size_t entity_len,
                                 char *picture_url, size_t picture_len) {
    bool requested = false;
    portENTER_CRITICAL(&g_mux);
    if (g_artwork_requested) {
        g_artwork_requested = false;
        copy_text(entity_id, entity_len, g_artwork_request_entity_id);
        copy_text(picture_url, picture_len, g_artwork_request_url);
        requested = true;
    }
    portEXIT_CRITICAL(&g_mux);
    return requested;
}

void worker_task(void *) {
    for (;;) {
        if (!network_service_connected() || !network_ready_snapshot()) {
            if (g_ws_started) stop_websocket_worker("Waiting for stable Wi-Fi...");
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // A full editor search is a one-shot REST request rather than a WSS
        // registry request.  Release the WSS TLS session first: ESP32-P4 has
        // a small DMA-capable TLS pool, and this also lets search succeed when
        // the subscription itself is in a reconnect loop.
        if (take_flag(g_rest_discovery_requested)) {
            if (g_ws_started) {
                g_ws.disconnect();
                g_ws_started = false;
                g_ws_authenticated = false;
                portENTER_CRITICAL(&g_mux);
                g_discovery.websocket_connected = false;
                g_discovery.websocket_authenticated = false;
                g_discovery.discovery_complete = false;
                portEXIT_CRITICAL(&g_mux);
                vTaskDelay(pdMS_TO_TICKS(HA_TLS_RELEASE_SETTLE_MS));
            }
            g_resume_entities_after_reconnect = false;
            g_resubscribe_requested = false;
            run_rest_discovery_worker();
            // Keep the completed REST result available long enough for the
            // browser's poll to consume it.  The live subscription will then
            // reconnect normally and rebuild only the saved panel layout.
            g_ws_restart_not_before_ms = millis() + HA_REST_DISCOVERY_RECONNECT_DELAY_MS;
            g_last_http_ms = millis();
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        if (take_flag(g_reconnect_requested)) {
            stop_websocket_worker("Home Assistant credentials changed; reconnecting...");
        }

        // HTTPS health checks and WSS handshakes both require the small pool
        // of DMA-capable internal RAM used by the P4 AES engine.  Give a
        // pending health check exclusive use of that pool before opening WSS.
        bool health_pending = false;
        portENTER_CRITICAL(&g_mux);
        health_pending = g_health_requested || g_health_in_progress;
        portEXIT_CRITICAL(&g_mux);
        const bool websocket_cooldown_complete = !g_ws_restart_not_before_ms ||
            static_cast<int32_t>(millis() - g_ws_restart_not_before_ms) >= 0;
        if (configured_snapshot() && !g_ws_started && !health_pending && websocket_cooldown_complete) {
            start_websocket_worker();
        }
        if (g_ws_started) g_ws.loop();

        if (g_weather_forecast_started_ms &&
            millis() - g_weather_forecast_started_ms >= HA_WEATHER_FORECAST_TIMEOUT_MS) {
            g_weather_hourly_request_id = 0;
            g_weather_daily_request_id = 0;
            g_weather_forecast_started_ms = 0;
            g_weather_forecast_retry_not_before_ms = millis() + HA_WEATHER_FORECAST_RETRY_MS;
            portENTER_CRITICAL(&g_mux);
            g_weather_cache.forecasts_loading = false;
            portEXIT_CRITICAL(&g_mux);
            set_discovery_message("Weather forecast refresh timed out; current conditions remain live.");
        }

        if (g_calendar_request_started_ms &&
            millis() - g_calendar_request_started_ms >= HA_CALENDAR_TIMEOUT_MS) {
            memset(g_calendar_request_ids, 0, sizeof(g_calendar_request_ids));
            g_calendar_request_started_ms = 0;
            g_calendar_retry_not_before_ms = millis() + HA_CALENDAR_RETRY_MS;
            portENTER_CRITICAL(&g_mux);
            g_calendar_cache.loading = false;
            g_calendar_cache.available = false;
            portEXIT_CRITICAL(&g_mux);
            set_discovery_message("Calendar refresh timed out; cached events remain visible.");
        }

        if (g_action_in_flight &&
            millis() - g_action_started_ms >= HA_COMMAND_RESULT_TIMEOUT_MS) {
            record_action_result(g_action_description[0] ? g_action_description : "Home Assistant action", -110);
            g_action_in_flight = false;
            g_action_request_id = 0;
            g_action_started_ms = 0;
            g_action_description[0] = '\0';
        }

        if (g_ws_authenticated) {
            char media_entity[96] = {};
            char media_content_id[HA_MEDIA_CONTENT_ID_LEN] = {};
            char media_content_type[HA_MEDIA_CONTENT_TYPE_LEN] = {};
            if (take_media_browse_request_worker(media_entity, sizeof(media_entity),
                                                 media_content_id, sizeof(media_content_id),
                                                 media_content_type, sizeof(media_content_type))) {
                send_media_browse_worker(media_entity, media_content_id, media_content_type, 0);
            }
            if (take_flag(g_weather_forecast_requested)) send_weather_forecasts_worker();
            if (!calendar_requests_active() && take_flag(g_calendar_requested)) send_calendar_events_worker();
        }

        if (g_ws_authenticated && take_flag(g_resubscribe_requested)) {
            send_subscribe_entities_worker();
            g_resume_entities_after_reconnect = false;
        }

        // A healthy subscribe_entities stream updates immediately when HA
        // changes a configured device. Reconcile only after a long idle period
        // so a dropped hosted-radio frame cannot leave a stale button behind.
        HomeAssistantDiscoveryStatus live_status = {};
        home_assistant_get_discovery_status(live_status);
        if (g_ws_authenticated && live_status.discovery_complete &&
            live_status.last_state_ms &&
            millis() - live_status.last_state_ms >= HA_ENTITY_SUBSCRIPTION_REFRESH_MS &&
            (!g_last_entity_subscription_refresh_ms ||
             millis() - g_last_entity_subscription_refresh_ms >= HA_ENTITY_SUBSCRIPTION_REFRESH_MS)) {
            refresh_entity_subscription_worker();
        }

        if (g_ws_authenticated && take_flag(g_discovery_requested)) {
            portENTER_CRITICAL(&g_mux);
            g_entity_count = 0;
            memset(g_media_favorites, 0, sizeof(g_media_favorites));
            g_media_favorite_count = 0;
            portEXIT_CRITICAL(&g_mux);
            send_area_lookup_worker();
        }

        if (g_discovery_started_ms &&
            !g_discovery.discovery_complete &&
            millis() - g_discovery_started_ms >= HA_DISCOVERY_TIMEOUT_MS) {
            g_discovery_started_ms = 0;
            set_discovery_message("Home Assistant discovery timed out.");
        }

        const bool http_ready = !g_last_http_ms ||
                                millis() - g_last_http_ms >= HA_HTTP_INTER_REQUEST_GAP_MS;

        if (http_ready && !g_action_in_flight && g_action_queue) {
            HaAction action = {};
            if (xQueueReceive(g_action_queue, &action, 0) == pdTRUE) {
                // A disconnect can happen after the UI accepted a tap.  Do
                // not send a blind REST command without the live session that
                // supplied the entity state; the user can retry after the
                // visible reconnect completes.
                if (home_assistant_commands_ready()) process_action_worker(action);
                else record_action_result("Home Assistant reconnecting", -102);
                g_last_http_ms = millis();
                if (g_ws_started) g_ws.loop();
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }
        }

        if (http_ready) {
            char artwork_entity[96] = {};
            char artwork_url[HA_MEDIA_ARTWORK_URL_LEN] = {};
            if (take_artwork_request_worker(artwork_entity, sizeof(artwork_entity),
                                            artwork_url, sizeof(artwork_url))) {
                // ESP32-P4 TLS needs DMA-capable internal RAM. Keeping the
                // authenticated WSS connection open while starting a second
                // HTTPS client can exhaust that pool (esp-aes allocation
                // failures), even with ample PSRAM. Release the idle socket
                // before fetching artwork; the next worker pass reconnects
                // and rediscovers the small area subscription.
                char base[sizeof(g_base_url)] = {};
                char token_unused[sizeof(g_token)] = {};
                credentials_snapshot(base, sizeof(base), token_unused, sizeof(token_unused));
                const bool secure_artwork = String(artwork_url).startsWith("https://") ||
                                            (artwork_url[0] == '/' && String(base).startsWith("https://"));
                if (secure_artwork && g_ws_started) {
                    // Keep the media snapshot intact so the UI does not
                    // briefly fall back to "not playing" while TLS memory is
                    // released for the artwork connection.
                    g_resume_entities_after_reconnect = g_entity_count > 0;
                    g_ws.disconnect();
                    g_ws_started = false;
                    g_ws_authenticated = false;
                    portENTER_CRITICAL(&g_mux);
                    g_discovery.websocket_connected = false;
                    g_discovery.websocket_authenticated = false;
                    copy_text(g_discovery.message, sizeof(g_discovery.message),
                              "Refreshing media artwork; reconnecting after download...");
                    portEXIT_CRITICAL(&g_mux);
                    // NetworkClientSecure and the AES DMA driver release
                    // allocations asynchronously after stop(). Starting the
                    // artwork TLS client immediately caused the -5 / DMA
                    // allocation failures seen on the serial console.
                    vTaskDelay(pdMS_TO_TICKS(HA_TLS_RELEASE_SETTLE_MS));
                }
                const int artwork_result = download_artwork_worker(artwork_entity, artwork_url);
                portENTER_CRITICAL(&g_mux);
                if (artwork_result >= 200 && artwork_result < 300) {
                    g_artwork_last_failed_url[0] = '\0';
                    g_artwork_retry_not_before_ms = 0;
                } else {
                    copy_text(g_artwork_last_failed_url, sizeof(g_artwork_last_failed_url), artwork_url);
                    g_artwork_retry_not_before_ms = millis() + HA_MEDIA_ARTWORK_FAILURE_RETRY_MS;
                }
                // A failed or successful HTTPS transfer both leave the hosted
                // radio with outstanding RX/TLS cleanup.  Avoid immediately
                // starting a WSS handshake, which was the path to the SDIO
                // receive-buffer assertion in the captured crash.
                g_ws_restart_not_before_ms = millis() + HA_TLS_POST_ARTWORK_COOLDOWN_MS;
                portEXIT_CRITICAL(&g_mux);
                g_last_http_ms = millis();
                if (g_ws_started) g_ws.loop();
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }
        }

        bool do_health = false;
        portENTER_CRITICAL(&g_mux);
        if (g_health_requested && !g_health_in_progress) {
            do_health = true;
            if (http_ready) g_health_requested = false;
        }
        portEXIT_CRITICAL(&g_mux);

        // Never begin a second TLS connection alongside an active WSS client.
        // A pending health check is served once the socket is intentionally
        // released (or before the initial WSS connection starts).
        if (do_health && http_ready && !g_ws_started && websocket_cooldown_complete) {
            run_health_check_worker();
            g_last_http_ms = millis();
            if (g_ws_started) g_ws.loop();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void snapshot_entity(const HaEntityModel &source, HomeAssistantEntitySnapshot &out) {
    memset(&out, 0, sizeof(out));
    copy_text(out.entity_id, sizeof(out.entity_id), source.entity_id);
    copy_text(out.name, sizeof(out.name), source.name);
    copy_text(out.domain, sizeof(out.domain), source.domain);
    copy_text(out.state, sizeof(out.state), source.state);
    copy_text(out.unit_of_measurement, sizeof(out.unit_of_measurement), source.unit_of_measurement);
    out.brightness_pct = source.brightness_pct;
    out.position_pct = source.position_pct;
    out.available = source.available;
    out.supports_brightness = source.supports_brightness;
    out.supports_position = source.supports_position;
    out.supports_fan_speed = source.supports_fan_speed;
    out.fan_speed_pct = source.fan_speed_pct;
    if (strcmp(source.domain, "timer") == 0) {
        out.timer_has_remaining = source.timer_has_remaining;
        out.timer_remaining_seconds = source.timer_remaining_seconds;
        if (strcmp(source.state, "idle") == 0) {
            out.timer_has_remaining = true;
            out.timer_remaining_seconds = 0;
        } else if (strcmp(source.state, "active") == 0) {
            const time_t now = time(nullptr);
            if (source.timer_finishes_at_epoch > 0 && now > 1600000000) {
                const int64_t remaining = source.timer_finishes_at_epoch - static_cast<int64_t>(now);
                out.timer_has_remaining = true;
                out.timer_remaining_seconds = remaining > 0 ? static_cast<uint32_t>(remaining) : 0;
            } else if (out.timer_has_remaining) {
                const uint32_t elapsed = (millis() - source.timer_sample_ms) / 1000U;
                out.timer_remaining_seconds = elapsed < out.timer_remaining_seconds
                                                  ? out.timer_remaining_seconds - elapsed : 0;
            }
        }
    }
}


void snapshot_media(const HaEntityModel &source, HomeAssistantMediaSnapshot &out) {
    memset(&out, 0, sizeof(out));
    copy_text(out.entity_id, sizeof(out.entity_id), source.entity_id);
    copy_text(out.name, sizeof(out.name), source.name);
    copy_text(out.state, sizeof(out.state), source.state);
    copy_text(out.title, sizeof(out.title), source.media_title);
    copy_text(out.artist, sizeof(out.artist), source.media_artist);
    copy_text(out.album, sizeof(out.album), source.media_album);
    copy_text(out.playlist, sizeof(out.playlist), source.media_playlist);
    copy_text(out.source, sizeof(out.source), source.media_source);
    copy_text(out.entity_picture, sizeof(out.entity_picture), source.entity_picture);
    out.volume_pct = source.volume_pct;
    out.volume_muted = source.volume_muted;
    out.available = source.available;
    out.supports_volume = source.supports_volume;
    out.supports_mute = source.supports_mute;
    out.source_count = source.media_source_count;
    for (uint8_t i = 0; i < source.media_source_count && i < HA_MAX_MEDIA_SOURCES; ++i) {
        copy_text(out.sources[i], HA_MEDIA_SOURCE_NAME_LEN, source.media_sources[i]);
    }
}

void snapshot_weather_current(const HaEntityModel &source, HomeAssistantWeatherSnapshot &out) {
    copy_text(out.entity_id, sizeof(out.entity_id), source.entity_id);
    copy_text(out.name, sizeof(out.name), source.name);
    copy_text(out.condition, sizeof(out.condition), source.state);
    copy_text(out.temperature_unit, sizeof(out.temperature_unit),
              source.weather_temperature_unit[0] ? source.weather_temperature_unit : source.unit_of_measurement);
    copy_text(out.wind_speed_unit, sizeof(out.wind_speed_unit), source.weather_wind_speed_unit);
    out.temperature = source.weather_temperature;
    out.apparent_temperature = source.weather_apparent_temperature;
    out.humidity = source.weather_humidity;
    out.wind_speed = source.weather_wind_speed;
    out.pressure = source.weather_has_pressure ? static_cast<uint16_t>(source.weather_pressure + 0.5f) : 0;
    out.available = source.available;
    out.has_temperature = source.weather_has_temperature;
    out.has_apparent_temperature = source.weather_has_apparent_temperature;
    out.has_humidity = source.weather_has_humidity;
    out.has_wind_speed = source.weather_has_wind_speed;
    out.has_pressure = source.weather_has_pressure;
}

bool queue_action(const HaAction &action) {
    const bool ready = home_assistant_commands_ready();
    if (!g_action_queue || !ready) {
        if (!ready) record_action_result("Home Assistant reconnecting", -102);
        return false;
    }
    return xQueueSend(g_action_queue, &action, 0) == pdTRUE;
}

}  // namespace

void home_assistant_begin() {
    load_preferences();

    g_entities = static_cast<HaEntityModel *>(
        heap_caps_calloc(HA_MAX_AREA_ENTITIES, sizeof(HaEntityModel),
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!g_entities) {
        g_entities = static_cast<HaEntityModel *>(calloc(HA_MAX_AREA_ENTITIES, sizeof(HaEntityModel)));
        Serial0.println("[HA] WARNING: state cache fell back to internal heap");
    } else {
        Serial0.printf("[HA] PSRAM state cache: %u entities\n",
                       static_cast<unsigned>(HA_MAX_AREA_ENTITIES));
    }

    if (!g_entities) {
        Serial0.println("[HA] ERROR: could not allocate entity state cache");
        set_discovery_message("Could not allocate Home Assistant state cache.");
        return;
    }

    g_calendar_events = static_cast<HomeAssistantCalendarEvent *>(
        heap_caps_calloc(HA_MAX_CALENDAR_EVENTS, sizeof(HomeAssistantCalendarEvent),
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!g_calendar_events)
        g_calendar_events = static_cast<HomeAssistantCalendarEvent *>(
            calloc(HA_MAX_CALENDAR_EVENTS, sizeof(HomeAssistantCalendarEvent)));
    if (!g_calendar_events)
        Serial0.println("[HA] WARNING: calendar event cache unavailable");

    g_action_queue = xQueueCreate(HA_ACTION_QUEUE_DEPTH, sizeof(HaAction));
    if (!g_action_queue) {
        Serial0.println("[HA] ERROR: could not create action queue");
        set_discovery_message("Could not create Home Assistant action queue.");
        return;
    }

    g_artwork_mutex = xSemaphoreCreateMutex();
    if (!g_artwork_mutex) {
        Serial0.println("[HA] WARNING: media artwork cache disabled (mutex allocation failed)");
    }

    memset(&g_status, 0, sizeof(g_status));
    memset(&g_discovery, 0, sizeof(g_discovery));

    portENTER_CRITICAL(&g_mux);
    g_status.configured = g_base_url[0] && g_token[0];
    copy_text(g_status.message, sizeof(g_status.message),
              g_status.configured ? "configured; waiting for network" : "not configured");
    copy_text(g_discovery.message, sizeof(g_discovery.message),
              g_status.configured ? "waiting for network" : "Home Assistant not configured");
    portEXIT_CRITICAL(&g_mux);

    xTaskCreate(worker_task, "ha_worker", HA_WORKER_STACK_BYTES, nullptr,
                HA_WORKER_PRIORITY, &g_worker);
}

void home_assistant_loop() {
    const uint32_t now = millis();

    if (!network_service_connected()) {
        g_connected_since_ms = 0;
        portENTER_CRITICAL(&g_mux);
        g_network_ready = false;
        portEXIT_CRITICAL(&g_mux);
        return;
    }

    if (!g_connected_since_ms) g_connected_since_ms = now;
    if (now - g_connected_since_ms < HA_BOOT_NETWORK_STABLE_MS) return;

    portENTER_CRITICAL(&g_mux);
    g_network_ready = true;
    portEXIT_CRITICAL(&g_mux);

    if (configured_snapshot() &&
        (!g_last_health_request_ms ||
         now - g_last_health_request_ms >= HA_HEALTH_INTERVAL_MS)) {
        if (home_assistant_request_health_check()) g_last_health_request_ms = now;
    }
}

bool home_assistant_request_health_check() {
    if (!g_worker || !configured_snapshot()) return false;

    bool queued = false;
    portENTER_CRITICAL(&g_mux);
    if (!g_health_requested && !g_health_in_progress) {
        g_health_requested = true;
        g_status.request_in_progress = true;
        queued = true;
    }
    portEXIT_CRITICAL(&g_mux);
    return queued;
}

bool home_assistant_commands_ready() {
    if (!network_service_connected() || !network_ready_snapshot() || !configured_snapshot()) {
        return false;
    }
    portENTER_CRITICAL(&g_mux);
    const bool ready = g_ws_started && g_ws_authenticated &&
                       g_discovery.websocket_authenticated &&
                       g_discovery.discovery_complete;
    portEXIT_CRITICAL(&g_mux);
    return ready;
}

void home_assistant_get_status(HomeAssistantStatus &out) {
    const bool connected = network_service_connected();
    portENTER_CRITICAL(&g_mux);
    out = g_status;
    out.connected = connected;
    out.request_in_progress = g_health_in_progress || g_health_requested;
    portEXIT_CRITICAL(&g_mux);
}

bool home_assistant_request_discovery() {
    if (!g_worker || !configured_snapshot()) return false;
    portENTER_CRITICAL(&g_mux);
    g_full_discovery_requested = false;
    g_discovery_requested = true;
    g_discovery.discovery_complete = false;
    copy_text(g_discovery.message, sizeof(g_discovery.message), "Discovery requested...");
    portEXIT_CRITICAL(&g_mux);
    return true;
}

bool home_assistant_request_full_discovery() {
    if (!g_worker || !configured_snapshot()) return false;
    portENTER_CRITICAL(&g_mux);
    g_full_discovery_requested = false;
    g_rest_discovery_requested = true;
    g_discovery.discovery_complete = false;
    copy_text(g_discovery.message, sizeof(g_discovery.message),
              "Direct Home Assistant search requested for the web layout editor...");
    portEXIT_CRITICAL(&g_mux);
    return true;
}

void home_assistant_get_discovery_status(HomeAssistantDiscoveryStatus &out) {
    portENTER_CRITICAL(&g_mux);
    out = g_discovery;
    portEXIT_CRITICAL(&g_mux);
}

size_t home_assistant_get_room_controls(HomeAssistantEntitySnapshot *out, size_t max_count) {
    if (!out || max_count == 0 || !g_entities) return 0;
    if (max_count > HA_MAX_ROOM_CONTROLS) max_count = HA_MAX_ROOM_CONTROLS;

    size_t count = 0;
    int selected[HA_MAX_ROOM_CONTROLS];
    for (size_t i = 0; i < HA_MAX_ROOM_CONTROLS; ++i) selected[i] = -1;

    const char *preferred[] = {"light", "switch", "fan", "cover"};

    portENTER_CRITICAL(&g_mux);
    for (const char *domain : preferred) {
        if (count >= max_count) break;
        for (size_t i = 0; i < g_entity_count; ++i) {
            if (strcmp(g_entities[i].domain, domain) == 0) {
                selected[count++] = static_cast<int>(i);
                break;
            }
        }
    }

    for (size_t i = 0; i < g_entity_count && count < max_count; ++i) {
        if (!is_control_domain(g_entities[i].domain)) continue;
        bool already = false;
        for (size_t j = 0; j < count; ++j) {
            if (selected[j] == static_cast<int>(i)) {
                already = true;
                break;
            }
        }
        if (!already) selected[count++] = static_cast<int>(i);
    }

    for (size_t i = 0; i < count; ++i) snapshot_entity(g_entities[selected[i]], out[i]);
    portEXIT_CRITICAL(&g_mux);
    return count;
}

size_t home_assistant_get_area_scenes(HomeAssistantEntitySnapshot *out, size_t max_count) {
    if (!out || max_count == 0 || !g_entities) return 0;
    if (max_count > HA_MAX_AREA_SCENES) max_count = HA_MAX_AREA_SCENES;

    size_t count = 0;
    portENTER_CRITICAL(&g_mux);
    for (size_t i = 0; i < g_entity_count && count < max_count; ++i) {
        if (strcmp(g_entities[i].domain, "scene") == 0) snapshot_entity(g_entities[i], out[count++]);
    }
    portEXIT_CRITICAL(&g_mux);
    return count;
}

void home_assistant_get_light_stats(HomeAssistantLightStats &out) {
    memset(&out, 0, sizeof(out));
    if (!g_entities) return;

    uint32_t brightness_sum = 0;
    uint16_t brightness_count = 0;

    portENTER_CRITICAL(&g_mux);
    for (size_t i = 0; i < g_entity_count; ++i) {
        const HaEntityModel &model = g_entities[i];
        if (strcmp(model.domain, "light") != 0 || !model.available) continue;
        ++out.total;
        if (strcmp(model.state, "on") == 0) {
            ++out.on;
            if (model.supports_brightness) {
                brightness_sum += model.brightness_pct;
                ++brightness_count;
            }
        }
    }
    portEXIT_CRITICAL(&g_mux);

    if (brightness_count) {
        out.average_brightness_pct =
            static_cast<uint8_t>(brightness_sum / brightness_count);
    } else if (out.on) {
        out.average_brightness_pct = 100;
    }
}

bool home_assistant_queue_toggle(const char *entity_id) {
    if (!entity_id || !entity_id[0]) return false;
    HaAction action = {};
    action.type = HaActionType::Toggle;
    copy_text(action.entity_id, sizeof(action.entity_id), entity_id);
    return queue_action(action);
}

bool home_assistant_queue_area_brightness(uint8_t brightness_pct) {
    HaAction action = {};
    action.type = HaActionType::AreaBrightness;
    action.value = constrain(static_cast<int>(brightness_pct), 0, 100);
    return queue_action(action);
}

bool home_assistant_queue_all_lights(bool turn_on) {
    HaAction action = {};
    action.type = HaActionType::AllLights;
    action.flag = turn_on;
    return queue_action(action);
}

bool home_assistant_queue_scene(const char *entity_id) {
    if (!entity_id || !entity_id[0]) return false;
    HaAction action = {};
    action.type = HaActionType::Scene;
    copy_text(action.entity_id, sizeof(action.entity_id), entity_id);
    return queue_action(action);
}


size_t home_assistant_get_media_players(HomeAssistantMediaSnapshot *out, size_t max_count) {
    if (!out || max_count == 0 || !g_entities) return 0;
    if (max_count > HA_MAX_MEDIA_PLAYERS) max_count = HA_MAX_MEDIA_PLAYERS;

    size_t count = 0;
    portENTER_CRITICAL(&g_mux);
    for (size_t i = 0; i < g_entity_count && count < max_count; ++i) {
        if (strcmp(g_entities[i].domain, "media_player") != 0) continue;
        snapshot_media(g_entities[i], out[count++]);
    }
    portEXIT_CRITICAL(&g_mux);
    return count;
}

size_t home_assistant_get_media_favorites(HomeAssistantMediaFavorite *out, size_t max_count) {
    if (!out || max_count == 0) return 0;
    if (max_count > HA_MAX_MEDIA_FAVORITES) max_count = HA_MAX_MEDIA_FAVORITES;

    portENTER_CRITICAL(&g_mux);
    const size_t count = g_media_favorite_count < max_count ? g_media_favorite_count : max_count;
    for (size_t i = 0; i < count; ++i) out[i] = g_media_favorites[i];
    portEXIT_CRITICAL(&g_mux);
    return count;
}

bool home_assistant_request_media_browse(const char *entity_id) {
    if (!entity_id || !entity_id[0] || !g_worker || !configured_snapshot()) return false;

    bool found = false;
    portENTER_CRITICAL(&g_mux);
    for (size_t i = 0; i < g_entity_count; ++i) {
        if (strcmp(g_entities[i].entity_id, entity_id) == 0 &&
            strcmp(g_entities[i].domain, "media_player") == 0) {
            found = true;
            break;
        }
    }
    if (found) {
        copy_text(g_media_browse_entity_id, sizeof(g_media_browse_entity_id), entity_id);
        g_media_browse_content_id[0] = '\0';
        g_media_browse_content_type[0] = '\0';
        g_media_browse_depth = 0;
        g_media_browse_requested = true;
        memset(g_media_favorites, 0, sizeof(g_media_favorites));
        g_media_favorite_count = 0;
    }
    portEXIT_CRITICAL(&g_mux);
    return found;
}

bool home_assistant_request_media_artwork(const char *entity_id) {
    if (!entity_id || !entity_id[0] || !g_worker || !configured_snapshot()) return false;

    bool found = false;
    bool queued = false;
    char picture[HA_MEDIA_ARTWORK_URL_LEN] = {};
    portENTER_CRITICAL(&g_mux);
    for (size_t i = 0; i < g_entity_count; ++i) {
        if (strcmp(g_entities[i].entity_id, entity_id) == 0 &&
            strcmp(g_entities[i].domain, "media_player") == 0 &&
            g_entities[i].entity_picture[0]) {
            copy_text(picture, sizeof(picture), g_entities[i].entity_picture);
            found = true;
            break;
        }
    }
    if (found) {
        const uint32_t now = millis();
        const bool in_failure_backoff =
            strcmp(g_artwork_last_failed_url, picture) == 0 &&
            g_artwork_retry_not_before_ms &&
            static_cast<int32_t>(now - g_artwork_retry_not_before_ms) < 0;
        const bool already_queued = g_artwork_requested &&
            strcmp(g_artwork_request_entity_id, entity_id) == 0 &&
            strcmp(g_artwork_request_url, picture) == 0;
        if (!in_failure_backoff && !already_queued) {
            copy_text(g_artwork_request_entity_id, sizeof(g_artwork_request_entity_id), entity_id);
            copy_text(g_artwork_request_url, sizeof(g_artwork_request_url), picture);
            g_artwork_requested = true;
            queued = true;
        }
    }
    portEXIT_CRITICAL(&g_mux);
    if (queued) {
        Serial0.printf("[HA] Media artwork queued: entity=%s picture_chars=%u\n",
                       entity_id, static_cast<unsigned>(strlen(picture)));
    }
    return queued;
}

void home_assistant_get_media_artwork_info(HomeAssistantMediaArtworkInfo &out) {
    memset(&out, 0, sizeof(out));
    if (!g_artwork_mutex) return;
    if (xSemaphoreTake(g_artwork_mutex, 0) != pdTRUE) return;
    out = g_artwork_info;
    xSemaphoreGive(g_artwork_mutex);
}

bool home_assistant_copy_media_artwork(uint8_t *dest, size_t capacity,
                                       HomeAssistantMediaArtworkInfo &out) {
    memset(&out, 0, sizeof(out));
    if (!g_artwork_mutex) return false;
    if (xSemaphoreTake(g_artwork_mutex, pdMS_TO_TICKS(20)) != pdTRUE) return false;

    out = g_artwork_info;
    const bool ok = g_artwork_data && out.data_size > 0 && dest && capacity >= out.data_size;
    if (ok) memcpy(dest, g_artwork_data, out.data_size);
    xSemaphoreGive(g_artwork_mutex);
    return ok;
}

bool home_assistant_queue_media_play_pause(const char *entity_id) {
    if (!entity_id || !entity_id[0]) return false;
    HaAction action = {};
    action.type = HaActionType::MediaPlayPause;
    copy_text(action.entity_id, sizeof(action.entity_id), entity_id);
    return queue_action(action);
}

bool home_assistant_queue_media_previous(const char *entity_id) {
    if (!entity_id || !entity_id[0]) return false;
    HaAction action = {};
    action.type = HaActionType::MediaPrevious;
    copy_text(action.entity_id, sizeof(action.entity_id), entity_id);
    return queue_action(action);
}

bool home_assistant_queue_media_next(const char *entity_id) {
    if (!entity_id || !entity_id[0]) return false;
    HaAction action = {};
    action.type = HaActionType::MediaNext;
    copy_text(action.entity_id, sizeof(action.entity_id), entity_id);
    return queue_action(action);
}

bool home_assistant_queue_media_volume(const char *entity_id, uint8_t volume_pct) {
    if (!entity_id || !entity_id[0]) return false;
    HaAction action = {};
    action.type = HaActionType::MediaVolume;
    action.value = static_cast<uint8_t>(constrain(static_cast<int>(volume_pct), 0, 100));
    copy_text(action.entity_id, sizeof(action.entity_id), entity_id);
    return queue_action(action);
}

bool home_assistant_queue_media_volume_step(const char *entity_id, bool increase) {
    if (!entity_id || !entity_id[0]) return false;
    HaAction action = {};
    action.type = increase ? HaActionType::MediaVolumeUp : HaActionType::MediaVolumeDown;
    copy_text(action.entity_id, sizeof(action.entity_id), entity_id);
    return queue_action(action);
}

bool home_assistant_queue_media_mute(const char *entity_id, bool muted) {
    if (!entity_id || !entity_id[0]) return false;
    HaAction action = {};
    action.type = HaActionType::MediaMute;
    action.flag = muted;
    copy_text(action.entity_id, sizeof(action.entity_id), entity_id);
    return queue_action(action);
}

bool home_assistant_queue_media_source(const char *entity_id, const char *source) {
    if (!entity_id || !entity_id[0] || !source || !source[0]) return false;
    HaAction action = {};
    action.type = HaActionType::MediaSource;
    copy_text(action.entity_id, sizeof(action.entity_id), entity_id);
    copy_text(action.text, sizeof(action.text), source);
    return queue_action(action);
}

bool home_assistant_queue_media_favorite(const HomeAssistantMediaFavorite &favorite) {
    if (!favorite.entity_id[0] || !favorite.media_content_id[0] ||
        !favorite.media_content_type[0]) return false;
    HaAction action = {};
    action.type = HaActionType::MediaFavorite;
    copy_text(action.entity_id, sizeof(action.entity_id), favorite.entity_id);
    copy_text(action.text, sizeof(action.text), favorite.media_content_id);
    copy_text(action.aux, sizeof(action.aux), favorite.media_content_type);
    return queue_action(action);
}

bool home_assistant_set_credentials(const char *base_url, const char *token) {
    String base = base_url ? base_url : "";
    String new_token = token ? token : "";
    base.trim();
    new_token.trim();

    if (!base.isEmpty() && !base.startsWith("http://") && !base.startsWith("https://")) {
        return false;
    }

    Preferences prefs;
    if (!prefs.begin("panel_ha", false)) return false;
    prefs.putString("url", base);
    if (!new_token.isEmpty()) prefs.putString("token", new_token);
    prefs.end();

    portENTER_CRITICAL(&g_mux);
    copy_text(g_base_url, sizeof(g_base_url), base.c_str());
    if (!new_token.isEmpty()) copy_text(g_token, sizeof(g_token), new_token.c_str());
    g_status.configured = g_base_url[0] && g_token[0];
    g_reconnect_requested = true;
    portEXIT_CRITICAL(&g_mux);
    return true;
}

String home_assistant_base_url() {
    return normalized_base();
}

bool home_assistant_token_configured() {
    portENTER_CRITICAL(&g_mux);
    const bool configured = g_token[0] != '\0';
    portEXIT_CRITICAL(&g_mux);
    return configured;
}

size_t home_assistant_get_room_entities(HomeAssistantEntitySnapshot *out, size_t max_count) {
    if (!out || !max_count || !g_entities) return 0;
    size_t count = 0;
    portENTER_CRITICAL(&g_mux);
    for (size_t i = 0; i < g_entity_count && count < max_count; ++i) {
        if (is_control_domain(g_entities[i].domain) || strcmp(g_entities[i].domain, "scene") == 0)
            snapshot_entity(g_entities[i], out[count++]);
    }
    portEXIT_CRITICAL(&g_mux);
    return count;
}

size_t home_assistant_get_layout_entities(HomeAssistantEntitySnapshot *out, size_t max_count) {
    if (!out || !max_count || !g_entities) return 0;
    size_t count = 0;
    portENTER_CRITICAL(&g_mux);
    for (size_t i = 0; i < g_entity_count && count < max_count; ++i)
        snapshot_entity(g_entities[i], out[count++]);
    portEXIT_CRITICAL(&g_mux);
    return count;
}

bool home_assistant_queue_light_brightness(const char *entity_id, uint8_t brightness_pct) {
    if (!entity_id || strncmp(entity_id, "light.", 6) != 0) return false;
    HaAction action = {};
    action.type = HaActionType::LightBrightness;
    copy_text(action.entity_id, sizeof(action.entity_id), entity_id);
    action.value = constrain(static_cast<int>(brightness_pct), 0, 100);
    return queue_action(action);
}

bool home_assistant_queue_fan_speed(const char *entity_id, uint8_t percentage) {
    if (!entity_id || strncmp(entity_id, "fan.", 4) != 0) return false;
    HaAction action = {};
    action.type = HaActionType::FanSpeed;
    copy_text(action.entity_id, sizeof(action.entity_id), entity_id);
    action.value = constrain(static_cast<int>(percentage), 0, 100);
    return queue_action(action);
}

bool home_assistant_get_room_entity(const char *entity_id, HomeAssistantEntitySnapshot &out) {
    if (!entity_id || !g_entities) return false;
    bool found = false;
    portENTER_CRITICAL(&g_mux);
    for (size_t i = 0; i < g_entity_count; ++i) {
        if (strcmp(entity_id, g_entities[i].entity_id) == 0 &&
            (is_control_domain(g_entities[i].domain) || strcmp(g_entities[i].domain, "scene") == 0)) {
            snapshot_entity(g_entities[i], out); found = true; break;
        }
    }
    portEXIT_CRITICAL(&g_mux);
    return found;
}

bool home_assistant_get_entity(const char *entity_id, HomeAssistantEntitySnapshot &out) {
    if (!entity_id || !g_entities) return false;
    bool found = false;
    portENTER_CRITICAL(&g_mux);
    for (size_t i = 0; i < g_entity_count; ++i) {
        if (strcmp(entity_id, g_entities[i].entity_id) == 0) {
            snapshot_entity(g_entities[i], out); found = true; break;
        }
    }
    portEXIT_CRITICAL(&g_mux);
    return found;
}

bool home_assistant_request_weather_forecasts(const char *entity_id) {
    if (!entity_id || strncmp(entity_id, "weather.", 8) != 0 || !g_worker) return false;
    bool queued = false;
    portENTER_CRITICAL(&g_mux);
    const bool entity_changed = strcmp(g_weather_forecast_entity_id, entity_id) != 0;
    const bool stale = !g_weather_cache.last_forecast_ms ||
        millis() - g_weather_cache.last_forecast_ms >= HA_WEATHER_FORECAST_REFRESH_MS;
    if (entity_changed) {
        memset(&g_weather_cache, 0, sizeof(g_weather_cache));
        copy_text(g_weather_forecast_entity_id, sizeof(g_weather_forecast_entity_id), entity_id);
    }
    const bool retry_ready = !g_weather_forecast_retry_not_before_ms ||
        static_cast<int32_t>(millis() - g_weather_forecast_retry_not_before_ms) >= 0;
    if ((entity_changed || stale) && retry_ready && !g_weather_forecast_requested &&
        !g_weather_hourly_request_id && !g_weather_daily_request_id) {
        g_weather_forecast_requested = true;
        g_weather_cache.forecasts_loading = true;
        queued = true;
    }
    portEXIT_CRITICAL(&g_mux);
    return queued;
}

bool home_assistant_get_weather(const char *entity_id, HomeAssistantWeatherSnapshot &out) {
    memset(&out, 0, sizeof(out));
    if (!entity_id || !entity_id[0] || !g_entities) return false;
    bool found = false;
    portENTER_CRITICAL(&g_mux);
    for (size_t i = 0; i < g_entity_count; ++i) {
        if (strcmp(entity_id, g_entities[i].entity_id) == 0 &&
            strcmp(g_entities[i].domain, "weather") == 0) {
            if (strcmp(g_weather_forecast_entity_id, entity_id) == 0) out = g_weather_cache;
            snapshot_weather_current(g_entities[i], out);
            found = true;
            break;
        }
    }
    portEXIT_CRITICAL(&g_mux);
    return found;
}

bool home_assistant_request_calendar_events(const char *range_start, const char *range_end) {
    if (!range_start || !range_end || !range_start[0] || !range_end[0] ||
        !g_worker || !g_calendar_events || config_service_get().calendar_count == 0) return false;
    bool queued = false;
    portENTER_CRITICAL(&g_mux);
    const bool range_changed = strcmp(g_calendar_next_start, range_start) != 0 ||
                               strcmp(g_calendar_next_end, range_end) != 0;
    const bool stale = !g_calendar_cache.last_update_ms ||
        millis() - g_calendar_cache.last_update_ms >= HA_CALENDAR_REFRESH_MS;
    const bool retry_ready = !g_calendar_retry_not_before_ms ||
        static_cast<int32_t>(millis() - g_calendar_retry_not_before_ms) >= 0;
    const bool in_flight_same = calendar_requests_active() &&
        strcmp(g_calendar_requested_start, range_start) == 0 &&
        strcmp(g_calendar_requested_end, range_end) == 0;
    if ((range_changed || stale) && retry_ready && !in_flight_same) {
        copy_text(g_calendar_next_start, sizeof(g_calendar_next_start), range_start);
        copy_text(g_calendar_next_end, sizeof(g_calendar_next_end), range_end);
        g_calendar_requested = true;
        g_calendar_cache.loading = true;
        if (range_changed && strcmp(g_calendar_cache.range_start, range_start) != 0) {
            g_calendar_cache.event_count = 0;
            g_calendar_cache.available = false;
        }
        queued = true;
    }
    portEXIT_CRITICAL(&g_mux);
    return queued;
}

void home_assistant_get_calendar(HomeAssistantCalendarSnapshot &out) {
    portENTER_CRITICAL(&g_mux);
    out = g_calendar_cache;
    portEXIT_CRITICAL(&g_mux);
}

bool home_assistant_get_calendar_event(size_t index, HomeAssistantCalendarEvent &out) {
    bool found = false;
    portENTER_CRITICAL(&g_mux);
    if (g_calendar_events && index < g_calendar_cache.event_count) {
        out = g_calendar_events[index];
        found = true;
    }
    portEXIT_CRITICAL(&g_mux);
    return found;
}
