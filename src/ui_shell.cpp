#include "ui_shell.h"
#include "app_config.h"
#include "battery_service.h"
#include "board_lvgl.h"
#include "config_service.h"
#include "display_text.h"
#include <string.h>
#include "home_assistant.h"
#include "module_registry.h"
#include "network_service.h"
#include "ha_icons_font.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <lvgl.h>
#include <time.h>

namespace {
constexpr int SCREEN_W=1280,SCREEN_H=800,HEADER_H=96,FOOTER_H=78,PAGE_Y=HEADER_H,PAGE_H=SCREEN_H-HEADER_H-FOOTER_H;
constexpr uint32_t BG=ui_theme::BG,PANEL=ui_theme::SURFACE,TEXT=ui_theme::TEXT,MUTED=ui_theme::MUTED,ACCENT=ui_theme::ACCENT,BORDER=ui_theme::BORDER,SUCCESS=ui_theme::SUCCESS,WARN=ui_theme::WARN,BAD=ui_theme::DANGER;
lv_obj_t *g_screen=nullptr,*g_header_title=nullptr,*g_header_subtitle=nullptr,*g_clock=nullptr,*g_wifi=nullptr,*g_battery_label=nullptr,*g_battery_body=nullptr,*g_battery_fill=nullptr,*g_battery_tip=nullptr,*g_settings_button=nullptr;
lv_obj_t *g_status_button=nullptr,*g_status_dot=nullptr,*g_status_label=nullptr,*g_status_overlay=nullptr,*g_status_message=nullptr,*g_status_time=nullptr;
lv_obj_t *g_signal_bars[4] = {};
lv_obj_t *g_pages[PANEL_MAX_MODULES]={},*g_nav_buttons[PANEL_MAX_MODULES]={},*g_nav_icons[PANEL_MAX_MODULES]={},*g_nav_labels[PANEL_MAX_MODULES]={};bool g_created[PANEL_MAX_MODULES]={};size_t g_active_index=0;uint32_t g_last_header_ms=0,g_last_activity_ms=0;
char g_last_status[160] = "Starting Home Assistant connection...";
char g_last_status_time[48] = "Time not synchronized";
uint32_t g_manual_status_until = 0;
uint32_t g_last_action_seen_ms = 0;
void update_header();
void show_module(size_t index);
void style_box(lv_obj_t *o,uint32_t bg,int radius=0,int border=0){lv_obj_set_style_bg_color(o,lv_color_hex(bg),LV_PART_MAIN);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,LV_PART_MAIN);lv_obj_set_style_radius(o,radius,LV_PART_MAIN);lv_obj_set_style_border_width(o,border,LV_PART_MAIN);if(border)lv_obj_set_style_border_color(o,lv_color_hex(BORDER),LV_PART_MAIN);lv_obj_set_style_pad_all(o,0,LV_PART_MAIN);lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);}
lv_obj_t *label(lv_obj_t *p,const char *t,const lv_font_t *f,uint32_t c){lv_obj_t *o=lv_label_create(p);lv_label_set_text(o,t?t:"");lv_obj_set_style_text_font(o,f,LV_PART_MAIN);lv_obj_set_style_text_color(o,lv_color_hex(c),LV_PART_MAIN);return o;}
void format_time(char *out, size_t out_len, const char *format) {
    if (!out || !out_len) return;
    const time_t now = time(nullptr);
    const struct tm *local = localtime(&now);
    if (now < 1700000000 || !local) {
        snprintf(out, out_len, "Time syncing...");
        return;
    }
    strftime(out, out_len, format, local);
}
void remember_status(const char *message) {
    if (!message || !message[0] || strcmp(g_last_status, message) == 0) return;
    snprintf(g_last_status, sizeof(g_last_status), "%s", message);
    format_time(g_last_status_time, sizeof(g_last_status_time), "%a, %b %d  %I:%M:%S %p");
}
void settings_button_cb(lv_event_t *) {
    for (size_t i = 0; i < module_registry_count(); ++i) {
        PanelModule *module = module_registry_at(i);
        if (module && strcmp(module->id(), "settings") == 0) { show_module(i); return; }
    }
}
void status_overlay_cb(lv_event_t *) { if (g_status_overlay) lv_obj_add_flag(g_status_overlay, LV_OBJ_FLAG_HIDDEN); }
void status_button_cb(lv_event_t *) {
    if (!g_status_overlay) return;
    lv_label_set_text(g_status_message, g_last_status);
    lv_label_set_text(g_status_time, g_last_status_time);
    lv_obj_remove_flag(g_status_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_status_overlay);
}
// Battery body, terminal and fill share a fixed icon coordinate system. This
// avoids align_to() reading a sibling's not-yet-resolved layout at startup.
void update_battery() {
    BatteryStatus battery = {};
    const bool valid = battery_service_get_status(battery) && battery.valid;
    const unsigned percent = valid ? (battery.percent > 100 ? 100 : battery.percent) : 0;
    const uint32_t color = !valid ? MUTED : percent <= 10 ? BAD : percent <= 20 ? WARN : 0x38BDF8;
    char text[12];
    if (valid) snprintf(text, sizeof(text), "%u%%", percent);
    else snprintf(text, sizeof(text), "--");
    lv_label_set_text(g_battery_label, text);
    lv_obj_set_style_text_color(g_battery_label, lv_color_hex(valid && percent <= 20 ? color : TEXT), LV_PART_MAIN);
    lv_obj_set_style_border_color(g_battery_body, lv_color_hex(valid ? color : MUTED), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_battery_tip, lv_color_hex(valid ? color : MUTED), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_battery_fill, lv_color_hex(color), LV_PART_MAIN);
    if (valid && percent) {
        lv_obj_set_width(g_battery_fill, (28 * percent + 99) / 100);
        lv_obj_remove_flag(g_battery_fill, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_battery_fill, LV_OBJ_FLAG_HIDDEN);
    }
}

