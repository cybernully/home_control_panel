#pragma once
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
    char domain[16];
    char state[32];
    uint8_t brightness_pct;
    int16_t position_pct;
    bool available;
    bool supports_brightness;
    bool supports_position;
};

struct HomeAssistantLightStats {
    uint16_t total;
    uint16_t on;
    uint8_t average_brightness_pct;
};

void home_assistant_begin();
void home_assistant_loop();

bool home_assistant_request_health_check();
void home_assistant_get_status(HomeAssistantStatus &out);

bool home_assistant_request_discovery();
void home_assistant_get_discovery_status(HomeAssistantDiscoveryStatus &out);

size_t home_assistant_get_room_controls(HomeAssistantEntitySnapshot *out, size_t max_count);
size_t home_assistant_get_area_scenes(HomeAssistantEntitySnapshot *out, size_t max_count);
void home_assistant_get_light_stats(HomeAssistantLightStats &out);

bool home_assistant_queue_toggle(const char *entity_id);
bool home_assistant_queue_area_brightness(uint8_t brightness_pct);
bool home_assistant_queue_all_lights(bool turn_on);
bool home_assistant_queue_scene(const char *entity_id);

bool home_assistant_set_credentials(const char *base_url, const char *token);
String home_assistant_base_url();
bool home_assistant_token_configured();
