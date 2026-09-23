#pragma once

#include "home_assistant.h"
#include "module.h"

#include <stddef.h>
#include <stdint.h>

class MediaModule final : public PanelModule {
public:
    const char *id() const override { return "media"; }
    const char *title() const override { return "Media"; }
    void create(lv_obj_t *parent) override;
    void update() override;

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
    char requested_picture_[224] = {};
    bool volume_dragging_ = false;

    PlayerControl players_[HA_MAX_MEDIA_PLAYERS];
    SourceControl sources_[HA_MAX_MEDIA_SOURCES];
    FavoriteControl favorites_[HA_MAX_MEDIA_FAVORITES];

    // Media snapshots are roughly 1 KB each because they include metadata,
    // artwork URLs and source lists.  Keeping these work buffers off the
    // Arduino loopTask stack prevents a stack-protection fault when the lazily
    // created Media page performs its first update.
    HomeAssistantMediaSnapshot *media_cache_ = nullptr;
    HomeAssistantMediaFavorite *favorite_cache_ = nullptr;

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
    lv_obj_t *mute_button_ = nullptr;
    lv_obj_t *mute_label_ = nullptr;

    lv_obj_t *artwork_box_ = nullptr;
    lv_obj_t *artwork_image_ = nullptr;
    lv_obj_t *artwork_placeholder_ = nullptr;
    uint8_t *artwork_buffer_ = nullptr;
    size_t artwork_capacity_ = 0;
    uint32_t artwork_generation_ = 0;
    lv_image_dsc_t artwork_dsc_ = {};
    // LVGL 9.3 intentionally exposes lv_fs_path_ex_t as an opaque public type.
    // Keep only a pointer here so translation units that include this header do
    // not require LVGL's private structure definition.
    lv_fs_path_ex_t *artwork_jpeg_path_ = nullptr;

    void select_player(const char *entity_id);
    bool allocate_work_buffers();
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
    static void mute_cb(lv_event_t *e);
    static void source_cb(lv_event_t *e);
    static void favorite_cb(lv_event_t *e);
};
