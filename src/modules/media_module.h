#pragma once

#include "home_assistant.h"
#include "module.h"

#include <stddef.h>
#include <stdint.h>

class JPEGDEC;

class MediaModule final : public PanelModule {
public:
    const char *id() const override { return "media"; }
    const char *title() const override { return "Media"; }
    void create(lv_obj_t *parent) override;
    void update() override;
    void on_deactivate() override;

private:
    struct PlayerControl {
        MediaModule *owner = nullptr;
        lv_obj_t *button = nullptr;
        lv_obj_t *label = nullptr;
        char entity_id[96] = {};
        bool bound = false;
    };

    struct SourceControl {
        MediaModule *owner = nullptr;
        lv_obj_t *button = nullptr;
        lv_obj_t *label = nullptr;
        char source[HA_MEDIA_SOURCE_NAME_LEN] = {};
        bool bound = false;
    };

    struct FavoriteControl {
        MediaModule *owner = nullptr;
        lv_obj_t *button = nullptr;
        lv_obj_t *label = nullptr;
        HomeAssistantMediaFavorite favorite = {};
        bool bound = false;
    };

    char selected_entity_id_[96] = {};
    char requested_picture_[HA_MEDIA_ARTWORK_URL_LEN] = {};
    uint32_t artwork_request_ms_ = 0;
    bool volume_dragging_ = false;

    PlayerControl players_[HA_MAX_MEDIA_PLAYERS];
    SourceControl sources_[HA_MAX_MEDIA_SOURCES];
    FavoriteControl shortcuts_[PANEL_MAX_MEDIA_SHORTCUTS];
    FavoriteControl favorites_[HA_MAX_MEDIA_FAVORITES];

    // Media snapshots are roughly 1 KB each because they include metadata,
    // artwork URLs and source lists.  Keeping these work buffers off the
    // Arduino loopTask stack prevents a stack-protection fault when the lazily
    // created Media page performs its first update.
    HomeAssistantMediaSnapshot *media_cache_ = nullptr;
    HomeAssistantMediaFavorite *favorite_cache_ = nullptr;
    HomeAssistantMediaArtworkInfo *artwork_info_cache_ = nullptr;

    struct PopupTrigger {
        MediaModule *owner = nullptr;
        int index = 0;
        lv_obj_t *button = nullptr;
    };
    PopupTrigger popup_triggers_[3];
    lv_obj_t *popup_ = nullptr, *popup_title_ = nullptr, *popup_hint_ = nullptr;
    lv_obj_t *popup_sections_[3] = {};
    lv_obj_t *popup_empty_[3] = {};
    lv_obj_t *popup_status_ = nullptr, *shortcuts_empty_ = nullptr;
    char volume_entity_id_[96] = {};
    static void popup_cb(lv_event_t *e);
    static void close_popup_cb(lv_event_t *e);
    static void volume_cancel_cb(lv_event_t *e);

    lv_obj_t *player_name_ = nullptr;
    lv_obj_t *track_label_ = nullptr;
    lv_obj_t *artist_label_ = nullptr;
    lv_obj_t *album_label_ = nullptr;
    lv_obj_t *state_label_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *play_button_ = nullptr;
    lv_obj_t *play_label_ = nullptr;
    lv_obj_t *prev_button_ = nullptr;
    lv_obj_t *next_button_ = nullptr;
    lv_obj_t *volume_slider_ = nullptr;
    lv_obj_t *volume_label_ = nullptr;
    lv_obj_t *volume_down_button_ = nullptr;
    lv_obj_t *volume_up_button_ = nullptr;
    lv_obj_t *mute_button_ = nullptr;
    lv_obj_t *mute_label_ = nullptr;

    lv_obj_t *artwork_box_ = nullptr;
    lv_obj_t *artwork_image_ = nullptr;
    lv_obj_t *artwork_placeholder_ = nullptr;
    uint8_t *artwork_buffer_ = nullptr;
    size_t artwork_capacity_ = 0;
    uint8_t *artwork_pixels_ = nullptr;
    size_t artwork_pixels_capacity_ = 0;
    JPEGDEC *jpeg_decoder_ = nullptr;
    uint32_t artwork_generation_ = 0;
    lv_image_dsc_t artwork_dsc_ = {};

    void select_player(const char *entity_id);
    bool allocate_work_buffers();
    bool decode_jpeg_artwork(const HomeAssistantMediaArtworkInfo &info,
                             uint16_t &decoded_width, uint16_t &decoded_height,
                             bool &progressive);
    bool decode_progressive_jpeg_artwork(const HomeAssistantMediaArtworkInfo &info,
                                         uint16_t &decoded_width, uint16_t &decoded_height);
    void clear_artwork();
    void refresh_artwork();
    void set_status(const char *text);

    static void player_cb(lv_event_t *e);
    static void play_cb(lv_event_t *e);
    static void previous_cb(lv_event_t *e);
    static void next_cb(lv_event_t *e);
    static void volume_pressed_cb(lv_event_t *e);
    static void volume_changed_cb(lv_event_t *e);
    static void volume_released_cb(lv_event_t *e);
    static void volume_down_cb(lv_event_t *e);
    static void volume_up_cb(lv_event_t *e);
    static void mute_cb(lv_event_t *e);
    static void source_cb(lv_event_t *e);
    static void favorite_cb(lv_event_t *e);
};
