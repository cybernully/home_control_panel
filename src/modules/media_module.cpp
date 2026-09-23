#include "media_module.h"

#include "module_ui.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <string.h>

using namespace module_ui;

namespace {

bool same_text(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

void configure_button_label(lv_obj_t *label_obj, int width) {
    if (!label_obj) return;
    lv_obj_set_width(label_obj, width);
    lv_label_set_long_mode(label_obj, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(label_obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(label_obj);
}

bool is_playing_state(const char *state) {
    return state && strcmp(state, "playing") == 0;
}

const char *media_title_or_state(const HomeAssistantMediaSnapshot &media) {
    if (media.title[0]) return media.title;
    if (strcmp(media.state, "off") == 0) return "Player is off";
    if (strcmp(media.state, "idle") == 0) return "Nothing playing";
    if (strcmp(media.state, "unavailable") == 0) return "Player unavailable";
    return "No media title";
}

}  // namespace

bool MediaModule::allocate_work_buffers() {
    bool allocated_now = false;
    if (!media_cache_) {
        media_cache_ = static_cast<HomeAssistantMediaSnapshot *>(
            heap_caps_calloc(HA_MAX_MEDIA_PLAYERS,
                             sizeof(HomeAssistantMediaSnapshot),
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!media_cache_) {
            media_cache_ = static_cast<HomeAssistantMediaSnapshot *>(
                calloc(HA_MAX_MEDIA_PLAYERS, sizeof(HomeAssistantMediaSnapshot)));
        }
        allocated_now = media_cache_ != nullptr;
    }

    if (!favorite_cache_) {
        favorite_cache_ = static_cast<HomeAssistantMediaFavorite *>(
            heap_caps_calloc(HA_MAX_MEDIA_FAVORITES,
                             sizeof(HomeAssistantMediaFavorite),
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!favorite_cache_) {
            favorite_cache_ = static_cast<HomeAssistantMediaFavorite *>(
                calloc(HA_MAX_MEDIA_FAVORITES, sizeof(HomeAssistantMediaFavorite)));
        }
        allocated_now = allocated_now || favorite_cache_ != nullptr;
    }

    if (!artwork_info_cache_) {
        artwork_info_cache_ = static_cast<HomeAssistantMediaArtworkInfo *>(
            heap_caps_calloc(2, sizeof(HomeAssistantMediaArtworkInfo),
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!artwork_info_cache_) {
            artwork_info_cache_ = static_cast<HomeAssistantMediaArtworkInfo *>(
                calloc(2, sizeof(HomeAssistantMediaArtworkInfo)));
        }
        allocated_now = allocated_now || artwork_info_cache_ != nullptr;
    }

    if (allocated_now && media_cache_ && favorite_cache_ && artwork_info_cache_) {
        Serial0.printf("[Media] Work buffers ready: %u bytes (PSRAM preferred)\n",
                       static_cast<unsigned>(
                           HA_MAX_MEDIA_PLAYERS * sizeof(HomeAssistantMediaSnapshot) +
                           HA_MAX_MEDIA_FAVORITES * sizeof(HomeAssistantMediaFavorite) +
                           2 * sizeof(HomeAssistantMediaArtworkInfo)));
    }
    return media_cache_ && favorite_cache_ && artwork_info_cache_;
}

void MediaModule::create(lv_obj_t *parent) {
    const bool buffers_ready = allocate_work_buffers();

    box(parent, BG, 0, 0);
    module_ui::title(parent, "Media", "Live Home Assistant media players assigned to this area");
    add_live_badge(parent);

    lv_obj_t *now = card(parent, 24, 92, 760, 394);
    player_name_ = label(now, "Discovering media players...", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(player_name_, 18, 18);
    lv_obj_set_width(player_name_, 710);
    lv_label_set_long_mode(player_name_, LV_LABEL_LONG_DOT);

    artwork_box_ = card(now, 18, 54, 214, 214);
    lv_obj_set_style_bg_color(artwork_box_, lv_color_hex(CARD_ALT), LV_PART_MAIN);
    artwork_placeholder_ = label(artwork_box_, "MEDIA", &lv_font_montserrat_24, MUTED);
    lv_obj_center(artwork_placeholder_);
    artwork_image_ = lv_image_create(artwork_box_);
    lv_obj_add_flag(artwork_image_, LV_OBJ_FLAG_HIDDEN);

    track_label_ = label(now, "Nothing playing", &lv_font_montserrat_24, TEXT);
    lv_obj_set_pos(track_label_, 254, 58);
    lv_obj_set_width(track_label_, 478);
    lv_label_set_long_mode(track_label_, LV_LABEL_LONG_DOT);

    artist_label_ = label(now, "", &lv_font_montserrat_16, MUTED);
    lv_obj_set_pos(artist_label_, 254, 100);
    lv_obj_set_width(artist_label_, 478);
    lv_label_set_long_mode(artist_label_, LV_LABEL_LONG_DOT);

    album_label_ = label(now, "", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(album_label_, 254, 132);
    lv_obj_set_width(album_label_, 478);
    lv_label_set_long_mode(album_label_, LV_LABEL_LONG_DOT);

    state_label_ = label(now, "Waiting for Home Assistant", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(state_label_, 254, 166);
    lv_obj_set_width(state_label_, 478);
    lv_label_set_long_mode(state_label_, LV_LABEL_LONG_DOT);

    prev_button_ = button(now, "PREV", 254, 214, 138, 70, CARD_ALT);
    lv_obj_add_event_cb(prev_button_, previous_cb, LV_EVENT_CLICKED, this);
    set_enabled(prev_button_, false);

    play_button_ = button(now, "PLAY", 404, 214, 176, 70, ACCENT);
    play_label_ = lv_obj_get_child(play_button_, 0);
    lv_obj_add_event_cb(play_button_, play_cb, LV_EVENT_CLICKED, this);
    set_enabled(play_button_, false);

    next_button_ = button(now, "NEXT", 592, 214, 138, 70, CARD_ALT);
    lv_obj_add_event_cb(next_button_, next_cb, LV_EVENT_CLICKED, this);
    set_enabled(next_button_, false);

    status_label_ = label(now, "Waiting for media discovery...", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(status_label_, 254, 314);
    lv_obj_set_width(status_label_, 478);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_WRAP);

    lv_obj_t *right = card(parent, 804, 92, 448, 394);
    lv_obj_t *ph = label(right, "Players", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(ph, 18, 16);

    for (int i = 0; i < HA_MAX_MEDIA_PLAYERS; ++i) {
        players_[i].owner = this;
        const int x = 18 + (i % 2) * 205;
        const int y = 50 + (i / 2) * 62;
        players_[i].button = button(right, "Available slot", x, y, 193, 52, CARD_ALT);
        players_[i].label = lv_obj_get_child(players_[i].button, 0);
        configure_button_label(players_[i].label, 169);
        set_enabled(players_[i].button, false);
        lv_obj_add_event_cb(players_[i].button, player_cb, LV_EVENT_CLICKED, &players_[i]);
    }

    lv_obj_t *vh = label(right, "Volume", &lv_font_montserrat_18, TEXT);
    lv_obj_set_pos(vh, 18, 188);
    volume_label_ = label(right, "--", &lv_font_montserrat_20, TEXT);
    lv_obj_align(volume_label_, LV_ALIGN_TOP_RIGHT, -20, 184);

    volume_slider_ = lv_slider_create(right);
    lv_obj_set_pos(volume_slider_, 24, 228);
    lv_obj_set_size(volume_slider_, 400, 28);
    lv_slider_set_range(volume_slider_, 0, 100);
    lv_slider_set_value(volume_slider_, 0, LV_ANIM_OFF);
    style_slider(volume_slider_);
    set_enabled(volume_slider_, false);
    lv_obj_add_event_cb(volume_slider_, volume_pressed_cb, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(volume_slider_, volume_changed_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(volume_slider_, volume_released_cb, LV_EVENT_RELEASED, this);

    mute_button_ = button(right, "MUTE", 24, 288, 400, 66, CARD_ALT);
    mute_label_ = lv_obj_get_child(mute_button_, 0);
    lv_obj_add_event_cb(mute_button_, mute_cb, LV_EVENT_CLICKED, this);
    set_enabled(mute_button_, false);

    lv_obj_t *bottom = card(parent, 24, 502, 1228, 130);
    lv_obj_t *sources_title = label(bottom, "Sources", &lv_font_montserrat_16, TEXT);
    lv_obj_set_pos(sources_title, 18, 10);
    for (int i = 0; i < HA_MAX_MEDIA_SOURCES; ++i) {
        sources_[i].owner = this;
        sources_[i].button = button(bottom, "--", 18 + i * 142, 44, 132, 58, CARD_ALT);
        sources_[i].label = lv_obj_get_child(sources_[i].button, 0);
        configure_button_label(sources_[i].label, 112);
        set_enabled(sources_[i].button, false);
        lv_obj_add_event_cb(sources_[i].button, source_cb, LV_EVENT_CLICKED, &sources_[i]);
    }

    lv_obj_t *favorites_title = label(bottom, "Favorites / playlists", &lv_font_montserrat_16, TEXT);
    lv_obj_set_pos(favorites_title, 624, 10);
    for (int i = 0; i < HA_MAX_MEDIA_FAVORITES; ++i) {
        favorites_[i].owner = this;
        favorites_[i].button = button(bottom, "--", 624 + i * 194, 44, 180, 58, CARD_ALT);
        favorites_[i].label = lv_obj_get_child(favorites_[i].button, 0);
        configure_button_label(favorites_[i].label, 158);
        set_enabled(favorites_[i].button, false);
        lv_obj_add_event_cb(favorites_[i].button, favorite_cb, LV_EVENT_CLICKED, &favorites_[i]);
    }

    if (!buffers_ready) {
        set_status("Media buffers could not be allocated; controls are disabled.");
    }
}

void MediaModule::set_status(const char *text) {
    if (status_label_) lv_label_set_text(status_label_, text ? text : "");
}

void MediaModule::select_player(const char *entity_id) {
    if (!entity_id || !entity_id[0] || same_text(selected_entity_id_, entity_id)) return;
    snprintf(selected_entity_id_, sizeof(selected_entity_id_), "%s", entity_id);
    requested_picture_[0] = '\0';
    clear_artwork();
    home_assistant_request_media_browse(selected_entity_id_);
    set_status("Media player selected; loading state, artwork, and favorites...");
}

void MediaModule::clear_artwork() {
    if (!artwork_image_) return;
    const void *old_src = lv_image_get_src(artwork_image_);
    if (old_src) lv_image_cache_drop(old_src);
    lv_obj_add_flag(artwork_image_, LV_OBJ_FLAG_HIDDEN);
    if (artwork_placeholder_) lv_obj_remove_flag(artwork_placeholder_, LV_OBJ_FLAG_HIDDEN);
    artwork_generation_ = 0;
}

void MediaModule::refresh_artwork() {
    if (!artwork_image_ || !selected_entity_id_[0] || !artwork_info_cache_) return;

    HomeAssistantMediaArtworkInfo &info = artwork_info_cache_[0];
    memset(&info, 0, sizeof(info));
    home_assistant_get_media_artwork_info(info);
    if (!info.generation || info.generation == artwork_generation_ ||
        !same_text(info.entity_id, selected_entity_id_) || info.data_size == 0) {
        return;
    }

    if (info.data_size > HA_MEDIA_ARTWORK_MAX_BYTES) return;

    // Drop any decoded/cache references before an old encoded backing buffer can
    // be released. The image widget is persistent across player/artwork changes.
    const void *old_src = lv_image_get_src(artwork_image_);
    if (old_src) lv_image_cache_drop(old_src);
    lv_obj_add_flag(artwork_image_, LV_OBJ_FLAG_HIDDEN);

    if (artwork_capacity_ < info.data_size) {
        uint8_t *next = static_cast<uint8_t *>(
            heap_caps_malloc(info.data_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!next) next = static_cast<uint8_t *>(malloc(info.data_size));
        if (!next) {
            set_status("Artwork could not be allocated in memory.");
            return;
        }
        if (artwork_buffer_) free(artwork_buffer_);
        artwork_buffer_ = next;
        artwork_capacity_ = info.data_size;
    }

    HomeAssistantMediaArtworkInfo &copied = artwork_info_cache_[1];
    memset(&copied, 0, sizeof(copied));
    if (!home_assistant_copy_media_artwork(artwork_buffer_, artwork_capacity_, copied) ||
        copied.generation != info.generation ||
        !same_text(copied.entity_id, selected_entity_id_)) {
        return;
    }

    if (copied.format == HomeAssistantArtworkFormat::Png ||
        copied.format == HomeAssistantArtworkFormat::Jpeg) {
        // Both enabled LVGL decoders accept a persistent RAW variable image.
        // This avoids LVGL 9.3's opaque/broken MEMFS path handling and keeps
        // the encoded bytes in PSRAM for as long as the widget references them.
        memset(&artwork_dsc_, 0, sizeof(artwork_dsc_));
        artwork_dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
        artwork_dsc_.header.cf = LV_COLOR_FORMAT_RAW;
        artwork_dsc_.header.w = copied.width;
        artwork_dsc_.header.h = copied.height;
        artwork_dsc_.data_size = copied.data_size;
        artwork_dsc_.data = artwork_buffer_;

        lv_image_header_t decoded_header = {};
        if (lv_image_decoder_get_info(&artwork_dsc_, &decoded_header) != LV_RESULT_OK) {
            Serial0.printf("[Media] Artwork decoder rejected %s variable image\n",
                           copied.format == HomeAssistantArtworkFormat::Jpeg ? "JPEG" : "PNG");
            set_status("Artwork downloaded, but the image decoder rejected it.");
            return;
        }
        lv_image_set_src(artwork_image_, &artwork_dsc_);
    } else {
        Serial0.println("[Media] Artwork ignored: unsupported cached format");
        return;
    }

    const uint32_t sx = copied.width ? (202U * 256U) / copied.width : 256U;
    const uint32_t sy = copied.height ? (202U * 256U) / copied.height : 256U;
    uint32_t scale = sx < sy ? sx : sy;
    if (scale > 256U) scale = 256U;
    if (scale < 16U) scale = 16U;
    lv_image_set_scale(artwork_image_, scale);
    lv_obj_center(artwork_image_);
    lv_obj_remove_flag(artwork_image_, LV_OBJ_FLAG_HIDDEN);
    if (artwork_placeholder_) lv_obj_add_flag(artwork_placeholder_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(artwork_image_);
    artwork_generation_ = copied.generation;
    Serial0.printf("[Media] Artwork shown: %s, %u bytes, %ux%u, scale=%u/256\n",
                   copied.format == HomeAssistantArtworkFormat::Jpeg ? "JPEG" : "PNG",
                   static_cast<unsigned>(copied.data_size),
                   static_cast<unsigned>(copied.width),
                   static_cast<unsigned>(copied.height),
                   static_cast<unsigned>(scale));
}

void MediaModule::update() {
    if (!allocate_work_buffers()) {
        set_status("Media buffers could not be allocated; controls are disabled.");
        return;
    }

    memset(media_cache_, 0,
           HA_MAX_MEDIA_PLAYERS * sizeof(HomeAssistantMediaSnapshot));
    const size_t count = home_assistant_get_media_players(
        media_cache_, HA_MAX_MEDIA_PLAYERS);

    if (count == 0) {
        selected_entity_id_[0] = '\0';
        requested_picture_[0] = '\0';
        clear_artwork();
        lv_label_set_text(player_name_, "No media_player entities found in this area");
        lv_label_set_text(track_label_, "Nothing playing");
        lv_label_set_text(artist_label_, "Assign a media player to the configured Home Assistant area.");
        lv_label_set_text(album_label_, "");
        lv_label_set_text(state_label_, "Waiting for area discovery");
        set_enabled(play_button_, false);
        set_enabled(prev_button_, false);
        set_enabled(next_button_, false);
        set_enabled(volume_slider_, false);
        set_enabled(mute_button_, false);
        for (auto &p : players_) { p.bound = false; set_enabled(p.button, false); lv_label_set_text(p.label, "Available slot"); }
        for (auto &s : sources_) { s.bound = false; set_enabled(s.button, false); lv_label_set_text(s.label, "--"); }
        for (auto &f : favorites_) { f.bound = false; set_enabled(f.button, false); lv_label_set_text(f.label, "--"); }
        set_status("No area media players discovered yet.");
        return;
    }

    size_t selected = count;
    for (size_t i = 0; i < count; ++i) {
        if (same_text(media_cache_[i].entity_id, selected_entity_id_)) {
            selected = i;
            break;
        }
    }
    if (selected == count) {
        select_player(media_cache_[0].entity_id);
        selected = 0;
    }

    for (size_t i = 0; i < HA_MAX_MEDIA_PLAYERS; ++i) {
        PlayerControl &control = players_[i];
        if (i < count) {
            control.bound = true;
            snprintf(control.entity_id, sizeof(control.entity_id), "%s",
                     media_cache_[i].entity_id);
            lv_label_set_text(control.label,
                              media_cache_[i].name[0]
                                  ? media_cache_[i].name
                                  : media_cache_[i].entity_id);
            set_enabled(control.button, media_cache_[i].available);
            lv_obj_set_style_bg_color(control.button,
                                      lv_color_hex(i == selected ? ACCENT : CARD_ALT), LV_PART_MAIN);
        } else {
            control.bound = false;
            control.entity_id[0] = '\0';
            lv_label_set_text(control.label, "Available slot");
            set_enabled(control.button, false);
            lv_obj_set_style_bg_color(control.button, lv_color_hex(CARD_ALT), LV_PART_MAIN);
        }
    }

    const HomeAssistantMediaSnapshot &active = media_cache_[selected];
    lv_label_set_text(player_name_, active.name[0] ? active.name : active.entity_id);
    lv_label_set_text(track_label_, media_title_or_state(active));

    char artist[128] = {};
    if (active.artist[0]) snprintf(artist, sizeof(artist), "%s", active.artist);
    else if (active.playlist[0]) snprintf(artist, sizeof(artist), "Playlist: %s", active.playlist);
    else snprintf(artist, sizeof(artist), "No artist metadata");
    lv_label_set_text(artist_label_, artist);

    char album[128] = {};
    if (active.album[0]) snprintf(album, sizeof(album), "Album: %s", active.album);
    else if (active.source[0]) snprintf(album, sizeof(album), "Source: %s", active.source);
    lv_label_set_text(album_label_, album);

    char state[160];
    snprintf(state, sizeof(state), "State: %s%s%s",
             active.state[0] ? active.state : "unknown",
             active.source[0] ? " | Source: " : "",
             active.source[0] ? active.source : "");
    lv_label_set_text(state_label_, state);

    set_enabled(play_button_, active.available);
    set_enabled(prev_button_, active.available);
    set_enabled(next_button_, active.available);
    lv_label_set_text(play_label_, is_playing_state(active.state) ? "PAUSE" : "PLAY");
    lv_obj_set_style_bg_color(play_button_, lv_color_hex(is_playing_state(active.state) ? ACCENT : CARD_ALT), LV_PART_MAIN);

    set_enabled(volume_slider_, active.available && active.supports_volume);
    if (!volume_dragging_) lv_slider_set_value(volume_slider_, active.volume_pct, LV_ANIM_OFF);
    char volume[20];
    if (active.supports_volume) {
        snprintf(volume, sizeof(volume), "%u%%", static_cast<unsigned>(active.volume_pct));
    } else {
        snprintf(volume, sizeof(volume), "--");
    }
    lv_label_set_text(volume_label_, volume);

    set_enabled(mute_button_, active.available && active.supports_mute);
    lv_label_set_text(mute_label_, active.volume_muted ? "UNMUTE" : "MUTE");
    lv_obj_set_style_bg_color(mute_button_, lv_color_hex(active.volume_muted ? ACCENT : CARD_ALT), LV_PART_MAIN);

    for (size_t i = 0; i < HA_MAX_MEDIA_SOURCES; ++i) {
        SourceControl &control = sources_[i];
        if (i < active.source_count) {
            control.bound = true;
            snprintf(control.source, sizeof(control.source), "%s", active.sources[i]);
            lv_label_set_text(control.label, control.source);
            set_enabled(control.button, active.available);
            lv_obj_set_style_bg_color(control.button,
                                      lv_color_hex(same_text(active.source, control.source) ? ACCENT : CARD_ALT),
                                      LV_PART_MAIN);
        } else {
            control.bound = false;
            control.source[0] = '\0';
            lv_label_set_text(control.label, "--");
            set_enabled(control.button, false);
            lv_obj_set_style_bg_color(control.button, lv_color_hex(CARD_ALT), LV_PART_MAIN);
        }
    }

    memset(favorite_cache_, 0,
           HA_MAX_MEDIA_FAVORITES * sizeof(HomeAssistantMediaFavorite));
    const size_t favorite_count = home_assistant_get_media_favorites(
        favorite_cache_, HA_MAX_MEDIA_FAVORITES);
    for (size_t i = 0; i < HA_MAX_MEDIA_FAVORITES; ++i) {
        FavoriteControl &control = favorites_[i];
        if (i < favorite_count &&
            same_text(favorite_cache_[i].entity_id, selected_entity_id_)) {
            control.bound = true;
            control.favorite = favorite_cache_[i];
            lv_label_set_text(control.label, favorite_cache_[i].title);
            set_enabled(control.button, active.available);
        } else {
            control.bound = false;
            memset(&control.favorite, 0, sizeof(control.favorite));
            lv_label_set_text(control.label, i == 0 ? "No browse items" : "--");
            set_enabled(control.button, false);
        }
    }

    if (active.entity_picture[0]) {
        if (!same_text(requested_picture_, active.entity_picture)) {
            snprintf(requested_picture_, sizeof(requested_picture_), "%s", active.entity_picture);
            if (!home_assistant_request_media_artwork(active.entity_id)) {
                requested_picture_[0] = '\0';
                Serial0.printf("[Media] Artwork request could not be queued for %s\n",
                               active.entity_id);
            }
        }
    } else {
        requested_picture_[0] = '\0';
        clear_artwork();
    }
    refresh_artwork();

    HomeAssistantDiscoveryStatus discovery = {};
    home_assistant_get_discovery_status(discovery);
    if (discovery.last_action_ms && discovery.last_action[0]) set_status(discovery.last_action);
    else set_status("Live state from Home Assistant. Commands update after HA confirms state.");
}

void MediaModule::player_cb(lv_event_t *e) {
    auto *control = static_cast<PlayerControl *>(lv_event_get_user_data(e));
    if (!control || !control->owner || !control->bound || !control->entity_id[0]) return;
    control->owner->select_player(control->entity_id);
}

void MediaModule::play_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (!self || !self->selected_entity_id_[0]) return;
    self->set_status(home_assistant_queue_media_play_pause(self->selected_entity_id_)
                         ? "Play/pause queued; waiting for Home Assistant."
                         : "Could not queue play/pause.");
}

void MediaModule::previous_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (!self || !self->selected_entity_id_[0]) return;
    self->set_status(home_assistant_queue_media_previous(self->selected_entity_id_)
                         ? "Previous track queued."
                         : "Could not queue previous track.");
}

void MediaModule::next_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (!self || !self->selected_entity_id_[0]) return;
    self->set_status(home_assistant_queue_media_next(self->selected_entity_id_)
                         ? "Next track queued."
                         : "Could not queue next track.");
}

void MediaModule::volume_pressed_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (self) self->volume_dragging_ = true;
}

void MediaModule::volume_changed_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (!self || !self->volume_label_) return;
    const int value = static_cast<int>(
        lv_slider_get_value(static_cast<lv_obj_t *>(lv_event_get_target(e))));
    char text[16];
    snprintf(text, sizeof(text), "%d%%", value);
    lv_label_set_text(self->volume_label_, text);
}

void MediaModule::volume_released_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (!self || !self->selected_entity_id_[0]) return;
    self->volume_dragging_ = false;
    const int value = static_cast<int>(
        lv_slider_get_value(static_cast<lv_obj_t *>(lv_event_get_target(e))));
    self->set_status(home_assistant_queue_media_volume(
                         self->selected_entity_id_, static_cast<uint8_t>(constrain(value, 0, 100)))
                         ? "Volume queued; waiting for Home Assistant."
                         : "Could not queue volume change.");
}

void MediaModule::mute_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (!self || !self->selected_entity_id_[0]) return;

    if (!self->allocate_work_buffers()) {
        self->set_status("Media buffers are unavailable.");
        return;
    }
    memset(self->media_cache_, 0,
           HA_MAX_MEDIA_PLAYERS * sizeof(HomeAssistantMediaSnapshot));
    const size_t count = home_assistant_get_media_players(
        self->media_cache_, HA_MAX_MEDIA_PLAYERS);
    bool muted = false;
    bool found = false;
    for (size_t i = 0; i < count; ++i) {
        if (same_text(self->media_cache_[i].entity_id,
                      self->selected_entity_id_)) {
            muted = self->media_cache_[i].volume_muted;
            found = true;
            break;
        }
    }
    if (!found) return;
    self->set_status(home_assistant_queue_media_mute(self->selected_entity_id_, !muted)
                         ? "Mute command queued; waiting for Home Assistant."
                         : "Could not queue mute command.");
}

void MediaModule::source_cb(lv_event_t *e) {
    auto *control = static_cast<SourceControl *>(lv_event_get_user_data(e));
    if (!control || !control->owner || !control->bound || !control->source[0] ||
        !control->owner->selected_entity_id_[0]) return;
    control->owner->set_status(
        home_assistant_queue_media_source(control->owner->selected_entity_id_, control->source)
            ? "Source change queued; waiting for Home Assistant."
            : "Could not queue source change.");
}

void MediaModule::favorite_cb(lv_event_t *e) {
    auto *control = static_cast<FavoriteControl *>(lv_event_get_user_data(e));
    if (!control || !control->owner || !control->bound) return;
    control->owner->set_status(home_assistant_queue_media_favorite(control->favorite)
                                   ? "Favorite/playlist queued; waiting for Home Assistant."
                                   : "Could not queue favorite/playlist.");
}
