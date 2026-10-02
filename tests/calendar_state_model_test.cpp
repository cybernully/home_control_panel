#include "config_service.h"
#include "home_assistant.h"
#include "ui_state_model.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <time.h>

static PanelConfig config = {};
static HomeAssistantCalendarSnapshot snapshot = {};
static HomeAssistantCalendarEvent events[3] = {};
static char requested_start[40] = {}, requested_end[40] = {};

const PanelConfig &config_service_get() { return config; }
bool home_assistant_request_calendar_events(const char *start, const char *end) {
    snprintf(requested_start, sizeof(requested_start), "%s", start);
    snprintf(requested_end, sizeof(requested_end), "%s", end);
    return true;
}
void home_assistant_get_calendar(HomeAssistantCalendarSnapshot &out) { out = snapshot; }
bool home_assistant_get_calendar_event(size_t index, HomeAssistantCalendarEvent &out) {
    if (index >= snapshot.event_count) return false;
    out = events[index]; return true;
}

int main() {
    config.calendar_count = 2;
    config.calendar_week_starts_monday = true;
    snprintf(config.calendars[0].entity_id, sizeof(config.calendars[0].entity_id), "calendar.work");
    snprintf(config.calendars[0].label, sizeof(config.calendars[0].label), "Work");
    snprintf(config.calendars[0].color, sizeof(config.calendars[0].color), "cyan");
    snprintf(config.calendars[1].entity_id, sizeof(config.calendars[1].entity_id), "calendar.family");
    snprintf(config.calendars[1].label, sizeof(config.calendars[1].label), "Family");
    snprintf(config.calendars[1].color, sizeof(config.calendars[1].color), "purple");

    const time_t now = time(nullptr);
    struct tm local = *localtime(&now);
    local.tm_hour = 0; local.tm_min = 0; local.tm_sec = 0; local.tm_isdst = -1;
    mktime(&local);
    const uint8_t today_index = static_cast<uint8_t>((local.tm_wday + 6) % 7);
    char today[16] = {}, tomorrow[16] = {}, timed_start[32] = {}, timed_end[32] = {};
    strftime(today, sizeof(today), "%Y-%m-%d", &local);
    struct tm next = local; next.tm_mday += 1; next.tm_isdst = -1; mktime(&next);
    strftime(tomorrow, sizeof(tomorrow), "%Y-%m-%d", &next);
    snprintf(timed_start, sizeof(timed_start), "%sT09:00:00", today);
    snprintf(timed_end, sizeof(timed_end), "%sT10:00:00", today);

    snapshot.available = true; snapshot.event_count = 3;
    snprintf(events[0].calendar_entity_id, sizeof(events[0].calendar_entity_id), "calendar.work");
    snprintf(events[0].summary, sizeof(events[0].summary), "Standup");
    snprintf(events[0].start, sizeof(events[0].start), "%s", timed_start);
    snprintf(events[0].end, sizeof(events[0].end), "%s", timed_end);
    snprintf(events[1].calendar_entity_id, sizeof(events[1].calendar_entity_id), "calendar.family");
    snprintf(events[1].summary, sizeof(events[1].summary), "Family day");
    snprintf(events[1].start, sizeof(events[1].start), "%s", today);
    snprintf(events[1].end, sizeof(events[1].end), "%s", tomorrow);
    snprintf(events[2].calendar_entity_id, sizeof(events[2].calendar_entity_id), "calendar.family");
    snprintf(events[2].summary, sizeof(events[2].summary), "Tomorrow");
    snprintf(events[2].start, sizeof(events[2].start), "%sT12:00:00", tomorrow);
    snprintf(events[2].end, sizeof(events[2].end), "%sT13:00:00", tomorrow);

    CalendarViewModel model = {};
    assert(ui_state_model_snapshot_calendar(0, today_index, model));
    assert(requested_start[0] && requested_end[0]);
    assert(model.days[today_index].today);
    assert(model.days[today_index].event_count == 2);
    assert(model.event_count == 2);
    assert(strcmp(model.events[0].title, "Family day") == 0);
    assert(strcmp(model.events[0].time, "All day") == 0);
    assert(strcmp(model.events[1].title, "Standup") == 0);
    assert(strstr(model.events[1].time, "9:00 AM"));
    return 0;
}
