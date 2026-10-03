#include "media_module.h"

#include "config_service.h"
#include "module_ui.h"
#include "display_text.h"
#include "ui_state_model.h"

#include <Arduino.h>
#include <JPEGDEC.h>
#include <esp_heap_caps.h>
#include <new>
#include <initializer_list>
#include <stb_image.h>
#include <stdlib.h>
#include <string.h>

using namespace module_ui;

namespace {

bool same_text(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

const char *media_command_unavailable_message() {
    return home_assistant_commands_ready() ? "Could not queue command." :
                                             "Home Assistant reconnecting. Please wait.";
}

void configure_button_label(lv_obj_t *label_obj, int width) {
    if (!label_obj) return;
    lv_obj_set_width(label_obj, width);
    lv_obj_set_height(label_obj, lv_obj_get_style_text_font(label_obj, LV_PART_MAIN)->line_height);
    lv_label_set_long_mode(label_obj, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(label_obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(label_obj);
}

lv_obj_t *configure_icon_button(lv_obj_t *button_obj, const char *icon,
                                const char *caption, int caption_width,
                                int icon_y = 8, int caption_y = 38) {
    if (!button_obj) return nullptr;
    lv_obj_t *icon_label = lv_obj_get_child(button_obj, 0);
    lv_label_set_text(icon_label, icon);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(icon_label, LV_ALIGN_TOP_MID, 0, icon_y);
    lv_obj_t *caption_label = lv_label_create(button_obj);
    lv_label_set_text(caption_label, caption ? caption : "");
    lv_obj_set_style_text_font(caption_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(caption_label, lv_color_hex(TEXT), LV_PART_MAIN);
    lv_obj_set_width(caption_label, caption_width);
    lv_obj_set_height(caption_label, lv_font_montserrat_12.line_height);
    lv_label_set_long_mode(caption_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(caption_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(caption_label, LV_ALIGN_TOP_MID, 0, caption_y);
    return caption_label;
}

const char *shortcut_icon(const char *name) {
    if (same_text(name, "radio")) return LV_SYMBOL_WIFI;
    if (same_text(name, "podcast")) return LV_SYMBOL_AUDIO;
    if (same_text(name, "playlist")) return LV_SYMBOL_LIST;
    if (same_text(name, "favorite") || same_text(name, "star")) return LV_SYMBOL_OK;
    if (same_text(name, "speaker")) return LV_SYMBOL_VOLUME_MAX;
    return LV_SYMBOL_AUDIO;
}

uint32_t media_fingerprint(const HomeAssistantMediaSnapshot &media) {
    uint32_t hash = 2166136261U;
    const char *parts[] = {media.entity_id, media.entity_picture, media.title,
                           media.artist, media.album};
    for (const char *part : parts) {
        for (const unsigned char *cursor =
                 reinterpret_cast<const unsigned char *>(part);
             cursor && *cursor; ++cursor) {
            hash ^= *cursor;
            hash *= 16777619U;
        }
        hash ^= 0xFFU;
        hash *= 16777619U;
    }
    return hash ? hash : 1U;
}

void set_media_label_text(lv_obj_t *label_obj, const char *text) {
    if (!label_obj) return;
    char normalized[256];
    panel_display_text(normalized, sizeof(normalized), text);
    lv_label_set_text(label_obj, normalized);
}

struct ArtworkDecodeTarget {
    uint16_t *pixels = nullptr;
    uint16_t width = 0;
    uint16_t height = 0;
    bool valid = true;
};

int copy_jpeg_pixels(JPEGDRAW *draw) {
    if (!draw || !draw->pUser || !draw->pPixels) return 0;
    auto *target = static_cast<ArtworkDecodeTarget *>(draw->pUser);
    if (!target->pixels || draw->x < 0 || draw->y < 0 ||
        draw->x >= target->width || draw->y >= target->height) {
        target->valid = false;
        return 0;
    }

    uint32_t copy_width = draw->iWidthUsed > 0
                              ? static_cast<uint32_t>(draw->iWidthUsed)
                              : static_cast<uint32_t>(draw->iWidth);
    uint32_t copy_height = static_cast<uint32_t>(draw->iHeight);
    if (copy_width > static_cast<uint32_t>(draw->iWidth)) {
        copy_width = static_cast<uint32_t>(draw->iWidth);
    }
    if (static_cast<uint32_t>(draw->x) + copy_width > target->width) {
        copy_width = target->width - static_cast<uint32_t>(draw->x);
    }
    if (static_cast<uint32_t>(draw->y) + copy_height > target->height) {
        copy_height = target->height - static_cast<uint32_t>(draw->y);
    }

    for (uint32_t row = 0; row < copy_height; ++row) {
        memcpy(target->pixels + (static_cast<uint32_t>(draw->y) + row) * target->width + draw->x,
               draw->pPixels + row * draw->iWidth,
               copy_width * sizeof(uint16_t));
    }
    return 1;
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
    module_ui::title(parent, "Media", "Now playing, shortcuts, and favorites");

    const char *menus[] = {"Players", "Sources", "Favorites"};
    const char *menu_icons[] = {LV_SYMBOL_AUDIO, LV_SYMBOL_LIST, LV_SYMBOL_OK};
    for (int i = 0; i < 3; ++i) {
        auto &trigger = popup_triggers_[i];
        trigger.owner = this; trigger.index = i;
        trigger.button = button(parent, menu_icons[i], 856 + i * 132, 8, 120, 72);
        lv_obj_set_style_radius(trigger.button, 28, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(trigger.button, 0, LV_PART_MAIN);
        configure_icon_button(trigger.button, menu_icons[i], menus[i], 104, 5, 40);
        lv_obj_add_event_cb(trigger.button, popup_cb, LV_EVENT_CLICKED, &trigger);
    }

    lv_obj_t *now = card(parent, 24, PAGE_CONTENT_TOP, 1232, 356);
    lv_obj_set_style_radius(now, 24, LV_PART_MAIN);
    artwork_box_ = card(now, 22, 22, 312, 312);
    lv_obj_set_style_bg_color(artwork_box_, lv_color_hex(0x101A2B), LV_PART_MAIN);
    lv_obj_set_style_radius(artwork_box_, 20, LV_PART_MAIN);
    lv_obj_set_style_border_width(artwork_box_, 0, LV_PART_MAIN);
    artwork_placeholder_ = label(artwork_box_, "No artwork", &lv_font_montserrat_16, MUTED);
    lv_obj_center(artwork_placeholder_);
    artwork_image_ = lv_image_create(artwork_box_);
    lv_obj_set_style_radius(artwork_image_, 12, LV_PART_MAIN);
    lv_image_set_antialias(artwork_image_, true);
    lv_obj_remove_flag(artwork_image_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(artwork_image_, LV_OBJ_FLAG_HIDDEN);

    auto text = [now](const char *value, const lv_font_t *font, uint32_t color, int y, int width) {
        lv_obj_t *obj = label(now, value, font, color);
        lv_obj_set_pos(obj, 376, y);
        lv_obj_set_width(obj, width);
        lv_obj_set_height(obj, font->line_height);
        lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
        return obj;
    };
    player_name_ = text("Discovering players...", &lv_font_montserrat_14, MUTED, 22, 828);
    track_label_ = text("Nothing playing", &lv_font_montserrat_28, TEXT, 54, 828);
    artist_label_ = text("", &lv_font_montserrat_18, TEXT, 96, 828);
    album_label_ = text("", &lv_font_montserrat_14, MUTED, 128, 828);
    state_label_ = text("Waiting for Home Assistant", &lv_font_montserrat_14, MUTED, 162, 420);

    prev_button_ = button(now, LV_SYMBOL_PREV, 376, 210, 76, 76);
    play_button_ = button(now, LV_SYMBOL_PLAY, 466, 198, 100, 100, ACCENT);
    next_button_ = button(now, LV_SYMBOL_NEXT, 580, 210, 76, 76);
    configure_icon_button(prev_button_, LV_SYMBOL_PREV, "Previous", 68, 7, 47);
    play_icon_ = lv_obj_get_child(play_button_, 0);
    play_label_ = configure_icon_button(play_button_, LV_SYMBOL_PLAY, "Play", 88, 14, 65);
    configure_icon_button(next_button_, LV_SYMBOL_NEXT, "Next", 68, 7, 47);
    for (auto *control : {prev_button_, play_button_, next_button_}) {
        lv_obj_set_style_radius(control, 50, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(control, 0, LV_PART_MAIN);
        set_enabled(control, false);
    }
    lv_obj_add_event_cb(prev_button_, previous_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(play_button_, play_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(next_button_, next_cb, LV_EVENT_CLICKED, this);

    auto *volume_title = label(now, "VOLUME", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(volume_title, 714, 174);
    volume_label_ = label(now, "--", &lv_font_montserrat_16, TEXT);
    lv_obj_set_width(volume_label_, 64);
    lv_obj_set_style_text_align(volume_label_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_pos(volume_label_, 1140, 169);
    volume_down_button_ = button(now, LV_SYMBOL_MINUS, 714, 224, 58, 58);
    volume_up_button_ = button(now, LV_SYMBOL_PLUS, 1094, 224, 58, 58);
    for (auto *control : {volume_down_button_, volume_up_button_}) {
        lv_obj_set_style_radius(control, 28, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(control, 0, LV_PART_MAIN);
        lv_obj_set_style_text_font(lv_obj_get_child(control, 0), &lv_font_montserrat_24, LV_PART_MAIN);
        set_enabled(control, false);
    }
    lv_obj_add_event_cb(volume_down_button_, volume_down_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(volume_up_button_, volume_up_cb, LV_EVENT_CLICKED, this);
    volume_slider_ = lv_slider_create(now);
    lv_obj_set_pos(volume_slider_, 794, 241);
    lv_obj_set_size(volume_slider_, 286, 24);
    lv_obj_set_ext_click_area(volume_slider_, 12);
    lv_slider_set_range(volume_slider_, 0, 100);
    style_slider(volume_slider_);
    lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(BORDER), LV_PART_MAIN);
    set_enabled(volume_slider_, false);
    lv_obj_add_event_cb(volume_slider_, volume_pressed_cb, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(volume_slider_, volume_changed_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(volume_slider_, volume_released_cb, LV_EVENT_RELEASED, this);
    lv_obj_add_event_cb(volume_slider_, volume_cancel_cb, LV_EVENT_PRESS_LOST, this);
    mute_button_ = button(now, LV_SYMBOL_MUTE, 1162, 216, 64, 76);
    lv_obj_set_style_radius(mute_button_, 32, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(mute_button_, 0, LV_PART_MAIN);
    mute_icon_ = lv_obj_get_child(mute_button_, 0);
    mute_label_ = configure_icon_button(mute_button_, LV_SYMBOL_MUTE, "Mute", 56, 7, 48);
    lv_obj_add_event_cb(mute_button_, mute_cb, LV_EVENT_CLICKED, this);
    set_enabled(mute_button_, false);

    auto *caption = label(parent, "QUICK PLAY", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(caption, 24, 458);
    for (int i = 0; i < PANEL_MAX_MEDIA_SHORTCUTS; ++i) {
        auto &control = shortcuts_[i]; control.owner = this;
        control.button = button(parent, LV_SYMBOL_AUDIO, 24 + i * 204, 482, 194, 98);
        lv_obj_set_style_radius(control.button, 20, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(control.button, 0, LV_PART_MAIN);
        control.icon_label = lv_obj_get_child(control.button, 0);
        control.label = configure_icon_button(control.button, LV_SYMBOL_AUDIO, "Shortcut", 166, 10, 58);
        set_enabled(control.button, false);
        lv_obj_add_flag(control.button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(control.button, favorite_cb, LV_EVENT_CLICKED, &control);
    }
    shortcuts_empty_ = label(parent, "Add your favorite playlists or stations in the web manager.", &lv_font_montserrat_16, MUTED);
    lv_obj_set_pos(shortcuts_empty_, 24, 520);
    status_label_ = label(parent, "Waiting for media discovery...", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(status_label_, 24, 590);
    lv_obj_set_width(status_label_, 1232);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_DOT);

    // Persistent page-owned popups match the room screen and avoid dense button strips.
    popup_ = card(parent, 0, 0, 1280, 658);
    box(popup_, 0x030712, 0, 0);
    lv_obj_set_style_bg_opa(popup_, LV_OPA_80, LV_PART_MAIN);
    lv_obj_add_event_cb(popup_, close_popup_cb, LV_EVENT_CLICKED, this);
    auto *sheet = card(popup_, 124, 72, 1032, 514);
    lv_obj_set_style_radius(sheet, 28, LV_PART_MAIN);
    popup_title_ = label(sheet, "", &lv_font_montserrat_24, TEXT);
    lv_obj_set_pos(popup_title_, 24, 24);
    popup_hint_ = label(sheet, "", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(popup_hint_, 24, 61);
    auto *close = button(sheet, LV_SYMBOL_CLOSE, 940, 18, 68, 56);
    lv_obj_set_style_radius(close, 28, LV_PART_MAIN);
    configure_icon_button(close, LV_SYMBOL_CLOSE, "Close", 60, 1, 34);
    lv_obj_add_event_cb(close, close_popup_cb, LV_EVENT_CLICKED, this);
    for (int group = 0; group < 3; ++group) {
        auto *section = card(sheet, 24, 104, 984, 324);
        box(section, CARD, 0, 0);
        popup_sections_[group] = section;
        popup_empty_[group] = label(section, "Waiting for Home Assistant...", &lv_font_montserrat_18, MUTED);
        lv_obj_set_pos(popup_empty_[group], 24, 104);
        lv_obj_set_width(popup_empty_[group], 930);
        lv_label_set_long_mode(popup_empty_[group], LV_LABEL_LONG_WRAP);
        lv_obj_add_flag(section, LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < HA_MAX_MEDIA_PLAYERS; ++i) {
        auto &control = players_[i]; control.owner = this;
        control.button = button(popup_sections_[0], "", (i % 3) * 328, (i / 3) * 154, 312, 138);
        lv_obj_set_style_radius(control.button, 22, LV_PART_MAIN);
        control.label = lv_obj_get_child(control.button, 0);
        configure_button_label(control.label, 272);
        set_enabled(control.button, false);
        lv_obj_add_flag(control.button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(control.button, player_cb, LV_EVENT_CLICKED, &control);
    }
    for (int i = 0; i < HA_MAX_MEDIA_SOURCES; ++i) {
        auto &control = sources_[i]; control.owner = this;
        control.button = button(popup_sections_[1], "", (i % 2) * 500, (i / 2) * 132, 484, 116);
        lv_obj_set_style_radius(control.button, 22, LV_PART_MAIN);
        control.label = lv_obj_get_child(control.button, 0);
        configure_button_label(control.label, 436);
        set_enabled(control.button, false);
        lv_obj_add_flag(control.button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(control.button, source_cb, LV_EVENT_CLICKED, &control);
    }
    for (int i = 0; i < PANEL_MAX_MEDIA_FAVORITES; ++i) {
        auto &control = favorites_[i]; control.owner = this;
        control.button = button(popup_sections_[2], LV_SYMBOL_OK, (i % 3) * 328, (i / 3) * 154, 312, 138);
        lv_obj_set_style_radius(control.button, 22, LV_PART_MAIN);
        control.icon_label = lv_obj_get_child(control.button, 0);
        control.label = configure_icon_button(control.button, LV_SYMBOL_OK, "Favorite", 272, 18, 82);
        set_enabled(control.button, false);
        lv_obj_add_flag(control.button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(control.button, favorite_cb, LV_EVENT_CLICKED, &control);
    }
    popup_status_ = label(sheet, "", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(popup_status_, 24, 470);
    lv_obj_set_width(popup_status_, 984);
    lv_label_set_long_mode(popup_status_, LV_LABEL_LONG_DOT);
    lv_obj_add_flag(popup_, LV_OBJ_FLAG_HIDDEN);
    if (!buffers_ready) set_status("Media buffers could not be allocated; controls are disabled.");
}

void MediaModule::popup_cb(lv_event_t *e) {
    auto *trigger = static_cast<PopupTrigger *>(lv_event_get_user_data(e));
    auto *self = trigger->owner;
    self->update();
    const char *titles[] = {"Players", "Sources", "Browse favorites"};
    const char *hints[] = {"Choose where to listen.", "Choose an input for the selected player.", "Your configured favorites appear first; Home Assistant browse items fill remaining spots."};
    lv_label_set_text(self->popup_title_, titles[trigger->index]);
    lv_label_set_text(self->popup_hint_, hints[trigger->index]);
    for (int i = 0; i < 3; ++i) {
        if (i == trigger->index) lv_obj_remove_flag(self->popup_sections_[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(self->popup_sections_[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_remove_flag(self->popup_, LV_OBJ_FLAG_HIDDEN);
}

void MediaModule::close_popup_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (self->popup_) lv_obj_add_flag(self->popup_, LV_OBJ_FLAG_HIDDEN);
}

void MediaModule::on_deactivate() {
    volume_dragging_ = false;
    volume_entity_id_[0] = 0;
    if (popup_) lv_obj_add_flag(popup_, LV_OBJ_FLAG_HIDDEN);
}

void MediaModule::set_status(const char *text) {
    set_media_label_text(status_label_, text);
    set_media_label_text(popup_status_, text);
}

void MediaModule::select_player(const char *entity_id) {
    if (!entity_id || !entity_id[0] || same_text(selected_entity_id_, entity_id)) return;
    snprintf(selected_entity_id_, sizeof(selected_entity_id_), "%s", entity_id);
    volume_dragging_ = false;
    volume_entity_id_[0] = 0;
    requested_picture_[0] = '\0';
    requested_media_fingerprint_ = 0;
    artwork_request_ms_ = 0;
    artwork_refresh_pending_ = false;
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

bool MediaModule::decode_jpeg_artwork(const HomeAssistantMediaArtworkInfo &info,
                                      uint16_t &decoded_width, uint16_t &decoded_height,
                                      bool &progressive) {
    decoded_width = 0;
    decoded_height = 0;
    progressive = false;

    if (!jpeg_decoder_) {
        void *storage = heap_caps_malloc(sizeof(JPEGDEC), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!storage) storage = malloc(sizeof(JPEGDEC));
        if (!storage) return false;
        jpeg_decoder_ = new (storage) JPEGDEC();
    }

    if (!jpeg_decoder_->openRAM(artwork_buffer_, static_cast<int>(info.data_size),
                                copy_jpeg_pixels)) {
        Serial0.printf("[Media] JPEGDEC open failed: error=%d\n",
                       jpeg_decoder_->getLastError());
        return false;
    }

    const int source_width = jpeg_decoder_->getWidth();
    const int source_height = jpeg_decoder_->getHeight();
    progressive = jpeg_decoder_->getJPEGType() == JPEG_MODE_PROGRESSIVE;
    if (progressive) {
        jpeg_decoder_->close();
        return decode_progressive_jpeg_artwork(info, decoded_width, decoded_height);
    }
    const int longest_side = source_width > source_height ? source_width : source_height;

    int divisor = 1;
    int options = 0;
    if (longest_side > 256) {
        if (longest_side <= 512) {
            divisor = 2;
            options = JPEG_SCALE_HALF;
        } else if (longest_side <= 1024) {
            divisor = 4;
            options = JPEG_SCALE_QUARTER;
        } else {
            divisor = 8;
            options = JPEG_SCALE_EIGHTH;
        }
    }

    const int output_width = (source_width + divisor - 1) / divisor;
    const int output_height = (source_height + divisor - 1) / divisor;
    if (source_width <= 0 || source_height <= 0 || output_width <= 0 || output_height <= 0 ||
        output_width > 512 || output_height > 512) {
        Serial0.printf("[Media] JPEG dimensions rejected: source=%dx%d output=%dx%d\n",
                       source_width, source_height, output_width, output_height);
        jpeg_decoder_->close();
        return false;
    }

    const size_t required = static_cast<size_t>(output_width) * output_height * sizeof(uint16_t);
    if (artwork_pixels_capacity_ < required) {
        uint8_t *next = static_cast<uint8_t *>(
            heap_caps_malloc(required, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!next) next = static_cast<uint8_t *>(malloc(required));
        if (!next) {
            jpeg_decoder_->close();
            return false;
        }
        if (artwork_pixels_) free(artwork_pixels_);
        artwork_pixels_ = next;
        artwork_pixels_capacity_ = required;
    }
    memset(artwork_pixels_, 0, required);

    ArtworkDecodeTarget target = {
        reinterpret_cast<uint16_t *>(artwork_pixels_),
        static_cast<uint16_t>(output_width),
        static_cast<uint16_t>(output_height),
        true,
    };
    jpeg_decoder_->setUserPointer(&target);
    jpeg_decoder_->setPixelType(RGB565_LITTLE_ENDIAN);
    const bool decoded = jpeg_decoder_->decode(0, 0, options) != 0;
    const int decode_error = jpeg_decoder_->getLastError();
    jpeg_decoder_->close();
    if (!decoded || !target.valid) {
        Serial0.printf("[Media] JPEGDEC decode failed: error=%d callback=%s\n",
                       decode_error, target.valid ? "ok" : "invalid");
        return false;
    }

    decoded_width = target.width;
    decoded_height = target.height;
    return true;
}

bool MediaModule::decode_progressive_jpeg_artwork(
    const HomeAssistantMediaArtworkInfo &info,
    uint16_t &decoded_width, uint16_t &decoded_height) {
    int source_width = 0;
    int source_height = 0;
    int source_channels = 0;
    if (!stbi_info_from_memory(artwork_buffer_, static_cast<int>(info.data_size),
                               &source_width, &source_height, &source_channels) ||
        source_width <= 0 || source_height <= 0 ||
        source_width > 1024 || source_height > 1024) {
        Serial0.printf("[Media] Progressive JPEG dimensions rejected: %dx%d\n",
                       source_width, source_height);
        return false;
    }

    int loaded_width = 0;
    int loaded_height = 0;
    int loaded_channels = 0;
    stbi_uc *rgb = stbi_load_from_memory(
        artwork_buffer_, static_cast<int>(info.data_size),
        &loaded_width, &loaded_height, &loaded_channels, 3);
    if (!rgb) {
        Serial0.printf("[Media] Full progressive JPEG decode failed: %s\n",
                       stbi_failure_reason() ? stbi_failure_reason() : "unknown error");
        return false;
    }

    uint32_t output_width = static_cast<uint32_t>(loaded_width);
    uint32_t output_height = static_cast<uint32_t>(loaded_height);
    if (output_width > 256U || output_height > 256U) {
        if (output_width >= output_height) {
            output_height = (output_height * 256U + output_width / 2U) / output_width;
            output_width = 256U;
        } else {
            output_width = (output_width * 256U + output_height / 2U) / output_height;
            output_height = 256U;
        }
    }
    if (output_width == 0) output_width = 1;
    if (output_height == 0) output_height = 1;

    const size_t required = static_cast<size_t>(output_width) * output_height * sizeof(uint16_t);
    if (artwork_pixels_capacity_ < required) {
        uint8_t *next = static_cast<uint8_t *>(
            heap_caps_malloc(required, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!next) next = static_cast<uint8_t *>(malloc(required));
        if (!next) {
            stbi_image_free(rgb);
            return false;
        }
        if (artwork_pixels_) free(artwork_pixels_);
        artwork_pixels_ = next;
        artwork_pixels_capacity_ = required;
    }

    auto *pixels = reinterpret_cast<uint16_t *>(artwork_pixels_);
    for (uint32_t y = 0; y < output_height; ++y) {
        const uint32_t source_y = y * static_cast<uint32_t>(loaded_height) / output_height;
        for (uint32_t x = 0; x < output_width; ++x) {
            const uint32_t source_x = x * static_cast<uint32_t>(loaded_width) / output_width;
            const stbi_uc *source = rgb +
                (source_y * static_cast<uint32_t>(loaded_width) + source_x) * 3U;
            pixels[y * output_width + x] =
                static_cast<uint16_t>(((source[0] & 0xF8U) << 8U) |
                                      ((source[1] & 0xFCU) << 3U) |
                                      (source[2] >> 3U));
        }
    }
    stbi_image_free(rgb);

    decoded_width = static_cast<uint16_t>(output_width);
    decoded_height = static_cast<uint16_t>(output_height);
    Serial0.printf("[Media] Progressive JPEG decoded full resolution: %dx%d -> %ux%u\n",
                   loaded_width, loaded_height,
                   static_cast<unsigned>(decoded_width),
                   static_cast<unsigned>(decoded_height));
    return true;
}

void MediaModule::refresh_artwork() {
    if (!artwork_image_ || !selected_entity_id_[0] || !artwork_info_cache_) return;

    HomeAssistantMediaArtworkInfo &info = artwork_info_cache_[0];
    memset(&info, 0, sizeof(info));
    home_assistant_get_media_artwork_info(info);
    if (!info.generation || info.generation == artwork_generation_ ||
        !same_text(info.entity_id, selected_entity_id_) ||
        !same_text(info.picture_url, requested_picture_) || info.data_size == 0) {
        return;
    }

    if (info.data_size > HA_MEDIA_ARTWORK_MAX_BYTES) return;

    // Drop any decoded/cache references before an old encoded backing buffer can
    // be released. The image widget is persistent across player/artwork changes.
    const void *old_src = lv_image_get_src(artwork_image_);
    if (old_src) lv_image_cache_drop(old_src);
    lv_obj_add_flag(artwork_image_, LV_OBJ_FLAG_HIDDEN);
    if (artwork_placeholder_) lv_obj_remove_flag(artwork_placeholder_, LV_OBJ_FLAG_HIDDEN);

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

    bool progressive_jpeg = false;
    if (copied.format == HomeAssistantArtworkFormat::Jpeg) {
        uint16_t decoded_width = 0;
        uint16_t decoded_height = 0;
        if (!decode_jpeg_artwork(copied, decoded_width, decoded_height, progressive_jpeg)) {
            set_status("JPEG artwork downloaded, but decoding failed.");
            return;
        }
        memset(&artwork_dsc_, 0, sizeof(artwork_dsc_));
        artwork_dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
        artwork_dsc_.header.cf = LV_COLOR_FORMAT_RGB565;
        artwork_dsc_.header.w = decoded_width;
        artwork_dsc_.header.h = decoded_height;
        artwork_dsc_.header.stride = decoded_width * sizeof(uint16_t);
        artwork_dsc_.data_size = static_cast<uint32_t>(decoded_width) * decoded_height * sizeof(uint16_t);
        artwork_dsc_.data = artwork_pixels_;
    } else if (copied.format == HomeAssistantArtworkFormat::Png) {
        memset(&artwork_dsc_, 0, sizeof(artwork_dsc_));
        artwork_dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
        artwork_dsc_.header.cf = LV_COLOR_FORMAT_RAW;
        artwork_dsc_.header.w = copied.width;
        artwork_dsc_.header.h = copied.height;
        artwork_dsc_.data_size = copied.data_size;
        artwork_dsc_.data = artwork_buffer_;
    } else {
        Serial0.println("[Media] Artwork ignored: unsupported cached format");
        return;
    }

    lv_image_header_t decoded_header = {};
    if (lv_image_decoder_get_info(&artwork_dsc_, &decoded_header) != LV_RESULT_OK) {
        Serial0.printf("[Media] Artwork decoder rejected %s variable image\n",
                       copied.format == HomeAssistantArtworkFormat::Jpeg ? "RGB565 JPEG" : "PNG");
        set_status("Artwork decoded, but LVGL rejected the image buffer.");
        return;
    }
    lv_image_set_src(artwork_image_, &artwork_dsc_);
    const uint32_t shown_width = artwork_dsc_.header.w;
    const uint32_t shown_height = artwork_dsc_.header.h;
    const uint32_t sx = shown_width ? (304U * 256U) / shown_width : 256U;
    const uint32_t sy = shown_height ? (304U * 256U) / shown_height : 256U;
    uint32_t scale = sx < sy ? sx : sy;
    // Preserve native artwork pixels.  Images may be reduced to fit the well,
    // but are never enlarged beyond their decoded resolution.
    if (scale > 256U) scale = 256U;
    if (scale < 16U) scale = 16U;
    lv_image_set_scale(artwork_image_, scale);
    lv_obj_center(artwork_image_);
    lv_obj_remove_flag(artwork_image_, LV_OBJ_FLAG_HIDDEN);
    if (artwork_placeholder_) lv_obj_add_flag(artwork_placeholder_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(artwork_image_);
    artwork_generation_ = copied.generation;
    artwork_refresh_pending_ = false;
    Serial0.printf("[Media] Artwork shown: %s%s, %u bytes, %ux%u, scale=%u/256\n",
                   copied.format == HomeAssistantArtworkFormat::Jpeg ? "JPEG" : "PNG",
                   progressive_jpeg ? " progressive full" : "",
                   static_cast<unsigned>(copied.data_size),
                   static_cast<unsigned>(shown_width),
                   static_cast<unsigned>(shown_height),
                   static_cast<unsigned>(scale));
}

void MediaModule::update() {
    if (!allocate_work_buffers()) {
        set_status("Media buffers could not be allocated; controls are disabled.");
        return;
    }
    // ui_shell refreshes the active module once per second. Rebinding an
    // already-visible translucent sheet invalidates the whole modal on this
    // display port and produces a visible flash. popup_cb() refreshes the
    // data while the sheet is still hidden, so the modal opens current and
    // remains visually stable until the user closes it.
    if (popup_ && !lv_obj_has_flag(popup_, LV_OBJ_FLAG_HIDDEN)) return;

    memset(media_cache_, 0,
           HA_MAX_MEDIA_PLAYERS * sizeof(HomeAssistantMediaSnapshot));
    const size_t count = home_assistant_get_media_players(
        media_cache_, HA_MAX_MEDIA_PLAYERS);

    const PanelConfig &panel_config = config_service_get();
    for (size_t i = 0; i < PANEL_MAX_MEDIA_SHORTCUTS; ++i) {
        FavoriteControl &control = shortcuts_[i];
        if (i < panel_config.media_shortcut_count) {
            const PanelMediaShortcut &configured = panel_config.media_shortcuts[i];
            control.bound = true;
            memset(&control.favorite, 0, sizeof(control.favorite));
            snprintf(control.favorite.entity_id, sizeof(control.favorite.entity_id),
                     "%s", configured.entity_id);
            snprintf(control.favorite.title, sizeof(control.favorite.title),
                     "%s", configured.label);
            snprintf(control.favorite.media_content_id,
                     sizeof(control.favorite.media_content_id),
                     "%s", configured.media_content_id);
            snprintf(control.favorite.media_content_type,
                     sizeof(control.favorite.media_content_type),
                     "%s", configured.media_content_type);
            set_media_label_text(control.label, configured.label);
            lv_label_set_text(control.icon_label,
                              shortcut_icon(configured.icon[0] ? configured.icon : "music"));

            bool target_available = false;
            for (size_t player = 0; player < count; ++player) {
                if (same_text(media_cache_[player].entity_id, configured.entity_id)) {
                    target_available = media_cache_[player].available;
                    break;
                }
            }
            set_enabled(control.button, target_available);
            lv_obj_remove_flag(control.button, LV_OBJ_FLAG_HIDDEN);
        } else {
            control.bound = false;
            memset(&control.favorite, 0, sizeof(control.favorite));
            lv_label_set_text(control.label, "--");
            set_enabled(control.button, false);
            lv_obj_add_flag(control.button, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (panel_config.media_shortcut_count) lv_obj_add_flag(shortcuts_empty_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(shortcuts_empty_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(popup_empty_[0], "No players discovered. Assign a media player to this Home Assistant area.");
    if (count) lv_obj_add_flag(popup_empty_[0], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(popup_empty_[0], LV_OBJ_FLAG_HIDDEN);

    if (count == 0) {
        selected_entity_id_[0] = '\0';
        requested_picture_[0] = '\0';
        requested_media_fingerprint_ = 0;
        artwork_refresh_pending_ = false;
        clear_artwork();
        lv_label_set_text(player_name_, "No players in this room");
        lv_label_set_text(track_label_, "Nothing playing");
        lv_label_set_text(artist_label_, "Assign a media player to the configured Home Assistant area.");
        lv_label_set_text(album_label_, "");
        lv_label_set_text(state_label_, "Waiting for area discovery");
        set_enabled(play_button_, false);
        set_enabled(prev_button_, false);
        set_enabled(next_button_, false);
        set_enabled(volume_slider_, false);
        set_enabled(volume_down_button_, false);
        set_enabled(volume_up_button_, false);
        set_enabled(mute_button_, false);
        volume_dragging_ = false;
        volume_entity_id_[0] = 0;
        lv_slider_set_value(volume_slider_, 0, LV_ANIM_OFF);
        lv_label_set_text(volume_label_, "--");
        lv_label_set_text(play_label_, "Play");
        lv_label_set_text(play_icon_, LV_SYMBOL_PLAY);
        lv_label_set_text(mute_label_, "Mute");
        lv_obj_set_style_bg_color(mute_button_, lv_color_hex(CARD_ALT), LV_PART_MAIN);
        for (auto &p : players_) { p.bound = false; set_enabled(p.button, false); lv_obj_add_flag(p.button, LV_OBJ_FLAG_HIDDEN); }
        for (auto &s : sources_) { s.bound = false; set_enabled(s.button, false); lv_obj_add_flag(s.button, LV_OBJ_FLAG_HIDDEN); }
        for (auto &f : favorites_) { f.bound = false; set_enabled(f.button, false); lv_obj_add_flag(f.button, LV_OBJ_FLAG_HIDDEN); }
        for (int i = 1; i < 3; ++i) {
            lv_label_set_text(popup_empty_[i], "Choose a player when discovery is complete.");
            lv_obj_remove_flag(popup_empty_[i], LV_OBJ_FLAG_HIDDEN);
        }
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
            set_media_label_text(control.label,
                                 media_cache_[i].name[0]
                                     ? media_cache_[i].name
                                     : media_cache_[i].entity_id);
            set_enabled(control.button, media_cache_[i].available);
            lv_obj_remove_flag(control.button, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(control.button,
                                      lv_color_hex(i == selected ? 0x183C50 : CARD_ALT), LV_PART_MAIN);
            lv_obj_set_style_border_color(control.button,
                                          lv_color_hex(i == selected ? 0x38BDF8 : BORDER), LV_PART_MAIN);
        } else {
            control.bound = false;
            control.entity_id[0] = '\0';
            lv_obj_add_flag(control.button, LV_OBJ_FLAG_HIDDEN);
            set_enabled(control.button, false);
            lv_obj_set_style_bg_color(control.button, lv_color_hex(CARD_ALT), LV_PART_MAIN);
        }
    }

    const HomeAssistantMediaSnapshot &active = media_cache_[selected];
    MediaPlayerViewModel view = {};
    ui_state_model_media_player(active, view);
    set_media_label_text(player_name_, view.player_name);
    set_media_label_text(track_label_, view.title);
    set_media_label_text(artist_label_, view.artist);
    set_media_label_text(album_label_, view.album);
    set_media_label_text(state_label_, view.state_text);

    set_enabled(play_button_, view.available);
    set_enabled(prev_button_, view.available);
    set_enabled(next_button_, view.available);
    lv_label_set_text(play_label_, view.playing ? "Pause" : "Play");
    lv_label_set_text(play_icon_, view.playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    lv_obj_set_style_bg_color(play_button_, lv_color_hex(ACCENT), LV_PART_MAIN);

    set_enabled(volume_slider_, view.available && view.supports_volume);
    set_enabled(volume_down_button_, view.available && view.supports_volume);
    set_enabled(volume_up_button_, view.available && view.supports_volume);
    if (!view.available || !view.supports_volume) volume_dragging_ = false;
    if (!volume_dragging_) {
        lv_slider_set_value(volume_slider_, view.available && view.supports_volume ? view.volume_pct : 0, LV_ANIM_OFF);
        char volume[20];
        if (view.available && view.supports_volume) snprintf(volume, sizeof(volume), "%u%%", static_cast<unsigned>(view.volume_pct));
        else snprintf(volume, sizeof(volume), "--");
        lv_label_set_text(volume_label_, volume);
    }

    set_enabled(mute_button_, view.available && view.supports_mute);
    lv_label_set_text(mute_label_, view.muted ? "Unmute" : "Mute");
    lv_label_set_text(mute_icon_, view.muted ? LV_SYMBOL_VOLUME_MAX : LV_SYMBOL_MUTE);
    lv_obj_set_style_bg_color(mute_button_, lv_color_hex(view.muted ? ACCENT : CARD_ALT), LV_PART_MAIN);

    for (size_t i = 0; i < HA_MAX_MEDIA_SOURCES; ++i) {
        SourceControl &control = sources_[i];
        if (i < active.source_count) {
            control.bound = true;
            snprintf(control.source, sizeof(control.source), "%s", active.sources[i]);
            set_media_label_text(control.label, control.source);
            set_enabled(control.button, active.available);
            lv_obj_remove_flag(control.button, LV_OBJ_FLAG_HIDDEN);
            const bool selected_source = same_text(active.source, control.source);
            lv_obj_set_style_bg_color(control.button, lv_color_hex(selected_source ? 0x183C50 : CARD_ALT), LV_PART_MAIN);
            lv_obj_set_style_border_color(control.button, lv_color_hex(selected_source ? 0x38BDF8 : BORDER), LV_PART_MAIN);
        } else {
            control.bound = false;
            control.source[0] = '\0';
            lv_obj_add_flag(control.button, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(control.label, "--");
            set_enabled(control.button, false);
            lv_obj_set_style_bg_color(control.button, lv_color_hex(CARD_ALT), LV_PART_MAIN);
        }
    }

    lv_label_set_text(popup_empty_[1], "This player does not expose selectable sources.");
    if (active.source_count) lv_obj_add_flag(popup_empty_[1], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(popup_empty_[1], LV_OBJ_FLAG_HIDDEN);

    memset(favorite_cache_, 0,
           HA_MAX_MEDIA_FAVORITES * sizeof(HomeAssistantMediaFavorite));
    const size_t discovered_favorite_count = home_assistant_get_media_favorites(
        favorite_cache_, HA_MAX_MEDIA_FAVORITES);
    size_t favorite_slot = 0;
    for (size_t configured_index = 0;
         configured_index < panel_config.media_favorite_count &&
         favorite_slot < PANEL_MAX_MEDIA_FAVORITES;
         ++configured_index, ++favorite_slot) {
            FavoriteControl &control = favorites_[favorite_slot];
            const PanelMediaShortcut &configured =
                panel_config.media_favorites[configured_index];
            control.bound = true;
            memset(&control.favorite, 0, sizeof(control.favorite));
            snprintf(control.favorite.entity_id, sizeof(control.favorite.entity_id), "%s", configured.entity_id);
            snprintf(control.favorite.title, sizeof(control.favorite.title), "%s", configured.label);
            snprintf(control.favorite.media_content_id, sizeof(control.favorite.media_content_id), "%s", configured.media_content_id);
            snprintf(control.favorite.media_content_type, sizeof(control.favorite.media_content_type), "%s", configured.media_content_type);
            bool target_available = false;
            for (size_t player = 0; player < count; ++player)
                if (same_text(media_cache_[player].entity_id, configured.entity_id)) {
                    target_available = media_cache_[player].available; break;
                }
            set_media_label_text(control.label, configured.label);
            lv_label_set_text(control.icon_label,
                              shortcut_icon(configured.icon[0] ? configured.icon : "star"));
            set_enabled(control.button, target_available);
            lv_obj_remove_flag(control.button, LV_OBJ_FLAG_HIDDEN);
    }
    for (size_t discovered_index = 0;
         discovered_index < discovered_favorite_count && favorite_slot < PANEL_MAX_MEDIA_FAVORITES;
         ++discovered_index) {
        if (!same_text(favorite_cache_[discovered_index].entity_id, selected_entity_id_)) continue;
        FavoriteControl &control = favorites_[favorite_slot++];
            control.bound = true;
            control.favorite = favorite_cache_[discovered_index];
            set_media_label_text(control.label, control.favorite.title);
            lv_label_set_text(control.icon_label, LV_SYMBOL_OK);
            set_enabled(control.button, active.available);
            lv_obj_remove_flag(control.button, LV_OBJ_FLAG_HIDDEN);
    }
    for (; favorite_slot < PANEL_MAX_MEDIA_FAVORITES; ++favorite_slot) {
        FavoriteControl &control = favorites_[favorite_slot];
        control.bound = false;
        memset(&control.favorite, 0, sizeof(control.favorite));
        lv_obj_add_flag(control.button, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(control.label, favorite_slot == 0 ? "No browse items" : "--");
        set_enabled(control.button, false);
    }

    bool has_favorites = false;
    for (const auto &favorite : favorites_) if (favorite.bound) has_favorites = true;
    lv_label_set_text(popup_empty_[2], "No browse favorites available. Add saved Browse favorites in the Media section of the web manager.");
    if (has_favorites) lv_obj_add_flag(popup_empty_[2], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(popup_empty_[2], LV_OBJ_FLAG_HIDDEN);

    if (active.entity_picture[0]) {
        const bool picture_changed = !same_text(requested_picture_, active.entity_picture);
        const uint32_t fingerprint = media_fingerprint(active);
        const bool media_changed = requested_media_fingerprint_ != fingerprint;
        if (media_changed) {
            requested_media_fingerprint_ = fingerprint;
            artwork_request_ms_ = 0;
            artwork_refresh_pending_ = true;
        }
        if (picture_changed) {
            snprintf(requested_picture_, sizeof(requested_picture_), "%s", active.entity_picture);
            artwork_request_ms_ = 0;
            // Never display a previous track's cover while the new one loads.
            clear_artwork();
        }

        HomeAssistantMediaArtworkInfo cached = {};
        home_assistant_get_media_artwork_info(cached);
        const bool cache_matches = cached.generation &&
                                   same_text(cached.entity_id, active.entity_id) &&
                                   same_text(cached.picture_url, requested_picture_) &&
                                   cached.data_size > 0;
        const uint32_t now = millis();
        // A request can be accepted by the worker but fail later (expired proxy
        // URL, transient Wi-Fi, or artwork decode response). Retrying every few
        // seconds creates a continuous cycle of TLS handshakes on ESP-Hosted.
        // A changed cover remains immediate; an unchanged failed cover waits.
        if ((!cache_matches || artwork_refresh_pending_) &&
            (!artwork_request_ms_ || now - artwork_request_ms_ >= HA_MEDIA_ARTWORK_RETRY_MS)) {
            if (home_assistant_request_media_artwork(active.entity_id)) {
                artwork_request_ms_ = now;
            } else {
                artwork_request_ms_ = now;
                Serial0.printf("[Media] Artwork retry deferred for %s\n", active.entity_id);
            }
        }
    } else {
        requested_picture_[0] = '\0';
        requested_media_fingerprint_ = 0;
        artwork_request_ms_ = 0;
        artwork_refresh_pending_ = false;
        clear_artwork();
    }
    refresh_artwork();

    HomeAssistantDiscoveryStatus discovery = {};
    home_assistant_get_discovery_status(discovery);
    if (discovery.last_action_ms && discovery.last_action[0]) set_status(discovery.last_action);
    else set_status("Connected to Home Assistant");
}

void MediaModule::player_cb(lv_event_t *e) {
    auto *control = static_cast<PlayerControl *>(lv_event_get_user_data(e));
    if (!control || !control->owner || !control->bound || !control->entity_id[0]) return;
    control->owner->select_player(control->entity_id);
    lv_obj_add_flag(control->owner->popup_, LV_OBJ_FLAG_HIDDEN);
    control->owner->update();
}

void MediaModule::play_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (!self || !self->selected_entity_id_[0]) return;
    self->set_status(home_assistant_queue_media_play_pause(self->selected_entity_id_)
                         ? "Play/pause queued; waiting for Home Assistant."
                         : media_command_unavailable_message());
}

void MediaModule::previous_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (!self || !self->selected_entity_id_[0]) return;
    self->set_status(home_assistant_queue_media_previous(self->selected_entity_id_)
                         ? "Previous track queued."
                         : media_command_unavailable_message());
}

void MediaModule::next_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (!self || !self->selected_entity_id_[0]) return;
    self->set_status(home_assistant_queue_media_next(self->selected_entity_id_)
                         ? "Next track queued."
                         : media_command_unavailable_message());
}

void MediaModule::volume_pressed_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (self) {
        self->volume_dragging_ = true;
        snprintf(self->volume_entity_id_, sizeof(self->volume_entity_id_), "%s", self->selected_entity_id_);
    }
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
    if (!self || !self->volume_dragging_) return;
    self->volume_dragging_ = false;
    if (!self->selected_entity_id_[0] || !same_text(self->selected_entity_id_, self->volume_entity_id_)) return;
    const int value = static_cast<int>(
        lv_slider_get_value(static_cast<lv_obj_t *>(lv_event_get_target(e))));
    self->set_status(home_assistant_queue_media_volume(
                         self->selected_entity_id_, static_cast<uint8_t>(constrain(value, 0, 100)))
                         ? "Volume queued; waiting for Home Assistant."
                         : media_command_unavailable_message());
}

void MediaModule::volume_down_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (!self || !self->selected_entity_id_[0]) return;
    self->set_status(home_assistant_queue_media_volume_step(
                         self->selected_entity_id_, false)
                         ? "Volume down queued; waiting for Home Assistant."
                         : media_command_unavailable_message());
}

void MediaModule::volume_up_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (!self || !self->selected_entity_id_[0]) return;
    self->set_status(home_assistant_queue_media_volume_step(
                         self->selected_entity_id_, true)
                         ? "Volume up queued; waiting for Home Assistant."
                         : media_command_unavailable_message());
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
                         : media_command_unavailable_message());
}

void MediaModule::source_cb(lv_event_t *e) {
    auto *control = static_cast<SourceControl *>(lv_event_get_user_data(e));
    if (!control || !control->owner || !control->bound || !control->source[0] ||
        !control->owner->selected_entity_id_[0]) return;
    control->owner->set_status(
        home_assistant_queue_media_source(control->owner->selected_entity_id_, control->source)
            ? "Source change queued; waiting for Home Assistant."
            : media_command_unavailable_message());
}

void MediaModule::favorite_cb(lv_event_t *e) {
    auto *control = static_cast<FavoriteControl *>(lv_event_get_user_data(e));
    if (!control || !control->owner || !control->bound) return;
    control->owner->set_status(home_assistant_queue_media_favorite(control->favorite)
                                   ? "Favorite/playlist queued; waiting for Home Assistant."
                                   : media_command_unavailable_message());
}

void MediaModule::volume_cancel_cb(lv_event_t *e) {
    auto *self = static_cast<MediaModule *>(lv_event_get_user_data(e));
    if (!self) return;
    self->volume_dragging_ = false;
    self->volume_entity_id_[0] = 0;
    self->update();
}

