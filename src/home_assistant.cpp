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
};

enum class HaActionType : uint8_t {
    Toggle,
    AreaBrightness,
    AllLights,
    Scene,
};

struct HaAction {
    HaActionType type;
    char entity_id[96];
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

uint32_t g_connected_since_ms = 0;
uint32_t g_last_health_request_ms = 0;
uint32_t g_last_http_ms = 0;
uint32_t g_discovery_started_ms = 0;
uint32_t g_next_ws_id = 1;
uint32_t g_area_request_id = 0;
uint32_t g_extract_request_id = 0;
uint32_t g_subscribe_request_id = 0;

char g_resolved_area_id[64] = {};
char g_resolved_area_name[64] = {};

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
    return is_control_domain(domain) || strcmp(domain, "scene") == 0;
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
        copy_text(g_discovery.message, sizeof(g_discovery.message), "Area found; no supported controls were discovered.");
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

void process_action_worker(const HaAction &action) {
    int code = -100;
    char description[80] = {};

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

        code = http_post_service(model->domain, service, entity_target_body(model->entity_id));
        snprintf(description, sizeof(description), "%s %s", model->name, service);
    } else if (action.type == HaActionType::AreaBrightness) {
        const bool turn_off = action.value == 0;
        code = http_post_service("light", turn_off ? "turn_off" : "turn_on",
                                 area_light_body(!turn_off, action.value));
        snprintf(description, sizeof(description), "Area lights %s",
                 turn_off ? "off" : "brightness");
    } else if (action.type == HaActionType::AllLights) {
        code = http_post_service("light", action.flag ? "turn_on" : "turn_off",
                                 area_light_body(false, 0));
        snprintf(description, sizeof(description), "All lights %s", action.flag ? "on" : "off");
    } else if (action.type == HaActionType::Scene) {
        HaEntityModel *model = find_entity_worker(action.entity_id);
        if (!model || strcmp(model->domain, "scene") != 0) {
            record_action_result("Scene no longer available", -103);
            return;
        }
        code = http_post_service("scene", "turn_on", entity_target_body(model->entity_id));
        snprintf(description, sizeof(description), "Scene %s", model->name);
    }

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

        if (g_ws_authenticated && take_flag(g_discovery_requested)) {
            portENTER_CRITICAL(&g_mux);
            g_entity_count = 0;
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

        if (http_ready && g_action_queue) {
            HaAction action = {};
            if (xQueueReceive(g_action_queue, &action, 0) == pdTRUE) {
                process_action_worker(action);
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

bool queue_action(const HaAction &action) {
    if (!g_action_queue || !network_service_connected() ||
        !network_ready_snapshot() || !configured_snapshot()) return false;
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
