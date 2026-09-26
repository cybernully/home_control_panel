#pragma once
#include "app_config.h"
#include "home_assistant.h"
#include "module.h"

class RoomModule final : public PanelModule {
public:
    const char *id() const override { return "room"; }
    const char *title() const override { return "Room"; }
    void create(lv_obj_t *parent) override;
    void update() override;
    void on_deactivate() override;
private:
    struct Tile {
        RoomModule *owner = nullptr;
        lv_obj_t *root = nullptr, *name = nullptr, *detail = nullptr;
        char entity_id[96] = {};
        bool scene = false;
        lv_obj_t *slider = nullptr;
        bool dragging = false, pressed = false;
    };
    struct Group {
        RoomModule *owner = nullptr;
        int index = 0;
        lv_obj_t *button = nullptr, *text = nullptr;
    };
    Tile favorites_[6], popup_tiles_[6];
    Group groups_[4];
    // Persistent storage avoids placing the 48-entity snapshot on loopTask's stack.
    HomeAssistantEntitySnapshot entities_[HA_MAX_AREA_ENTITIES] = {};
    size_t count_ = 0;
    lv_obj_t *heading_ = nullptr, *status_ = nullptr, *empty_ = nullptr;
    lv_obj_t *overlay_ = nullptr, *popup_title_ = nullptr, *popup_hint_ = nullptr, *page_label_ = nullptr;
    lv_obj_t *previous_ = nullptr, *next_ = nullptr;
    int group_ = -1, page_ = 0;
    uint32_t feedback_until_ = 0;
    void make_tile(Tile &tile, lv_obj_t *parent, int x, int y, int w, int h);
    void bind(Tile &tile, const HomeAssistantEntitySnapshot *entity);
    void render_popup();
    static void brightness_cb(lv_event_t *e);
    static void action_cb(lv_event_t *e);
    static void group_cb(lv_event_t *e);
    static void close_cb(lv_event_t *e);
    static void page_cb(lv_event_t *e);
};
