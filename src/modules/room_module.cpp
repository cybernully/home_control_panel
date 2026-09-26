#include "room_module.h"
#include "config_service.h"
#include "display_text.h"
#include "module_ui.h"
#include <Arduino.h>
#include <algorithm>
#include <string.h>
using namespace module_ui;

namespace {
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
        if (strcmp(cfg.room_controls[i].entity_id, id) == 0) return &cfg.room_controls[i];
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
void display(lv_obj_t *obj, const char *text) {
    char safe[256]; panel_display_text(safe, sizeof(safe), text);
    lv_label_set_text(obj, safe);
}
void ellipsis(lv_obj_t *obj, int width) {
    lv_obj_set_width(obj, width);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
}
}

void RoomModule::make_tile(Tile &t, lv_obj_t *parent, int x, int y, int w, int h) {
    t.owner = this;
    t.root = button(parent, "", x, y, w, h, CARD);
    lv_obj_set_style_radius(t.root, 22, LV_PART_MAIN);
    t.name = lv_obj_get_child(t.root, 0);
    lv_obj_set_style_text_font(t.name, &lv_font_montserrat_20, LV_PART_MAIN);
    ellipsis(t.name, w - 40);
    lv_obj_align(t.name, LV_ALIGN_TOP_LEFT, 20, 20);
    t.detail = label(t.root, "", &lv_font_montserrat_14, MUTED);
    ellipsis(t.detail, w - 40);
    lv_obj_align(t.detail, LV_ALIGN_BOTTOM_LEFT, 20, -20);
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
    }
    lv_obj_add_flag(t.root, LV_OBJ_FLAG_HIDDEN);
}

void RoomModule::create(lv_obj_t *parent) {
    box(parent, BG, 0, 0);
    heading_ = module_ui::title(parent, "Your room", "Favorites within reach. Tap a group to explore.");
    ellipsis(heading_, 1000);
    auto *caption = label(parent, "FAVORITES", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(caption, 24, 92);
    for (int i = 0; i < 6; ++i)
        make_tile(favorites_[i], parent, 24 + (i % 3) * 416, 120 + (i / 3) * 142, 400, 126);
    empty_ = card(parent, 24, 120, 1232, 268);
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
    const bool active = strcmp(e->state,"on")==0 || strcmp(e->state,"open")==0 || strcmp(e->state,"opening")==0;
    char detail[112];
    if (!e->available) snprintf(detail, sizeof(detail), "Unavailable");
    else if (t.scene) snprintf(detail, sizeof(detail), "Scene  |  Tap to activate");
    else if (strcmp(e->domain,"cover")==0) snprintf(detail,sizeof(detail),"%s  |  Tap to %s",e->state,active?"close":"open");
    else if (e->supports_brightness && active) snprintf(detail,sizeof(detail),"On at %u%%  |  Tap to turn off",e->brightness_pct);
    else snprintf(detail,sizeof(detail),"%s  |  Tap to turn %s",active?"On":"Off",active?"off":"on");
    const bool dimmer = t.slider && e->supports_brightness && strcmp(e->domain, "light") == 0;
    if (t.slider) {
        if (dimmer) {
            lv_obj_remove_flag(t.slider, LV_OBJ_FLAG_HIDDEN);
            lv_slider_set_value(t.slider, active ? e->brightness_pct : 0, LV_ANIM_OFF);
            set_enabled(t.slider, e->available);
            snprintf(detail,sizeof(detail), e->available ? "Brightness  %u%%" : "Unavailable", active ? e->brightness_pct : 0);
        } else lv_obj_add_flag(t.slider, LV_OBJ_FLAG_HIDDEN);
        ellipsis(t.detail, dimmer ? 208 : 444);
    }
    display(t.detail, detail);
    set_enabled(t.root, e->available);
    lv_obj_set_style_bg_color(t.root, lv_color_hex(active ? 0x183C50 : CARD_ALT), LV_PART_MAIN);
    lv_obj_set_style_border_color(t.root, lv_color_hex(active ? 0x38BDF8 : BORDER), LV_PART_MAIN);
    lv_obj_remove_flag(t.root, LV_OBJ_FLAG_HIDDEN);
}

void RoomModule::update() {
    count_ = home_assistant_get_room_entities(entities_, HA_MAX_AREA_ENTITIES);
    std::sort(entities_, entities_ + count_, [](const HomeAssistantEntitySnapshot &a, const HomeAssistantEntitySnapshot &b) {
        const int ao = order(a.entity_id), bo = order(b.entity_id);
        if (ao != bo) return ao < bo;
        const int name = strcmp(a.name, b.name);
        return name ? name < 0 : strcmp(a.entity_id, b.entity_id) < 0;
    });
    const auto &cfg = config_service_get();
    size_t favorite_count = 0;
    // A missing favorite retains its position instead of becoming another device.
    for (size_t i = 0; i < cfg.room_control_count && favorite_count < 6; ++i) {
        const auto &p = cfg.room_controls[i];
        if (p.placement != 1) continue;
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
    display(heading_, discovery.area_name[0] ? discovery.area_name : cfg.area_id[0] ? cfg.area_id : "Your room");
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
    display(self->status_,queued ? "Command queued. Waiting for Home Assistant." : "Command queue busy. Please try again.");
    self->feedback_until_ = millis()+4000;
    // Feedback remains visible above the popup's pagination controls.
    if (self->group_ >= 0) display(self->page_label_,queued ? "Command queued" : "Queue busy - try again");
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
        display(t->owner->page_label_,queued ? "Brightness queued" : "Queue busy - try again");
        t->owner->feedback_until_ = millis()+4000;
        display(t->owner->status_,queued ? "Brightness queued. Waiting for Home Assistant." : "Command queue busy. Please try again.");
    }
}
