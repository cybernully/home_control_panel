#pragma once

#include "app_config.h"
#include <stddef.h>
#include <stdint.h>

enum class UiControlKind : uint8_t {
    Unknown = 0,
    Light,
    Switch,
    Fan,
    Cover,
    Scene,
};

struct RoomControlViewModel {
    char entity_id[96];
    char title[64];
    char state_text[48];
    UiControlKind kind;
    uint8_t level_pct;
    bool available;
    bool active;
    bool supports_level;
    bool favorite;
};

struct RoomViewModel {
    uint8_t active_room;
    uint8_t room_count;
    char room_name[PANEL_ROOM_NAME_LEN];
    char room_heading[PANEL_ROOM_NAME_LEN];
    char temperature[16];
    char humidity[16];
    char system_status[40];
    char system_detail[128];
    uint16_t group_total[4];
    uint16_t group_active[4];
    uint16_t devices_online;
    uint16_t device_count;
    uint8_t favorite_count;
    RoomControlViewModel favorites[4];
    bool healthy;
    bool busy;
};

// Builds a UI-neutral snapshot from configuration and Home Assistant state.
// No LVGL objects are referenced here; callers own the output buffers.
bool ui_state_model_snapshot_room(RoomViewModel &room,
                                  RoomControlViewModel *controls,
                                  size_t control_capacity,
                                  size_t &control_count);
uint8_t ui_state_model_active_room();
void ui_state_model_set_active_room(uint8_t room_index);
void ui_state_model_next_room();

// UI intents are translated to the existing bounded HA action queue here.
bool ui_state_model_activate(const RoomControlViewModel &control);
bool ui_state_model_set_level(const RoomControlViewModel &control, uint8_t level_pct);

