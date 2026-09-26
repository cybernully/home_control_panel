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
void home_assistant_get_discovery_status(HomeAssistantDiscoveryStatus &out);

size_t home_assistant_get_room_controls(HomeAssistantEntitySnapshot *out, size_t max_count);
size_t home_assistant_get_area_scenes(HomeAssistantEntitySnapshot *out, size_t max_count);
void home_assistant_get_light_stats(HomeAssistantLightStats &out);

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

bool home_assistant_set_credentials(const char *base_url, const char *token);
String home_assistant_base_url();
bool home_assistant_token_configured();

// All discovered controllable room entities and scenes, without UI slot limits.
size_t home_assistant_get_room_entities(HomeAssistantEntitySnapshot *out, size_t max_count);

bool home_assistant_queue_light_brightness(const char *entity_id, uint8_t brightness_pct);

bool home_assistant_get_room_entity(const char *entity_id, HomeAssistantEntitySnapshot &out);
