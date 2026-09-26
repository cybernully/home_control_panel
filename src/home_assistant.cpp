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
#include <stdlib.h>
#include <string.h>

namespace {

struct HaEntityModel {
    char entity_id[96];
    char name[64];
    char domain[16];
    char state[32];
    uint8_t brightness_pct;
    int16_t position_pct;
    bool available;
    bool supports_brightness;
    bool supports_position;

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
bool g_reconnect_requested = false;
bool g_network_ready = false;
bool g_action_in_flight = false;

uint32_t g_connected_since_ms = 0;
uint32_t g_last_health_request_ms = 0;
uint32_t g_last_http_ms = 0;
uint32_t g_discovery_started_ms = 0;
uint32_t g_next_ws_id = 1;
uint32_t g_area_request_id = 0;
uint32_t g_extract_request_id = 0;
uint32_t g_subscribe_request_id = 0;
uint32_t g_action_request_id = 0;
uint32_t g_action_started_ms = 0;
char g_action_description[96] = {};

char g_resolved_area_id[64] = {};
char g_resolved_area_name[64] = {};

void record_action_result(const char *description, int code);

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
           strcmp(domain, "media_player") == 0;
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

void apply_attributes_locked(HaEntityModel &model, JsonObjectConst attrs) {
    if (attrs.isNull()) return;

    const char *friendly = attrs["friendly_name"].as<const char *>();
    if (friendly && friendly[0]) copy_text(model.name, sizeof(model.name), friendly);

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
                } else if (strcmp(key, "friendly_name") == 0) {
                    fallback_name_from_id(model->entity_id, model->name, sizeof(model->name));
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
        set_discovery_message("Subscribing to live Home Assistant state...");
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
        g_discovery_requested = true;
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
        } else if (id == g_subscribe_request_id) {
            const bool success = doc["success"] | false;
            if (!success) set_discovery_message("Home Assistant rejected live entity subscription.");
        } else if (id == g_media_browse_request_id) {
            handle_media_browse_result_worker(doc);
        } else if (id == g_action_request_id && g_action_in_flight) {
            handle_action_result_worker(doc);
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
            // Area-filtered subscriptions are intentionally kept small enough
            // for the library's normal ESP32 frame buffer. If HA returns a frame
            // above that limit, surface it instead of silently using partial JSON.
            set_discovery_message("WebSocket message exceeded frame buffer; reduce entities in the configured area.");
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
    reset_discovery_state("Connecting to Home Assistant WebSocket...");

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

    while (http.connected() && size < capacity && (remaining > 0 || declared < 0)) {
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
        if (!model || !is_control_domain(model->domain)) {
            record_action_result("Entity no longer available", -103);
            return;
        }

        const char *service = nullptr;
        if (strcmp(model->domain, "cover") == 0) {
            const bool open = strcmp(model->state, "open") == 0 || strcmp(model->state, "opening") == 0;
            service = open ? "close_cover" : "open_cover";
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

        if (take_flag(g_reconnect_requested)) {
            stop_websocket_worker("Home Assistant credentials changed; reconnecting...");
        }

        if (configured_snapshot() && !g_ws_started) start_websocket_worker();
        if (g_ws_started) g_ws.loop();

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
                download_artwork_worker(artwork_entity, artwork_url);
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

        if (do_health && http_ready) {
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
    out.brightness_pct = source.brightness_pct;
    out.position_pct = source.position_pct;
    out.available = source.available;
    out.supports_brightness = source.supports_brightness;
    out.supports_position = source.supports_position;
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
    g_discovery_requested = true;
    g_discovery.discovery_complete = false;
    copy_text(g_discovery.message, sizeof(g_discovery.message), "Discovery requested...");
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
        copy_text(g_artwork_request_entity_id, sizeof(g_artwork_request_entity_id), entity_id);
        copy_text(g_artwork_request_url, sizeof(g_artwork_request_url), picture);
        g_artwork_requested = true;
    }
    portEXIT_CRITICAL(&g_mux);
    if (found) {
        Serial0.printf("[HA] Media artwork queued: entity=%s picture_chars=%u\n",
                       entity_id, static_cast<unsigned>(strlen(picture)));
    }
    return found;
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

bool home_assistant_queue_light_brightness(const char *entity_id, uint8_t brightness_pct) {
    if (!entity_id || strncmp(entity_id, "light.", 6) != 0) return false;
    HaAction action = {};
    action.type = HaActionType::LightBrightness;
    copy_text(action.entity_id, sizeof(action.entity_id), entity_id);
    action.value = constrain(static_cast<int>(brightness_pct), 0, 100);
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