void update_header() {
    const PanelConfig &cfg = config_service_get();
    char title[96];
    panel_display_text(title, sizeof(title), cfg.display_name);
    lv_label_set_text(g_header_title, title);
    char clock_text[48] = {};
    format_time(clock_text, sizeof(clock_text), "%a, %b %d  |  %I:%M %p");
    lv_label_set_text(g_clock, clock_text);
    HomeAssistantDiscoveryStatus discovery = {};
    home_assistant_get_discovery_status(discovery);

    const bool connected = network_service_connected();
    const bool ready = connected && discovery.websocket_authenticated && discovery.discovery_complete;
    const char *live_message = discovery.last_action[0] ? discovery.last_action :
                               (discovery.message[0] ? discovery.message : "Waiting for Home Assistant");
    if (discovery.last_action_ms && discovery.last_action_ms != g_last_action_seen_ms) {
        g_last_action_seen_ms = discovery.last_action_ms;
        g_last_status[0] = '\0';
    }
    if (!g_manual_status_until || static_cast<int32_t>(millis() - g_manual_status_until) >= 0)
        remember_status(live_message);
    HomeAssistantStatus health = {};
    home_assistant_get_status(health);
    const bool failed = !connected || !health.configured || strstr(live_message, "failed") ||
                        strstr(live_message, "rejected") || strstr(live_message, "invalid");
    const uint32_t status_color = ready ? SUCCESS : failed ? BAD : WARN;
    lv_obj_set_style_bg_color(g_status_dot, lv_color_hex(status_color), LV_PART_MAIN);
    lv_label_set_text(g_status_label, ready ? "All good" : failed ? "Offline" : "Syncing");
    lv_obj_set_style_text_color(g_status_label, lv_color_hex(status_color), LV_PART_MAIN);
    const int rssi = connected ? network_service_rssi() : -127;
    const int strength = !connected ? 0 : rssi >= -55 ? 4 : rssi >= -67 ? 3 : rssi >= -75 ? 2 : 1;
    lv_label_set_text(g_wifi, connected ? "Wi-Fi" : "Offline");
    lv_obj_set_style_text_color(g_wifi, lv_color_hex(connected ? TEXT : WARN), LV_PART_MAIN);
    for (int i = 0; i < 4; ++i)
        lv_obj_set_style_bg_color(g_signal_bars[i], lv_color_hex(i < strength ? 0x38BDF8 : BORDER), LV_PART_MAIN);
    update_battery();
}

