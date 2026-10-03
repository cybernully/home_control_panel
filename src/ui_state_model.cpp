#include "ui_state_model.h"

#include "config_service.h"
#include "home_assistant.h"

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
uint8_t g_active_room = 0;
HomeAssistantEntitySnapshot g_entity_snapshot[HA_MAX_AREA_ENTITIES] = {};

void copy_text(char *out, size_t out_len, const char *value) {
    if (!out || !out_len) return;
    snprintf(out, out_len, "%s", value ? value : "");
}

bool contains_ci(const char *text, const char *needle) {
    if (!text || !needle || !needle[0]) return false;
    for (const char *start = text; *start; ++start) {
        const char *a = start, *b = needle;
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

bool csv_contains_state(const char *csv, const char *state) {
    if (!csv || !csv[0] || !state) return false;
    const char *cursor = csv;
    while (*cursor) {
        while (*cursor == ' ' || *cursor == ',') ++cursor;
        const char *end = cursor;
        while (*end && *end != ',') ++end;
        const char *trimmed_end = end;
        while (trimmed_end > cursor && trimmed_end[-1] == ' ') --trimmed_end;
        const size_t len = static_cast<size_t>(trimmed_end - cursor);
        if (len == strlen(state)) {
            bool equal = true;
            for (size_t i = 0; i < len; ++i) {
                char left = cursor[i], right = state[i];
                if (left >= 'A' && left <= 'Z') left = static_cast<char>(left - 'A' + 'a');
                if (right >= 'A' && right <= 'Z') right = static_cast<char>(right - 'A' + 'a');
                if (left != right) { equal = false; break; }
            }
            if (equal) return true;
        }
        cursor = end;
    }
    return false;
}

void format_timer(char *out, size_t out_len, uint32_t total_seconds, bool paused) {
    const uint32_t hours = total_seconds / 3600U;
    const uint32_t minutes = (total_seconds / 60U) % 60U;
    const uint32_t seconds = total_seconds % 60U;
    if (paused) snprintf(out, out_len, "Paused %02lu:%02lu:%02lu",
                         static_cast<unsigned long>(hours), static_cast<unsigned long>(minutes),
                         static_cast<unsigned long>(seconds));
    else snprintf(out, out_len, "%02lu:%02lu:%02lu",
                  static_cast<unsigned long>(hours), static_cast<unsigned long>(minutes),
                  static_cast<unsigned long>(seconds));
}

UiControlKind kind_from(const PanelRoomControl &pref, const HomeAssistantEntitySnapshot *entity) {
    const char *type = pref.device_type[0] && strcmp(pref.device_type, "auto") != 0
                           ? pref.device_type
                           : entity ? entity->domain : "";
    if (strcmp(type, "light") == 0) return UiControlKind::Light;
    if (strcmp(type, "fan") == 0) return UiControlKind::Fan;
    if (strcmp(type, "cover") == 0) return UiControlKind::Cover;
    if (strcmp(type, "scene") == 0) return UiControlKind::Scene;
    if (strcmp(type, "switch") == 0) {
        const char *name = pref.label[0] ? pref.label : entity ? entity->name : "";
        if (contains_ci(name, "light") || contains_ci(name, "lamp")) return UiControlKind::Light;
        if (contains_ci(name, "fan")) return UiControlKind::Fan;
        if (contains_ci(name, "shade") || contains_ci(name, "blind") || contains_ci(name, "curtain"))
            return UiControlKind::Cover;
        return UiControlKind::Switch;
    }
    return UiControlKind::Unknown;
}

int group_for(UiControlKind kind) {
    if (kind == UiControlKind::Light) return 0;
    if (kind == UiControlKind::Cover) return 2;
    if (kind == UiControlKind::Scene) return 3;
    return 1;
}

const HomeAssistantEntitySnapshot *find_entity(const char *entity_id, size_t count) {
    if (!entity_id || !entity_id[0]) return nullptr;
    for (size_t i = 0; i < count; ++i)
        if (strcmp(g_entity_snapshot[i].entity_id, entity_id) == 0) return &g_entity_snapshot[i];
    return nullptr;
}

void format_sensor(char *out, size_t out_len, const char *entity_id,
                   size_t entity_count, const char *suffix) {
    const auto *entity = find_entity(entity_id, entity_count);
    if (!entity || !entity->available || !entity->state[0] ||
        strcmp(entity->state, "unknown") == 0 || strcmp(entity->state, "unavailable") == 0) {
        copy_text(out, out_len, "--");
        return;
    }
    snprintf(out, out_len, "%s%s", entity->state, suffix ? suffix : "");
}

void build_control(RoomControlViewModel &out, const PanelRoomControl &pref,
                   const HomeAssistantEntitySnapshot *entity) {
    memset(&out, 0, sizeof(out));
    copy_text(out.entity_id, sizeof(out.entity_id), pref.entity_id);
    copy_text(out.title, sizeof(out.title), pref.label[0] ? pref.label :
              entity && entity->name[0] ? entity->name : pref.entity_id);
    out.kind = kind_from(pref, entity);
    out.available = entity && entity->available;
    out.active = entity && (strcmp(entity->state, "on") == 0 ||
                            strcmp(entity->state, "open") == 0 ||
                            strcmp(entity->state, "opening") == 0);
    out.favorite = pref.placement == 1;
    if (entity) {
        out.supports_level = ((out.kind == UiControlKind::Light || out.kind == UiControlKind::Switch) &&
                              entity->supports_brightness) ||
                             (out.kind == UiControlKind::Fan && entity->supports_fan_speed);
        out.level_pct = out.kind == UiControlKind::Fan ? entity->fan_speed_pct : entity->brightness_pct;
    }
    if (!out.available) copy_text(out.state_text, sizeof(out.state_text), "Unavailable");
    else if (out.kind == UiControlKind::Scene) copy_text(out.state_text, sizeof(out.state_text), "Scene");
    else if (out.kind == UiControlKind::Cover)
        snprintf(out.state_text, sizeof(out.state_text), "%s", out.active ? "Open" : "Closed");
    else if (out.kind == UiControlKind::Fan && out.supports_level && out.active)
        copy_text(out.state_text, sizeof(out.state_text),
                  out.level_pct <= 40 ? "Low" : out.level_pct <= 75 ? "Medium" : "High");
    else if (out.supports_level && out.active)
        snprintf(out.state_text, sizeof(out.state_text), "On  |  %u%%", static_cast<unsigned>(out.level_pct));
    else copy_text(out.state_text, sizeof(out.state_text), out.active ? "On" : "Off");
}

void build_status_slot(RoomStatusViewModel &out, const PanelRoomStatusSlot &slot,
                       const RoomViewModel &room, const HomeAssistantEntitySnapshot *entity) {
    memset(&out, 0, sizeof(out));
    copy_text(out.label, sizeof(out.label), slot.label);
    copy_text(out.icon, sizeof(out.icon), slot.icon[0] ? slot.icon : "auto");
    copy_text(out.entity_id, sizeof(out.entity_id), slot.entity_id);
    copy_text(out.color, sizeof(out.color), slot.color[0] ? slot.color : "cyan");
    if (strcmp(slot.type, "devices") == 0) {
        out.available = room.device_count > 0;
        out.active = out.available && room.devices_online == room.device_count;
        snprintf(out.value, sizeof(out.value), "%u / %u", static_cast<unsigned>(room.devices_online),
                 static_cast<unsigned>(room.device_count));
        return;
    }
    if (strcmp(slot.type, "controls") == 0) {
        out.available = room.device_count > 0;
        out.active = room.healthy;
        copy_text(out.value, sizeof(out.value), room.system_status);
        return;
    }
    out.available = entity && entity->available && entity->state[0] &&
                    strcmp(entity->state, "unknown") != 0 && strcmp(entity->state, "unavailable") != 0;
    if (!out.available) { copy_text(out.value, sizeof(out.value), "--"); return; }
    out.active = slot.active_states[0] ? csv_contains_state(slot.active_states, entity->state) : true;
    if (strcmp(entity->domain, "timer") == 0 && strcmp(entity->state, "idle") == 0) {
        copy_text(out.value, sizeof(out.value), "Idle");
    } else if (strcmp(entity->domain, "timer") == 0 && entity->timer_has_remaining) {
        format_timer(out.value, sizeof(out.value), entity->timer_remaining_seconds,
                     strcmp(entity->state, "paused") == 0);
    } else if (out.active && slot.active_label[0]) {
        copy_text(out.value, sizeof(out.value), slot.active_label);
    } else if (!out.active && slot.inactive_label[0]) {
        copy_text(out.value, sizeof(out.value), slot.inactive_label);
    } else if (strcmp(entity->domain, "sensor") == 0) {
        const char *unit = entity->unit_of_measurement;
        if (!unit[0] && strcmp(slot.icon, "temperature") == 0) unit = "°";
        if (!unit[0] && strcmp(slot.icon, "humidity") == 0) unit = "%";
        const bool compact_unit = unit[0] == '%' || static_cast<unsigned char>(unit[0]) == 0xC2;
        snprintf(out.value, sizeof(out.value), "%s%s%s", entity->state,
                 unit[0] && !compact_unit ? " " : "", unit);
    } else {
        copy_text(out.value, sizeof(out.value), entity->state);
    }
}

}  // namespace

void ui_state_model_media_player(const HomeAssistantMediaSnapshot &source,
                                 MediaPlayerViewModel &out) {
    memset(&out, 0, sizeof(out));
    copy_text(out.entity_id, sizeof(out.entity_id), source.entity_id);
    copy_text(out.player_name, sizeof(out.player_name),
              source.name[0] ? source.name : source.entity_id);
    if (source.title[0]) copy_text(out.title, sizeof(out.title), source.title);
    else if (strcmp(source.state, "off") == 0) copy_text(out.title, sizeof(out.title), "Player is off");
    else if (strcmp(source.state, "idle") == 0) copy_text(out.title, sizeof(out.title), "Nothing playing");
    else if (strcmp(source.state, "unavailable") == 0) copy_text(out.title, sizeof(out.title), "Player unavailable");
    else copy_text(out.title, sizeof(out.title), "No media title");
    if (source.artist[0]) copy_text(out.artist, sizeof(out.artist), source.artist);
    else if (source.playlist[0]) snprintf(out.artist, sizeof(out.artist), "Playlist: %s", source.playlist);
    copy_text(out.album, sizeof(out.album), source.album);
    const char *playback = !source.available ? "Unavailable" :
                           strcmp(source.state, "playing") == 0 ? "Playing" :
                           strcmp(source.state, "paused") == 0 ? "Paused" :
                           strcmp(source.state, "off") == 0 ? "Player off" : "Ready";
    snprintf(out.state_text, sizeof(out.state_text), "%s%s%s", playback,
             source.source[0] ? "  /  " : "", source.source);
    copy_text(out.artwork_url, sizeof(out.artwork_url), source.entity_picture);
    out.volume_pct = source.volume_pct;
    out.available = source.available;
    out.playing = strcmp(source.state, "playing") == 0;
    out.muted = source.volume_muted;
    out.supports_volume = source.supports_volume;
    out.supports_mute = source.supports_mute;
}

uint8_t ui_state_model_active_room() { return g_active_room; }

void ui_state_model_set_active_room(uint8_t room_index) {
    const auto &cfg = config_service_get();
    g_active_room = cfg.room_count ? std::min<uint8_t>(room_index, cfg.room_count - 1) : 0;
}

void ui_state_model_next_room() {
    const auto &cfg = config_service_get();
    if (cfg.room_count > 1) g_active_room = static_cast<uint8_t>((g_active_room + 1) % cfg.room_count);
}

bool ui_state_model_snapshot_room(RoomViewModel &room,
                                  RoomControlViewModel *controls,
                                  size_t control_capacity,
                                  size_t &control_count) {
    memset(&room, 0, sizeof(room));
    control_count = 0;
    const auto &cfg = config_service_get();
    if (g_active_room >= cfg.room_count) g_active_room = 0;
    room.active_room = g_active_room;
    room.room_count = cfg.room_count;
    const PanelRoom *configured_room = cfg.room_count ? &cfg.rooms[g_active_room] : nullptr;
    copy_text(room.room_name, sizeof(room.room_name), configured_room ? configured_room->tab_label : "Room");
    copy_text(room.room_heading, sizeof(room.room_heading), configured_room ? configured_room->header : "Your room");

    const size_t entity_count = home_assistant_get_layout_entities(g_entity_snapshot, HA_MAX_AREA_ENTITIES);
    format_sensor(room.temperature, sizeof(room.temperature),
                  configured_room ? configured_room->temperature_entity_id : "", entity_count, "°");
    format_sensor(room.humidity, sizeof(room.humidity),
                  configured_room ? configured_room->humidity_entity_id : "", entity_count, "%");

    for (uint8_t i = 0; i < cfg.room_control_count; ++i) {
        const auto &pref = cfg.room_controls[i];
        if (pref.room_index != g_active_room || pref.placement == 2) continue;
        RoomControlViewModel item = {};
        build_control(item, pref, find_entity(pref.entity_id, entity_count));
        const int group = group_for(item.kind);
        ++room.group_total[group];
        if (item.available && (group == 1 || item.active || item.kind == UiControlKind::Scene))
            ++room.group_active[group];
        ++room.device_count;
        if (item.available) ++room.devices_online;
        if (item.favorite && room.favorite_count < 4) room.favorites[room.favorite_count++] = item;
        if (controls && control_count < control_capacity) controls[control_count++] = item;
    }

    const uint16_t unavailable = static_cast<uint16_t>(room.device_count - room.devices_online);
    room.healthy = room.device_count > 0 && unavailable == 0;
    room.busy = room.device_count > 0 && unavailable > 0;
    if (room.device_count == 0) {
        copy_text(room.system_status, sizeof(room.system_status), "No controls");
        copy_text(room.system_detail, sizeof(room.system_detail), "No controls are assigned to this room.");
    } else if (unavailable == 0) {
        copy_text(room.system_status, sizeof(room.system_status), "All ready");
        snprintf(room.system_detail, sizeof(room.system_detail), "%u room controls are available.",
                 static_cast<unsigned>(room.device_count));
    } else {
        snprintf(room.system_status, sizeof(room.system_status), "%u offline",
                 static_cast<unsigned>(unavailable));
        snprintf(room.system_detail, sizeof(room.system_detail), "%u of %u room controls are unavailable.",
                 static_cast<unsigned>(unavailable), static_cast<unsigned>(room.device_count));
    }
    if (configured_room) {
        for (uint8_t i = 0; i < PANEL_ROOM_STATUS_SLOTS; ++i) {
            const PanelRoomStatusSlot &slot = configured_room->status_slots[i];
            build_status_slot(room.status_slots[i], slot, room, find_entity(slot.entity_id, entity_count));
        }
    }
    return configured_room != nullptr;
}

bool ui_state_model_activate(const RoomControlViewModel &control) {
    if (!control.available || !control.entity_id[0]) return false;
    HomeAssistantEntitySnapshot current = {};
    if (!home_assistant_get_room_entity(control.entity_id, current) || !current.available) return false;
    return control.kind == UiControlKind::Scene
               ? home_assistant_queue_scene(control.entity_id)
               : home_assistant_queue_toggle(control.entity_id);
}

bool ui_state_model_set_level(const RoomControlViewModel &control, uint8_t level_pct) {
    if (!control.available || !control.supports_level || !control.entity_id[0]) return false;
    HomeAssistantEntitySnapshot current = {};
    if (!home_assistant_get_room_entity(control.entity_id, current) || !current.available) return false;
    return strcmp(current.domain, "fan") == 0
               ? home_assistant_queue_fan_speed(control.entity_id, level_pct)
               : home_assistant_queue_light_brightness(control.entity_id, level_pct);
}


