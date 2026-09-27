#include "room_module.h"
#include "config_service.h"
#include "display_text.h"
#include "ha_icons_font.h"
#include "module_ui.h"
#include <Arduino.h>
#include <algorithm>
#include <string.h>
using namespace module_ui;

namespace {
uint8_t g_active_room = 0;
const char *GROUP_NAMES[] = {"Lights", "Devices", "Shades", "Scenes"};
int entity_group(const char *domain) {
    if (strcmp(domain, "light") == 0) return 0;
    if (strcmp(domain, "cover") == 0) return 2;
    if (strcmp(domain, "scene") == 0) return 3;
    return 1;
}
const PanelRoomControl *preference(const char *id) {
    const auto &cfg = config_service_get();
    for (size_t i = 0; i < cfg.room_control_count; ++i)
        if (cfg.room_controls[i].room_index == g_active_room && strcmp(cfg.room_controls[i].entity_id, id) == 0) return &cfg.room_controls[i];
    return nullptr;
}
int order(const char *id) {
    const auto *pref = preference(id);
    return pref ? static_cast<int>(pref - config_service_get().room_controls) : HA_MAX_AREA_ENTITIES;
}
bool hidden(const char *id) {
    const auto *p = preference(id);
    return p && p->placement == 2;
}
bool contains_ci(const char *text, const char *needle) {
    if (!text || !needle || !needle[0]) return false;
    for (const char *start = text; *start; ++start) {
        const char *a = start;
        const char *b = needle;
        while (*a && *b) {
            char left = *a, right = *b;
            if (left >= 'A' && left <= 'Z') left = static_cast<char>(left - 'A' + 'a');
            if (right >= 'A' && right <= 'Z') right = static_cast<char>(right - 'A' + 'a');
            if (left != right) break;
            ++a; ++b;
        }
        if (!*b) return true;
    }
    return false;
}
const char *display_type(const HomeAssistantEntitySnapshot &entity) {
    const auto *p = preference(entity.entity_id);
    if (p && p->device_type[0] && strcmp(p->device_type, "auto") != 0) return p->device_type;
    if (strcmp(entity.domain, "switch") != 0) return entity.domain;

    // HA exposes many RF/legacy devices as switches. Make the default useful
    // without requiring IDs or manual type selection for obvious names. A web
    // override above always wins for ambiguous devices.
    const char *name = p && p->label[0] ? p->label : entity.name;
    if (contains_ci(name, "light") || contains_ci(name, "lamp")) return "light";
    if (contains_ci(name, "fan")) return "fan";
    if (contains_ci(name, "shade") || contains_ci(name, "blind") ||
        contains_ci(name, "curtain")) return "cover";
    return entity.domain;
}
void display(lv_obj_t *obj, const char *text) {
    char safe[256]; panel_display_text(safe, sizeof(safe), text);
    lv_label_set_text(obj, safe);
}
void ellipsis(lv_obj_t *obj, int width) {
    lv_obj_set_width(obj, width);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
}
void set_icon_glyph(lv_obj_t *label_obj, uint32_t codepoint) {
    // MDI glyphs live in Unicode's private-use area.  Encode explicitly so
    // the source remains portable regardless of the compiler file encoding.
    char text[5] = {};
    text[0] = static_cast<char>(0xF0 | (codepoint >> 18));
    text[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
    text[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    text[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
    lv_label_set_text(label_obj, text);
}
}

void RoomModule::make_tile(Tile &t, lv_obj_t *parent, int x, int y, int w, int h) {
    t.owner = this;
    t.root = button(parent, "", x, y, w, h, CARD);
    lv_obj_set_style_radius(t.root, 22, LV_PART_MAIN);
    t.name = lv_obj_get_child(t.root, 0);
    lv_obj_set_style_text_font(t.name, &lv_font_montserrat_20, LV_PART_MAIN);
    ellipsis(t.name, w - 96);
    lv_obj_align(t.name, LV_ALIGN_TOP_LEFT, 78, 20);
    t.detail = label(t.root, "", &lv_font_montserrat_14, MUTED);
    // Reserve the left column for the device glyph.  The old detail position
    // started below the glyph, which made the icon appear over its first words.
    ellipsis(t.detail, w - 96);
    lv_obj_align(t.detail, LV_ALIGN_BOTTOM_LEFT, 78, -20);
    lv_obj_add_event_cb(t.root, action_cb, LV_EVENT_ALL, &t);
    // The wider popup tiles have an independent brightness target.
    if (w == 484) {
        t.slider = lv_slider_create(t.root);
        lv_obj_set_pos(t.slider, 244, 82); lv_obj_set_size(t.slider, 208, 24);
        lv_obj_set_ext_click_area(t.slider, 14);
        lv_slider_set_range(t.slider, 0, 100); style_slider(t.slider);
        lv_obj_set_style_bg_color(t.slider, lv_color_hex(BORDER), LV_PART_MAIN);
        lv_obj_remove_flag(t.slider, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_event_cb(t.slider, brightness_cb, LV_EVENT_ALL, &t);
        lv_obj_add_flag(t.slider, LV_OBJ_FLAG_HIDDEN);
        const char *fan_labels[] = {"Off", "Low", "Med", "High"};
        const uint8_t fan_values[] = {0, 33, 66, 100};
        for (uint8_t i = 0; i < 4; ++i) {
            t.fan_choices[i].tile = &t;
            t.fan_choices[i].percentage = fan_values[i];
            t.fan_choices[i].button = button(t.root, fan_labels[i], 244 + i * 52, 82, 48, 28, CARD_ALT);
            lv_obj_set_style_text_font(lv_obj_get_child(t.fan_choices[i].button, 0), &lv_font_montserrat_12, LV_PART_MAIN);
            lv_obj_remove_flag(t.fan_choices[i].button, LV_OBJ_FLAG_EVENT_BUBBLE);
            lv_obj_add_event_cb(t.fan_choices[i].button, fan_speed_cb, LV_EVENT_CLICKED, &t.fan_choices[i]);
            lv_obj_add_flag(t.fan_choices[i].button, LV_OBJ_FLAG_HIDDEN);
        }
    }
    // A label backed by Home Assistant's MDI icon family is decorative only.
    // It deliberately has no click target, leaving the entire tile reliable
    // for touch actions.
    t.icon = label(t.root, "", &ha_icons_font, MUTED);
    lv_obj_set_size(t.icon, 44, 44); lv_obj_set_pos(t.icon, 18, 15);
    lv_obj_set_style_text_align(t.icon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_remove_flag(t.icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(t.icon, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(t.root, LV_OBJ_FLAG_HIDDEN);
}

void RoomModule::create(lv_obj_t *parent) {
    box(parent, BG, 0, 0);
    const PanelConfig &cfg = config_service_get();
    heading_ = module_ui::title(parent, "Your room", "Favorites within reach. Tap a group to explore.");
    ellipsis(heading_, 1000);
    for (uint8_t i = 0; i < cfg.room_count && i < PANEL_MAX_ROOMS; ++i) { room_tabs_[i].owner=this; room_tabs_[i].index=i; room_tabs_[i].button=button(parent,cfg.rooms[i].tab_label,24+i*210,92,196,42,CARD_ALT); lv_obj_add_event_cb(room_tabs_[i].button,room_tab_cb,LV_EVENT_CLICKED,&room_tabs_[i]); }
    auto *caption = label(parent, "FAVORITES", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(caption, 24, 142);
    for (int i = 0; i < 6; ++i)
        make_tile(favorites_[i], parent, 24 + (i % 3) * 416, 170 + (i / 3) * 142, 400, 126);
    empty_ = card(parent, 24, 170, 1232, 238);
    auto *text = label(empty_, "Make this room yours", &lv_font_montserrat_24, TEXT);
    lv_obj_set_pos(text, 28, 60);
    text = label(empty_, "Open a group below to control your room.\nChoose up to six favorites, rename, reorder or hide controls in the web manager.", &lv_font_montserrat_18, MUTED);
    lv_obj_set_pos(text, 28, 110);
    lv_obj_set_width(text, 1150);
    caption = label(parent, "EXPLORE ROOM", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(caption, 24, 422);
    for (int i = 0; i < 4; ++i) {
        auto &g = groups_[i]; g.owner = this; g.index = i;
        g.button = button(parent, GROUP_NAMES[i], 24 + i * 312, 452, 296, 90);
        lv_obj_set_style_radius(g.button, 45, LV_PART_MAIN);
        g.text = lv_obj_get_child(g.button, 0);
        lv_obj_set_pos(g.text, 58, 34);
        // Three small bars form a menu icon without relying on an optional
        // Unicode icon font.
        for (int line = 0; line < 3; ++line) {
            lv_obj_t *menu_line = lv_obj_create(g.button);
            lv_obj_set_size(menu_line, 25, 3); lv_obj_set_pos(menu_line, 22, 29 + line * 9);
            lv_obj_set_style_radius(menu_line, 2, LV_PART_MAIN);
            lv_obj_set_style_bg_color(menu_line, lv_color_hex(TEXT), LV_PART_MAIN);
            lv_obj_set_style_border_width(menu_line, 0, LV_PART_MAIN);
            // The menu bars decorate the button; they must not become an
            // independent touch target above it.
            lv_obj_remove_flag(menu_line, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(menu_line, LV_OBJ_FLAG_EVENT_BUBBLE);
        }
        lv_obj_add_event_cb(g.button, group_cb, LV_EVENT_CLICKED, &g);
    }
    status_ = label(parent, "Connecting to Home Assistant...", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(status_, 24, 577); ellipsis(status_, 1220);

    // A page-owned scrim blocks underlying controls and disappears on navigation.
    overlay_ = card(parent, 0, 0, 1280, 658);
    box(overlay_, 0x030712, 0, 0);
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_80, LV_PART_MAIN);
    lv_obj_add_event_cb(overlay_, close_cb, LV_EVENT_CLICKED, this);
    auto *sheet = card(overlay_, 124, 24, 1032, 610);
    lv_obj_set_style_radius(sheet, 28, LV_PART_MAIN);
    popup_title_ = label(sheet, "", &lv_font_montserrat_24, TEXT);
    lv_obj_set_pos(popup_title_, 24, 22);
    popup_hint_ = label(sheet, "", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(popup_hint_, 180, 30); ellipsis(popup_hint_, 650);
    auto *close = button(sheet, "Close", 866, 14, 142, 56);
    lv_obj_add_event_cb(close, close_cb, LV_EVENT_CLICKED, this);
    for (int i = 0; i < 6; ++i)
        make_tile(popup_tiles_[i], sheet, 24 + (i % 2) * 500, 90 + (i / 2) * 144, 484, 128);
    previous_ = button(sheet, "Previous", 24, 534, 170, 56);
    next_ = button(sheet, "Next", 838, 534, 170, 56);
    lv_obj_add_event_cb(previous_, page_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(next_, page_cb, LV_EVENT_CLICKED, this);
    page_label_ = label(sheet, "", &lv_font_montserrat_16, MUTED);
    lv_obj_align(page_label_, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
}

void RoomModule::bind(Tile &t, const HomeAssistantEntitySnapshot *e) {
    if (!e) {
        t.dragging = false; t.pressed = false; t.entity_id[0] = 0;
        lv_obj_add_flag(t.root, LV_OBJ_FLAG_HIDDEN); return;
    }
    if (hidden(t.entity_id)) { t.dragging = false; t.pressed = false; }
    if (t.dragging || t.pressed) return; // Keep identity and thumb stable until release.
    snprintf(t.entity_id, sizeof(t.entity_id), "%s", e->entity_id);
    t.scene = strcmp(e->domain, "scene") == 0;
    const auto *p = preference(e->entity_id);
    display(t.name, p && p->label[0] ? p->label : e->name[0] ? e->name : e->entity_id);
    const char *type = display_type(*e);
    const bool active = strcmp(e->state,"on")==0 || strcmp(e->state,"open")==0 || strcmp(e->state,"opening")==0;
    char detail[112];
    if (!e->available) snprintf(detail, sizeof(detail), "Unavailable");
    else if (t.scene) snprintf(detail, sizeof(detail), "Scene  |  Tap to activate");
    else if (strcmp(type,"cover")==0) snprintf(detail,sizeof(detail),"%s  |  Tap to %s",e->state,active?"close":"open");
    else if (strcmp(type,"fan")==0) snprintf(detail,sizeof(detail),active?"Fan %u%%":"Fan off",e->fan_speed_pct);
    else if (e->supports_brightness && active) snprintf(detail,sizeof(detail),"On at %u%%  |  Tap to turn off",e->brightness_pct);
    else snprintf(detail,sizeof(detail),"%s  |  Tap to turn %s",active?"On":"Off",active?"off":"on");
    const bool dimmer = t.slider && e->supports_brightness && strcmp(type, "light") == 0;
    const bool fan = t.fan_choices[0].button && strcmp(type, "fan") == 0;
    if (t.slider) {
        if (dimmer) {
            lv_obj_remove_flag(t.slider, LV_OBJ_FLAG_HIDDEN);
            lv_slider_set_value(t.slider, active ? e->brightness_pct : 0, LV_ANIM_OFF);
            set_enabled(t.slider, e->available);
            snprintf(detail,sizeof(detail), e->available ? "Brightness  %u%%" : "Unavailable", active ? e->brightness_pct : 0);
        } else lv_obj_add_flag(t.slider, LV_OBJ_FLAG_HIDDEN);
        for (uint8_t i = 0; i < 4; ++i) {
            if (fan) {
                lv_obj_remove_flag(t.fan_choices[i].button, LV_OBJ_FLAG_HIDDEN);
                const bool selected = (i == 0 && !active) ||
                    (i > 0 && active && e->fan_speed_pct >= t.fan_choices[i].percentage - 16 && e->fan_speed_pct <= t.fan_choices[i].percentage + 17);
                lv_obj_set_style_bg_color(t.fan_choices[i].button, lv_color_hex(selected ? ACCENT : CARD_ALT), LV_PART_MAIN);
                set_enabled(t.fan_choices[i].button, e->available);
            } else lv_obj_add_flag(t.fan_choices[i].button, LV_OBJ_FLAG_HIDDEN);
        }
        ellipsis(t.detail, (dimmer || fan) ? 208 : 444);
    }
    display(t.detail, detail);
    set_enabled(t.root, e->available);
    lv_obj_set_style_bg_color(t.root, lv_color_hex(active ? 0x183C50 : CARD_ALT), LV_PART_MAIN);
    lv_obj_set_style_border_color(t.root, lv_color_hex(active ? 0x38BDF8 : BORDER), LV_PART_MAIN);
    render_icon(t, type, active, e->available);
    lv_obj_remove_flag(t.root, LV_OBJ_FLAG_HIDDEN);
}

void RoomModule::render_icon(Tile &t, const char *type, bool active, bool available) {
    // Match the Home Assistant icon language: familiar Material Design Icons
    // rather than improvised shapes. Off states use the MDI outline/off
    // variants; active states use their filled counterparts.
    constexpr uint32_t MDI_BLINDS = 0xF00AC;
    constexpr uint32_t MDI_FAN = 0xF0210;
    constexpr uint32_t MDI_LIGHTBULB = 0xF0335;
    constexpr uint32_t MDI_LIGHTBULB_OUTLINE = 0xF0336;
    constexpr uint32_t MDI_PALETTE = 0xF03D8;
    constexpr uint32_t MDI_POWER = 0xF0425;
    constexpr uint32_t MDI_SOCKET_US = 0xF07E9;
    constexpr uint32_t MDI_FAN_OFF = 0xF081D;
    constexpr uint32_t MDI_POWER_OFF = 0xF0902;
    constexpr uint32_t MDI_PALETTE_OUTLINE = 0xF0E0C;
    constexpr uint32_t MDI_LIGHTBULB_OFF = 0xF0E4F;
    constexpr uint32_t MDI_BLINDS_OPEN = 0xF1011;
    constexpr uint32_t MDI_TOOLS = 0xF1064;
    const uint32_t color = !available ? MUTED : active ? 0x38BDF8 : MUTED;
    uint32_t glyph = active ? MDI_POWER : MDI_POWER_OFF;
    if (strcmp(type, "light") == 0)
        glyph = active ? MDI_LIGHTBULB : MDI_LIGHTBULB_OUTLINE;
    else if (strcmp(type, "fan") == 0)
        glyph = active ? MDI_FAN : MDI_FAN_OFF;
    else if (strcmp(type, "cover") == 0)
        glyph = active ? MDI_BLINDS_OPEN : MDI_BLINDS;
    else if (strcmp(type, "scene") == 0)
        glyph = active ? MDI_PALETTE : MDI_PALETTE_OUTLINE;
    else if (strcmp(type, "switch") == 0)
        glyph = active ? MDI_SOCKET_US : MDI_POWER_OFF;
    else if (strcmp(type, "tools") == 0)
        glyph = MDI_TOOLS;
    if (!available && strcmp(type, "light") == 0) glyph = MDI_LIGHTBULB_OFF;
    set_icon_glyph(t.icon, glyph);
    lv_obj_set_style_text_color(t.icon, lv_color_hex(color), LV_PART_MAIN);
}

void RoomModule::update() {
    count_ = home_assistant_get_room_entities(entities_, HA_MAX_AREA_ENTITIES);
    const auto &cfg = config_service_get();
    if (g_active_room >= cfg.room_count) g_active_room = 0;
    if (cfg.room_count) {
        size_t filtered = 0;
        for (size_t i = 0; i < count_; ++i) {
            const PanelRoomControl *p = preference(entities_[i].entity_id);
            if (p) entities_[filtered++] = entities_[i];
        }
        count_ = filtered;
    }
    std::sort(entities_, entities_ + count_, [](const HomeAssistantEntitySnapshot &a, const HomeAssistantEntitySnapshot &b) {
        const int ao = order(a.entity_id), bo = order(b.entity_id);
        if (ao != bo) return ao < bo;
        const int name = strcmp(a.name, b.name);
        return name ? name < 0 : strcmp(a.entity_id, b.entity_id) < 0;
    });
    size_t favorite_count = 0;
    // A missing favorite retains its position instead of becoming another device.
    for (size_t i = 0; i < cfg.room_control_count && favorite_count < 6; ++i) {
        const auto &p = cfg.room_controls[i];
        if (p.room_index != g_active_room || p.placement != 1) continue;
        const HomeAssistantEntitySnapshot *found = nullptr;
        for (size_t j = 0; j < count_; ++j)
            if (strcmp(p.entity_id, entities_[j].entity_id)==0) { found = &entities_[j]; break; }
        HomeAssistantEntitySnapshot missing = {};
        snprintf(missing.entity_id,sizeof(missing.entity_id),"%s",p.entity_id);
        bind(favorites_[favorite_count++], found ? found : &missing);
    }
    for (size_t i = favorite_count; i < 6; ++i) bind(favorites_[i], nullptr);
    if (favorite_count) lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(empty_, LV_OBJ_FLAG_HIDDEN);
    for (int g = 0; g < 4; ++g) {
        size_t total = 0;
        for (size_t i = 0; i < count_; ++i)
            if (!hidden(entities_[i].entity_id) && entity_group(entities_[i].domain)==g) ++total;
        char text[48]; snprintf(text,sizeof(text),"%s   %u",GROUP_NAMES[g],static_cast<unsigned>(total));
        display(groups_[g].text,text); set_enabled(groups_[g].button,total > 0);
    }
    HomeAssistantDiscoveryStatus discovery = {};
    home_assistant_get_discovery_status(discovery);
    display(heading_, cfg.room_count && cfg.rooms[g_active_room].header[0] ? cfg.rooms[g_active_room].header : discovery.area_name[0] ? discovery.area_name : "Your room");
    for (uint8_t i = 0; i < cfg.room_count && i < PANEL_MAX_ROOMS; ++i) if (room_tabs_[i].button) lv_obj_set_style_bg_color(room_tabs_[i].button,lv_color_hex(i==g_active_room?ACCENT:CARD_ALT),LV_PART_MAIN);
    if (!feedback_until_ || static_cast<int32_t>(millis() - feedback_until_) >= 0) {
        if (discovery.last_action_ms && millis() - discovery.last_action_ms < 12000) {
            display(status_, discovery.last_action);
            if (group_ >= 0 && discovery.last_action_http_code >= 400)
                display(page_label_, "Home Assistant rejected the command");
        } else display(status_, discovery.message[0] ? discovery.message : "Waiting for Home Assistant");
    }
    if (group_ >= 0) {
        render_popup();
        if (discovery.last_action_ms && millis() - discovery.last_action_ms < 12000 &&
            (discovery.last_action_http_code < 0 || discovery.last_action_http_code >= 400))
            display(page_label_, "Command failed. Check Home Assistant.");
    }
}

void RoomModule::render_popup() {
    size_t indices[HA_MAX_AREA_ENTITIES], total = 0;
    for (size_t i = 0; i < count_; ++i)
        if (!hidden(entities_[i].entity_id) && entity_group(entities_[i].domain)==group_) indices[total++] = i;
    const int pages = total ? (total + 5) / 6 : 1;
    if (page_ >= pages) page_ = pages - 1;
    display(popup_title_, GROUP_NAMES[group_]);
    display(popup_hint_, group_ == 0 ? "Tap a tile to toggle. Slide to dim." :
                         group_ == 3 ? "Tap a scene to activate." :
                         group_ == 2 ? "Tap a shade to open or close." : "Tap a device to turn it on or off.");
    for (size_t i = 0; i < 6; ++i) {
        size_t offset = page_ * 6 + i;
        bind(popup_tiles_[i], offset < total ? &entities_[indices[offset]] : nullptr);
    }
    char text[64];
    if (total) snprintf(text,sizeof(text),"%d / %d",page_+1,pages);
    else snprintf(text,sizeof(text),"No visible controls");
    if (!feedback_until_ || static_cast<int32_t>(millis() - feedback_until_) >= 0) display(page_label_,text);
    set_enabled(previous_,page_>0); set_enabled(next_,page_+1<pages);
}

void RoomModule::action_cb(lv_event_t *e) {
    auto *t = static_cast<Tile *>(lv_event_get_user_data(e));
    if (!t) return;
    const auto code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) { t->pressed = true; return; }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) { t->pressed = false; return; }
    if (code != LV_EVENT_CLICKED || !t->entity_id[0] || hidden(t->entity_id)) return;
    // Revalidate against fresh HA state; never send an old tile's stale action.
    auto *self = t->owner;
    HomeAssistantEntitySnapshot current = {};
    if (!home_assistant_get_room_entity(t->entity_id, current) || !current.available) {
        self->update(); return;
    }
    const bool queued = t->scene ? home_assistant_queue_scene(t->entity_id) : home_assistant_queue_toggle(t->entity_id);
    display(self->status_,queued ? "Command queued. Waiting for Home Assistant." :
            (home_assistant_commands_ready() ? "Command queue busy. Please try again." :
             "Home Assistant reconnecting. Please wait."));
    self->feedback_until_ = millis()+4000;
    // Feedback remains visible above the popup's pagination controls.
    if (self->group_ >= 0) display(self->page_label_,queued ? "Command queued" :
                                    (home_assistant_commands_ready() ? "Queue busy - try again" : "HA reconnecting"));
}
void RoomModule::group_cb(lv_event_t *e) {
    auto *g = static_cast<Group *>(lv_event_get_user_data(e));
    g->owner->feedback_until_ = 0; g->owner->group_ = g->index; g->owner->page_ = 0;
    g->owner->update(); lv_obj_remove_flag(g->owner->overlay_,LV_OBJ_FLAG_HIDDEN);
}
void RoomModule::close_cb(lv_event_t *e) {
    static_cast<RoomModule *>(lv_event_get_user_data(e))->on_deactivate();
}
void RoomModule::page_cb(lv_event_t *e) {
    auto *self = static_cast<RoomModule *>(lv_event_get_user_data(e));
    for (auto &tile : self->popup_tiles_) tile.dragging = false;
    self->feedback_until_ = 0;
    self->page_ += lv_event_get_target(e)==self->next_ ? 1 : -1;
    if (self->page_ < 0) self->page_ = 0;
    self->render_popup();
}
void RoomModule::room_tab_cb(lv_event_t *e) { auto *tab=static_cast<RoomTab *>(lv_event_get_user_data(e)); if(!tab)return; g_active_room=tab->index; tab->owner->on_deactivate(); tab->owner->update(); }
void RoomModule::on_deactivate() {
    group_ = -1; page_ = 0;
    for (auto &tile : popup_tiles_) { tile.dragging = false; tile.pressed = false; }
    for (auto &tile : favorites_) tile.pressed = false;
    if (overlay_) lv_obj_add_flag(overlay_,LV_OBJ_FLAG_HIDDEN);
}

void RoomModule::brightness_cb(lv_event_t *e) {
    auto *t = static_cast<Tile *>(lv_event_get_user_data(e));
    const auto code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) t->dragging = true;
    else if (code == LV_EVENT_PRESS_LOST) { t->dragging = false; t->owner->update(); }
    else if (code == LV_EVENT_VALUE_CHANGED) {
        char text[32]; snprintf(text,sizeof(text),"Brightness  %d%%",static_cast<int>(lv_slider_get_value(t->slider)));
        display(t->detail,text);
    } else if (code == LV_EVENT_RELEASED && t->dragging) {
        t->dragging = false;
        if (hidden(t->entity_id)) { t->owner->update(); return; }
        const bool queued = home_assistant_queue_light_brightness(t->entity_id,lv_slider_get_value(t->slider));
        display(t->owner->page_label_,queued ? "Brightness queued" :
                (home_assistant_commands_ready() ? "Queue busy - try again" : "HA reconnecting"));
        t->owner->feedback_until_ = millis()+4000;
        display(t->owner->status_,queued ? "Brightness queued. Waiting for Home Assistant." :
                (home_assistant_commands_ready() ? "Command queue busy. Please try again." :
                 "Home Assistant reconnecting. Please wait."));
    }
}

void RoomModule::fan_speed_cb(lv_event_t *e) {
    auto *choice = static_cast<Tile::FanChoice *>(lv_event_get_user_data(e));
    if (!choice || !choice->tile || !choice->tile->owner || !choice->tile->entity_id[0]) return;
    Tile *tile = choice->tile;
    RoomModule *self = tile->owner;
    const bool queued = home_assistant_queue_fan_speed(tile->entity_id, choice->percentage);
    display(self->status_, queued ? "Fan speed queued. Waiting for Home Assistant." :
            (home_assistant_commands_ready() ? "Command queue busy. Please try again." : "Home Assistant reconnecting. Please wait."));
    self->feedback_until_ = millis() + 4000;
}
