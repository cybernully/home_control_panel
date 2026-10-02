#pragma once

#include "app_config.h"
#include <stddef.h>
#include <stdint.h>

struct HomeAssistantMediaSnapshot;

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

struct RoomStatusViewModel {
    char label[24];
    char value[64];
    char icon[20];
    char entity_id[96];
    char color[12];
    bool available;
    bool active;
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
    RoomStatusViewModel status_slots[PANEL_ROOM_STATUS_SLOTS];
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

struct OverviewCardViewModel {
    char type[16];
    char entity_id[96];
    char action_entity_id[96];
    char title[PANEL_OVERVIEW_LABEL_LEN];
    char state_text[64];
    char icon[20];
    char color[12];
    char action[16];
    uint8_t span;
    bool available;
    bool active;
    bool actionable;
    bool confirm;
};

// Builds the complete Overview presentation state without creating widgets.
size_t ui_state_model_snapshot_overview(OverviewCardViewModel *cards, size_t capacity);
bool ui_state_model_activate_overview(const OverviewCardViewModel &card);

struct WeatherForecastViewModel {
    char period[16];
    char condition[32];
    char temperature[24];
    char detail[32];
};

struct WeatherViewModel {
    char entity_name[64];
    char condition[32];
    char temperature[24];
    char detail[96];
    bool available;
    bool loading;
    uint8_t hourly_count;
    uint8_t daily_count;
    WeatherForecastViewModel hourly[HA_MAX_WEATHER_HOURLY];
    WeatherForecastViewModel daily[HA_MAX_WEATHER_DAILY];
};

bool ui_state_model_snapshot_weather(WeatherViewModel &weather);

struct CalendarDayViewModel {
    char weekday[8];
    char date[8];
    uint8_t event_count;
    bool today;
    bool selected;
};

struct CalendarEventViewModel {
    char calendar[40];
    char color[12];
    char title[120];
    char time[40];
    char location[120];
    char description[512];
    char date_range[80];
};

struct CalendarViewModel {
    char week_label[48];
    char selected_day_label[48];
    char status[64];
    bool loading;
    bool available;
    uint8_t day_count;
    CalendarDayViewModel days[7];
    uint8_t event_count;
    CalendarEventViewModel events[8];
};

// period_offset is relative to the current configured 1, 3, or 7-day period.
// selected_day is an index within that period. The model owns date math and HA
// range requests; LVGL only renders.
bool ui_state_model_snapshot_calendar(int16_t period_offset, uint8_t selected_day,
                                      CalendarViewModel &calendar);

struct MediaPlayerViewModel {
    char entity_id[96];
    char player_name[64];
    char title[128];
    char artist[128];
    char album[128];
    char state_text[160];
    char artwork_url[256];
    uint8_t volume_pct;
    bool available;
    bool playing;
    bool muted;
    bool supports_volume;
    bool supports_mute;
};

// Presentation-only media state. Transport, JSON and LVGL stay outside this model.
void ui_state_model_media_player(const HomeAssistantMediaSnapshot &source,
                                 MediaPlayerViewModel &out);

