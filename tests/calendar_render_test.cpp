#include "calendar_module.h"
#include "config_service.h"
#include "ui_state_model.h"

#include <lvgl.h>
#include <cassert>
#include <cstdio>
#include <cstring>

static PanelConfig config = {};
static CalendarViewModel calendar = {};

const PanelConfig &config_service_get() { return config; }
bool ui_state_model_snapshot_calendar(int16_t, uint8_t selected_day, CalendarViewModel &out) {
    calendar.days[0].selected = selected_day == 0;
    calendar.days[1].selected = selected_day == 1;
    out = calendar;
    return out.available;
}

static unsigned char buffer[1280 * 658 * 4];
static void flush(lv_display_t *display, const lv_area_t *, uint8_t *) { lv_display_flush_ready(display); }
static lv_obj_t *find(lv_obj_t *root, const char *text) {
    if (lv_obj_check_type(root, &lv_label_class) && strcmp(lv_label_get_text(root), text) == 0) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto *result = find(lv_obj_get_child(root, i), text)) return result;
    return nullptr;
}
static void shot(const char *name) {
    lv_refr_now(nullptr); FILE *file = fopen(name, "wb"); assert(file);
    fprintf(file, "P6\n1280 658\n255\n");
    for (int i = 0; i < 1280 * 658; ++i) {
        fputc(buffer[4*i+2], file); fputc(buffer[4*i+1], file); fputc(buffer[4*i], file);
    }
    fclose(file);
}

int main() {
    lv_init(); auto *display = lv_display_create(1280, 658);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display, buffer, nullptr, sizeof(buffer), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    config.calendar_count = 2;
    config.calendar_week_starts_monday = true;
    calendar.available = true;
    snprintf(calendar.week_label, sizeof(calendar.week_label), "September 28 - October 4, 2026");
    snprintf(calendar.selected_day_label, sizeof(calendar.selected_day_label), "Thursday, October 01");
    snprintf(calendar.status, sizeof(calendar.status), "3 events");
    const char *days[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
    const char *dates[] = {"28","29","30","01","02","03","04"};
    for (uint8_t i = 0; i < 7; ++i) {
        snprintf(calendar.days[i].weekday, sizeof(calendar.days[i].weekday), "%s", days[i]);
        snprintf(calendar.days[i].date, sizeof(calendar.days[i].date), "%s", dates[i]);
        calendar.days[i].event_count = i == 3 ? 3 : i % 2;
        calendar.days[i].selected = i == 3;
        calendar.days[i].today = i == 3;
    }
    calendar.event_count = 3;
    const char *titles[] = {"Team standup", "Dentist appointment", "School concert"};
    const char *times[] = {"9:00 AM - 9:30 AM", "1:00 PM - 2:00 PM", "All day"};
    for (uint8_t i = 0; i < calendar.event_count; ++i) {
        snprintf(calendar.events[i].title, sizeof(calendar.events[i].title), "%s", titles[i]);
        snprintf(calendar.events[i].time, sizeof(calendar.events[i].time), "%s", times[i]);
        snprintf(calendar.events[i].calendar, sizeof(calendar.events[i].calendar), "%s", i == 2 ? "Family" : "Work");
        snprintf(calendar.events[i].color, sizeof(calendar.events[i].color), "%s", i == 2 ? "purple" : "cyan");
        if (i == 1) snprintf(calendar.events[i].location, sizeof(calendar.events[i].location), "Downtown Clinic");
    }
    CalendarModule module; auto *root = lv_screen_active(); module.create(root); lv_obj_update_layout(root);
    assert(find(root, "Calendar") && find(root, "Today") && find(root, "Thursday, October 01"));
    assert(find(root, "Team standup") && find(root, "Dentist appointment") && find(root, "School concert"));
    shot(".test-build/calendar-week.ppm");
    lv_obj_send_event(lv_obj_get_parent(find(root, "Team standup")), LV_EVENT_CLICKED, nullptr);
    lv_obj_update_layout(root);
    auto *details = find(root, "EVENT DETAILS");
    assert(details && !lv_obj_has_flag(lv_obj_get_parent(lv_obj_get_parent(details)), LV_OBJ_FLAG_HIDDEN));
    shot(".test-build/calendar-event-details.ppm");
    return 0;
}