void create_header() {
    lv_obj_t *header = lv_obj_create(g_screen);
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_set_pos(header, 0, 0);
    style_box(header, 0x071D33);
    // A restrained keyline separates the persistent header from page content.
    lv_obj_t *divider = lv_obj_create(header);
    style_box(divider, 0x263449);
    lv_obj_set_size(divider, SCREEN_W, 1);
    lv_obj_set_pos(divider, 0, HEADER_H - 1);
    g_header_title = label(header, "", &lv_font_montserrat_28, TEXT);
    lv_obj_set_pos(g_header_title, 30, 15);
    lv_obj_set_width(g_header_title, 370);
    lv_label_set_long_mode(g_header_title, LV_LABEL_LONG_DOT);
    g_clock = label(header, "Time syncing...", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(g_clock, 31, 57); lv_obj_set_width(g_clock, 360);

    auto divider_at = [&](int x) {
        lv_obj_t *vertical = lv_obj_create(header);
        style_box(vertical, BORDER);
        lv_obj_set_pos(vertical, x, 18);
        lv_obj_set_size(vertical, 1, 60);
    };
    divider_at(420); divider_at(704); divider_at(904); divider_at(1136);

    g_status_button = lv_obj_create(header);
    lv_obj_add_flag(g_status_button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(g_status_button, 438, 10);
    lv_obj_set_size(g_status_button, 248, 76);
    style_box(g_status_button, 0x071D33, 0, 0);
    g_status_dot = lv_obj_create(g_status_button);
    style_box(g_status_dot, WARN, 8);
    lv_obj_set_size(g_status_dot, 16, 16);
    lv_obj_align(g_status_dot, LV_ALIGN_LEFT_MID, 14, 0);
    g_status_label = label(g_status_button, "Syncing", &lv_font_montserrat_16, TEXT);
    lv_obj_set_width(g_status_label, 178);
    lv_obj_align(g_status_label, LV_ALIGN_RIGHT_MID, -12, -9);
    g_header_subtitle = label(g_status_button,"System Status",&lv_font_montserrat_12,MUTED);
    lv_obj_align(g_header_subtitle,LV_ALIGN_RIGHT_MID,-12,13);
    lv_obj_add_event_cb(g_status_button, status_button_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *wifi = lv_obj_create(header);
    lv_obj_set_pos(wifi, 722, 10);
    lv_obj_set_size(wifi, 164, 76);
    style_box(wifi, 0x071D33, 0, 0);
    for (int i = 0; i < 4; ++i) {
        g_signal_bars[i] = lv_obj_create(wifi);
        style_box(g_signal_bars[i], BORDER, 1);
        const int height = 5 + i * 3;
        lv_obj_set_size(g_signal_bars[i], 3, height);
        lv_obj_set_pos(g_signal_bars[i], 17 + i * 6, 29 - height);
    }
    g_wifi = label(wifi, "Offline", &lv_font_montserrat_12, TEXT);
    lv_obj_align(g_wifi, LV_ALIGN_LEFT_MID, 48, 0);

    lv_obj_t *battery = lv_obj_create(header);
    lv_obj_set_pos(battery, 922, 10);
    lv_obj_set_size(battery, 196, 76);
    style_box(battery, 0x071D33, 0, 0);
    lv_obj_t *icon = lv_obj_create(battery);
    style_box(icon, 0x071D33);
    lv_obj_set_style_bg_opa(icon, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_size(icon, 42, 24);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 16, 0);
    g_battery_body = lv_obj_create(icon);
    style_box(g_battery_body, 0x071D33, 4, 2);
    lv_obj_set_style_bg_opa(g_battery_body, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_size(g_battery_body, 36, 20);
    lv_obj_set_pos(g_battery_body, 0, 2);
    g_battery_tip = lv_obj_create(icon);
    style_box(g_battery_tip, MUTED, 1);
    lv_obj_set_size(g_battery_tip, 4, 8);
    lv_obj_set_pos(g_battery_tip, 37, 8);
    g_battery_fill = lv_obj_create(icon);
    style_box(g_battery_fill, 0x38BDF8, 1);
    lv_obj_set_size(g_battery_fill, 28, 12);
    lv_obj_set_pos(g_battery_fill, 4, 6);
    lv_obj_add_flag(g_battery_fill, LV_OBJ_FLAG_HIDDEN);
    g_battery_label = label(battery, "--", &lv_font_montserrat_16, TEXT);
    lv_obj_set_width(g_battery_label, 44);
    lv_obj_set_style_text_align(g_battery_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(g_battery_label, LV_ALIGN_RIGHT_MID, -18, 0);

    g_settings_button=lv_obj_create(header);lv_obj_add_flag(g_settings_button,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_pos(g_settings_button,1152,10);lv_obj_set_size(g_settings_button,108,76);
    style_box(g_settings_button,0x071D33,0,0);lv_obj_add_event_cb(g_settings_button,settings_button_cb,LV_EVENT_CLICKED,nullptr);
    lv_obj_t *gear=label(g_settings_button,"",&ha_icons_font,0xAFC6FF);ui_theme::set_glyph(gear,0xF1064);lv_obj_center(gear);

    g_status_overlay = lv_obj_create(g_screen);
    lv_obj_set_size(g_status_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(g_status_overlay, 0, 0);
    style_box(g_status_overlay, 0x030712);
    lv_obj_set_style_bg_opa(g_status_overlay, LV_OPA_80, LV_PART_MAIN);
    lv_obj_add_event_cb(g_status_overlay, status_overlay_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *dialog = lv_obj_create(g_status_overlay);
    lv_obj_set_size(dialog, 600, 260);
    lv_obj_center(dialog);
    style_box(dialog, PANEL, 24, 1);
    lv_obj_remove_flag(dialog, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *dialog_title = label(dialog, "HOME ASSISTANT STATUS", &lv_font_montserrat_16, TEXT);
    lv_obj_set_pos(dialog_title, 28, 24);
    g_status_message = label(dialog, "", &lv_font_montserrat_20, TEXT);
    lv_obj_set_pos(g_status_message, 28, 70);
    lv_obj_set_width(g_status_message, 544);
    lv_label_set_long_mode(g_status_message, LV_LABEL_LONG_WRAP);
    g_status_time = label(dialog, "", &lv_font_montserrat_14, MUTED);
    lv_obj_set_pos(g_status_time, 28, 184);
    auto *close = label(dialog, "Tap anywhere to close", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(close, 28, 220);
    lv_obj_add_flag(g_status_overlay, LV_OBJ_FLAG_HIDDEN);
}
void show_module(size_t index) {
    if (index >= module_registry_count()) return;
    if (g_active_index < module_registry_count() && g_active_index != index) {
        PanelModule *old = module_registry_at(g_active_index);
        if (old) old->on_deactivate();
    }
    if (!g_created[index]) {
        PanelModule *module = module_registry_at(index);
        if (!module) return;
        module->create(g_pages[index]);
        g_created[index] = true;
    }
    for (size_t i = 0; i < module_registry_count(); ++i) {
        if (i == index) lv_obj_remove_flag(g_pages[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(g_pages[i], LV_OBJ_FLAG_HIDDEN);
        if (!g_nav_buttons[i]) continue;  // Settings is reached from the header only.
        style_box(g_nav_buttons[i], i == index ? ACCENT : PANEL, 14, i == index ? 0 : 1);
        lv_obj_set_style_text_color(g_nav_icons[i], lv_color_hex(i == index ? 0xBFF7FF : 0xAFC6FF), LV_PART_MAIN);
        lv_obj_set_style_text_color(g_nav_labels[i], lv_color_hex(i == index ? TEXT : MUTED), LV_PART_MAIN);
    }
    g_active_index = index;
    PanelModule *module = module_registry_at(index);
    if (module) { module->on_activate(); module->update(); }
}
void nav_cb(lv_event_t *e){const size_t index=reinterpret_cast<size_t>(lv_event_get_user_data(e));show_module(index);}
}

void ui_shell_begin() {
    g_screen = lv_screen_active();
    style_box(g_screen, BG);
    create_header();
    lv_obj_t *footer = lv_obj_create(g_screen);
    lv_obj_set_size(footer, SCREEN_W, FOOTER_H);
    lv_obj_set_pos(footer, 0, SCREEN_H - FOOTER_H);
    style_box(footer, 0x071D33, 18, 1);

    const size_t count = module_registry_count();
    size_t nav_count = 0;
    for (size_t i = 0; i < count; ++i) {
        PanelModule *module = module_registry_at(i);
        if (module && strcmp(module->id(), "settings") != 0) ++nav_count;
        g_pages[i] = lv_obj_create(g_screen);
        lv_obj_set_size(g_pages[i], SCREEN_W, PAGE_H);
        lv_obj_set_pos(g_pages[i], 0, PAGE_Y);
        style_box(g_pages[i], BG);
        lv_obj_add_flag(g_pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    const int gap = 5;
    const int available = SCREEN_W - 24 - static_cast<int>((nav_count ? nav_count - 1 : 0) * gap);
    const int button_w = nav_count ? available / static_cast<int>(nav_count) : available;
    int x = 12;
    for (size_t i = 0; i < count; ++i) {
        PanelModule *module = module_registry_at(i);
        if (!module || strcmp(module->id(), "settings") == 0) continue;
        uint32_t glyph = 0xF0425;
        if (strcmp(module->id(), "room") == 0) glyph = 0xF0335;
        else if (strcmp(module->id(), "media") == 0) glyph = 0xF03D8;
        else if (strcmp(module->id(), "climate") == 0) glyph = 0xF0210;
        else if (strcmp(module->id(), "security") == 0) glyph = 0xF0902;
        else if (strcmp(module->id(), "weather") == 0) glyph = 0xF1011;
        else if (strcmp(module->id(), "calendar") == 0) glyph = 0xF00AC;
        g_nav_buttons[i] = lv_button_create(footer);
        lv_obj_set_size(g_nav_buttons[i], button_w, 64);
        lv_obj_set_pos(g_nav_buttons[i], x, 7);
        style_box(g_nav_buttons[i], i == 0 ? ACCENT : 0x071D33, 14, i == 0 ? 0 : 1);
        lv_obj_add_event_cb(g_nav_buttons[i], nav_cb, LV_EVENT_CLICKED, reinterpret_cast<void *>(i));
        g_nav_icons[i] = label(g_nav_buttons[i], "", &ha_icons_font, 0xAFC6FF);
        ui_theme::set_glyph(g_nav_icons[i], glyph);
        lv_obj_set_width(g_nav_icons[i], button_w);
        lv_obj_set_style_text_align(g_nav_icons[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_pos(g_nav_icons[i], 0, 5);
        g_nav_labels[i] = label(g_nav_buttons[i], module->title(), &lv_font_montserrat_12, MUTED);
        lv_obj_set_width(g_nav_labels[i], button_w);
        lv_obj_set_style_text_align(g_nav_labels[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_pos(g_nav_labels[i], 0, 38);
        x += button_w + gap;
    }
    g_active_index = 0;
    show_module(0);
    update_header();
    g_last_activity_ms = millis();
}
void ui_shell_loop(){const PanelConfig &cfg=config_service_get();const uint32_t now=millis();if(board_take_touch_activity()){g_last_activity_ms=now;if(!board_display_awake())board_set_display_awake(true,cfg.backlight);}if(board_display_awake()&&cfg.screen_timeout_seconds>0&&now-g_last_activity_ms>=cfg.screen_timeout_seconds*1000UL)board_set_display_awake(false,cfg.backlight);if(now-g_last_header_ms>=1000UL){g_last_header_ms=now;update_header();PanelModule *m=module_registry_at(g_active_index);if(m)m->update();}}
void ui_shell_refresh_header(){update_header();}
void ui_shell_report_status(const char *message) {
    if (!message || !message[0]) return;
    remember_status(message);
    g_manual_status_until = millis() + 5000UL;
    update_header();
}

