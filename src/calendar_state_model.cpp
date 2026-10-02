#include "ui_state_model.h"

#include "config_service.h"
#include "home_assistant.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

namespace {
constexpr time_t DAY_SECONDS = 86400;

void copy_text(char *out, size_t out_len, const char *value) {
    if (out && out_len) snprintf(out, out_len, "%s", value ? value : "");
}

int64_t days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

bool parse_time(const char *value, time_t &timestamp, bool &all_day) {
    if (!value || !value[0]) return false;
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    all_day = strlen(value) == 10;
    if (all_day) {
        if (sscanf(value, "%d-%d-%d", &year, &month, &day) != 3) return false;
        struct tm local = {};
        local.tm_year = year - 1900; local.tm_mon = month - 1; local.tm_mday = day;
        local.tm_isdst = -1;
        timestamp = mktime(&local);
        return timestamp > 0;
    }
    if (sscanf(value, "%d-%d-%d%*c%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6)
        return false;
    const char *zone = value + 19;
    while (*zone && *zone != 'Z' && *zone != '+' && *zone != '-') ++zone;
    if (*zone == 'Z' || *zone == '+' || *zone == '-') {
        int64_t epoch = days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * DAY_SECONDS +
                        hour * 3600 + minute * 60 + second;
        if (*zone == '+' || *zone == '-') {
            int zone_hour = 0, zone_minute = 0;
            if (sscanf(zone + 1, "%d:%d", &zone_hour, &zone_minute) >= 1) {
                const int offset = zone_hour * 3600 + zone_minute * 60;
                epoch += *zone == '+' ? -offset : offset;
            }
        }
        timestamp = static_cast<time_t>(epoch);
        return true;
    }
    struct tm local = {};
    local.tm_year = year - 1900; local.tm_mon = month - 1; local.tm_mday = day;
    local.tm_hour = hour; local.tm_min = minute; local.tm_sec = second; local.tm_isdst = -1;
    timestamp = mktime(&local);
    return timestamp > 0;
}

const PanelCalendarSource *source_for(const PanelConfig &cfg, const char *entity_id) {
    for (uint8_t i = 0; i < cfg.calendar_count; ++i)
        if (strcmp(cfg.calendars[i].entity_id, entity_id) == 0) return &cfg.calendars[i];
    return nullptr;
}

void format_clock(char *out, size_t out_len, time_t timestamp) {
    const struct tm *local = localtime(&timestamp);
    if (!local) { copy_text(out, out_len, "Time unavailable"); return; }
    strftime(out, out_len, "%I:%M %p", local);
    if (out[0] == '0') memmove(out, out + 1, strlen(out));
}

void build_event(CalendarEventViewModel &out, const HomeAssistantCalendarEvent &source,
                 const PanelConfig &cfg, time_t start, time_t end, bool all_day) {
    const PanelCalendarSource *calendar = source_for(cfg, source.calendar_entity_id);
    copy_text(out.calendar, sizeof(out.calendar),
              calendar && calendar->label[0] ? calendar->label : source.calendar_entity_id);
    copy_text(out.color, sizeof(out.color), calendar ? calendar->color : "cyan");
    copy_text(out.title, sizeof(out.title), source.summary[0] ? source.summary : "Untitled event");
    copy_text(out.location, sizeof(out.location), source.location);
    copy_text(out.description, sizeof(out.description), source.description);
    if (all_day) copy_text(out.time, sizeof(out.time), "All day");
    else {
        char start_text[20] = {}, end_text[20] = {};
        format_clock(start_text, sizeof(start_text), start);
        format_clock(end_text, sizeof(end_text), end);
        snprintf(out.time, sizeof(out.time), "%s - %s", start_text, end_text);
    }
    const time_t display_end = all_day && end > start ? end - 1 : end;
    struct tm start_local = {}, end_local = {};
    if (const struct tm *value = localtime(&start)) start_local = *value;
    if (const struct tm *value = localtime(&display_end)) end_local = *value;
    char start_date[32] = {}, end_date[32] = {};
    strftime(start_date, sizeof(start_date), "%a, %b %d", &start_local);
    strftime(end_date, sizeof(end_date), "%a, %b %d", &end_local);
    snprintf(out.date_range, sizeof(out.date_range), "%s  |  %s", start_date, all_day ? "All day" : out.time);
    if (strcmp(start_date, end_date) != 0)
        snprintf(out.date_range, sizeof(out.date_range), "%s - %s", start_date, end_date);
}
}

bool ui_state_model_snapshot_calendar(int16_t week_offset, uint8_t selected_day,
                                      CalendarViewModel &calendar) {
    memset(&calendar, 0, sizeof(calendar));
    const PanelConfig &cfg = config_service_get();
    if (!cfg.calendar_count) {
        copy_text(calendar.status, sizeof(calendar.status), "Select calendars in Web Admin");
        return false;
    }

    const time_t now = time(nullptr);
    if (now < 1700000000) {
        copy_text(calendar.status, sizeof(calendar.status), "Waiting for date and time");
        return false;
    }
    struct tm today_tm = *localtime(&now);
    today_tm.tm_hour = 0; today_tm.tm_min = 0; today_tm.tm_sec = 0; today_tm.tm_isdst = -1;
    const time_t today = mktime(&today_tm);
    const int first_day = cfg.calendar_week_starts_monday ? 1 : 0;
    const int days_since_start = (today_tm.tm_wday - first_day + 7) % 7;
    struct tm week_start_tm = today_tm;
    week_start_tm.tm_mday += week_offset * 7 - days_since_start;
    week_start_tm.tm_isdst = -1;
    const time_t week_start = mktime(&week_start_tm);
    struct tm week_end_tm = week_start_tm;
    week_end_tm.tm_mday += 7; week_end_tm.tm_isdst = -1;
    const time_t week_end = mktime(&week_end_tm);
    if (selected_day > 6) selected_day = 0;

    char request_start[32] = {}, request_end[32] = {};
    week_start_tm = *localtime(&week_start);
    week_end_tm = *localtime(&week_end);
    strftime(request_start, sizeof(request_start), "%Y-%m-%d 00:00:00", &week_start_tm);
    strftime(request_end, sizeof(request_end), "%Y-%m-%d 00:00:00", &week_end_tm);
    home_assistant_request_calendar_events(request_start, request_end);

    struct tm last_tm = week_start_tm;
    last_tm.tm_mday += 6; last_tm.tm_isdst = -1;
    const time_t week_last = mktime(&last_tm);
    last_tm = *localtime(&week_last);
    if (week_start_tm.tm_mon == last_tm.tm_mon)
        strftime(calendar.week_label, sizeof(calendar.week_label), "%B %d", &week_start_tm);
    else
        strftime(calendar.week_label, sizeof(calendar.week_label), "%b %d", &week_start_tm);
    char tail[24] = {};
    strftime(tail, sizeof(tail), week_start_tm.tm_mon == last_tm.tm_mon ? " - %d, %Y" : " - %b %d, %Y", &last_tm);
    strncat(calendar.week_label, tail, sizeof(calendar.week_label) - strlen(calendar.week_label) - 1);

    time_t day_starts[8] = {};
    for (uint8_t day = 0; day <= 7; ++day) {
        struct tm local = week_start_tm;
        local.tm_mday += day; local.tm_isdst = -1;
        day_starts[day] = mktime(&local);
        local = *localtime(&day_starts[day]);
        if (day == 7) continue;
        strftime(calendar.days[day].weekday, sizeof(calendar.days[day].weekday), "%a", &local);
        strftime(calendar.days[day].date, sizeof(calendar.days[day].date), "%d", &local);
        calendar.days[day].today = day_starts[day] == today;
        calendar.days[day].selected = day == selected_day;
    }
    const time_t selected_start = day_starts[selected_day];
    const time_t selected_end = day_starts[selected_day + 1];
    struct tm selected_tm = *localtime(&selected_start);
    strftime(calendar.selected_day_label, sizeof(calendar.selected_day_label), "%A, %B %d", &selected_tm);

    HomeAssistantCalendarSnapshot snapshot = {};
    home_assistant_get_calendar(snapshot);
    calendar.loading = snapshot.loading;
    calendar.available = snapshot.available;
    time_t selected_epochs[8] = {};
    for (uint8_t i = 0; i < snapshot.event_count; ++i) {
        HomeAssistantCalendarEvent source = {};
        if (!home_assistant_get_calendar_event(i, source)) continue;
        time_t start = 0, end = 0;
        bool start_all_day = false, end_all_day = false;
        if (!parse_time(source.start, start, start_all_day)) continue;
        if (!parse_time(source.end, end, end_all_day)) end = start + (start_all_day ? DAY_SECONDS : 3600);
        if (end <= start) end = start + (start_all_day ? DAY_SECONDS : 3600);
        for (uint8_t day = 0; day < 7; ++day) {
            if (start < day_starts[day + 1] && end > day_starts[day] && calendar.days[day].event_count < 255)
                ++calendar.days[day].event_count;
        }
        if (start >= selected_end || end <= selected_start) continue;
        uint8_t insert = calendar.event_count;
        if (insert >= 8) continue;
        while (insert > 0 && selected_epochs[insert - 1] > start) {
            calendar.events[insert] = calendar.events[insert - 1];
            selected_epochs[insert] = selected_epochs[insert - 1];
            --insert;
        }
        build_event(calendar.events[insert], source, cfg, start, end, start_all_day);
        selected_epochs[insert] = start;
        ++calendar.event_count;
    }

    if (calendar.event_count)
        snprintf(calendar.status, sizeof(calendar.status), "%u event%s",
                 static_cast<unsigned>(calendar.event_count), calendar.event_count == 1 ? "" : "s");
    else if (calendar.loading) copy_text(calendar.status, sizeof(calendar.status), "Loading events...");
    else if (!calendar.available) copy_text(calendar.status, sizeof(calendar.status), "Calendar unavailable");
    else copy_text(calendar.status, sizeof(calendar.status), "No events");
    return snapshot.available || snapshot.loading;
}
