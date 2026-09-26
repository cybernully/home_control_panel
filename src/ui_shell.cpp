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
#include <Arduino.h>
#include <lvgl.h>

namespace {
constexpr int SCREEN_W=1280,SCREEN_H=800,HEADER_H=78,FOOTER_H=64,PAGE_Y=HEADER_H,PAGE_H=SCREEN_H-HEADER_H-FOOTER_H;
constexpr uint32_t BG=0x0F172A,PANEL=0x172033,TEXT=0xF8FAFC,MUTED=0x94A3B8,ACCENT=0x2563EB,BORDER=0x334155,SUCCESS=0x4ADE80,WARN=0xFBBF24,BAD=0xF87171;
lv_obj_t *g_screen=nullptr,*g_header_title=nullptr,*g_header_subtitle=nullptr,*g_wifi=nullptr,*g_battery_label=nullptr,*g_battery_body=nullptr,*g_battery_fill=nullptr,*g_battery_tip=nullptr;
lv_obj_t *g_signal_bars[4] = {};
lv_obj_t *g_pages[PANEL_MAX_MODULES]={},*g_nav_buttons[PANEL_MAX_MODULES]={};bool g_created[PANEL_MAX_MODULES]={};size_t g_active_index=0;uint32_t g_last_header_ms=0,g_last_activity_ms=0;
void style_box(lv_obj_t *o,uint32_t bg,int radius=0,int border=0){lv_obj_set_style_bg_color(o,lv_color_hex(bg),LV_PART_MAIN);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,LV_PART_MAIN);lv_obj_set_style_radius(o,radius,LV_PART_MAIN);lv_obj_set_style_border_width(o,border,LV_PART_MAIN);if(border)lv_obj_set_style_border_color(o,lv_color_hex(BORDER),LV_PART_MAIN);lv_obj_set_style_pad_all(o,0,LV_PART_MAIN);lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);}
lv_obj_t *label(lv_obj_t *p,const char *t,const lv_font_t *f,uint32_t c){lv_obj_t *o=lv_label_create(p);lv_label_set_text(o,t?t:"");lv_obj_set_style_text_font(o,f,LV_PART_MAIN);lv_obj_set_style_text_color(o,lv_color_hex(c),LV_PART_MAIN);return o;}
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
    const char *profile = strcmp(cfg.profile, "room") == 0 ? "Room controller" :
                          strcmp(cfg.profile, "whole_home") == 0 ? "Whole home" :
                          strcmp(cfg.profile, "calendar") == 0 ? "Calendar" : "Custom panel";
    HomeAssistantDiscoveryStatus discovery = {};
    home_assistant_get_discovery_status(discovery);
    const bool current_area = strcmp(cfg.area_id, discovery.area_id) == 0 ||
                              strcmp(cfg.area_id, discovery.area_name) == 0;
    const char *area = current_area && discovery.area_name[0] ? discovery.area_name :
                       cfg.area_id[0] ? cfg.area_id : "No area selected";
    char subtitle[192], safe[192];
    snprintf(subtitle, sizeof(subtitle), "%s  /  %s  /  v%s", profile, area, APP_VERSION);
    panel_display_text(safe, sizeof(safe), subtitle);
    lv_label_set_text(g_header_subtitle, safe);

    const bool connected = network_service_connected();
    const int rssi = connected ? network_service_rssi() : -127;
    const int strength = !connected ? 0 : rssi >= -55 ? 4 : rssi >= -67 ? 3 : rssi >= -75 ? 2 : 1;
    lv_label_set_text(g_wifi, connected ? "Wi-Fi online" : "Wi-Fi offline");
    lv_obj_set_style_text_color(g_wifi, lv_color_hex(connected ? TEXT : WARN), LV_PART_MAIN);
    for (int i = 0; i < 4; ++i)
        lv_obj_set_style_bg_color(g_signal_bars[i], lv_color_hex(i < strength ? 0x38BDF8 : BORDER), LV_PART_MAIN);
    update_battery();
}

void create_header() {
    lv_obj_t *header = lv_obj_create(g_screen);
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_set_pos(header, 0, 0);
    style_box(header, 0x121D30);
    // A restrained keyline separates the persistent header from page content.
    lv_obj_t *divider = lv_obj_create(header);
    style_box(divider, 0x263449);
    lv_obj_set_size(divider, SCREEN_W, 1);
    lv_obj_set_pos(divider, 0, HEADER_H - 1);
    g_header_title = label(header, "", &lv_font_montserrat_24, TEXT);
    lv_obj_set_pos(g_header_title, 24, 11);
    lv_obj_set_width(g_header_title, 830);
    lv_label_set_long_mode(g_header_title, LV_LABEL_LONG_DOT);
    g_header_subtitle = label(header, "", &lv_font_montserrat_12, MUTED);
    lv_obj_set_pos(g_header_subtitle, 25, 45);
    lv_obj_set_width(g_header_subtitle, 830);
    lv_label_set_long_mode(g_header_subtitle, LV_LABEL_LONG_DOT);

    lv_obj_t *wifi = lv_obj_create(header);
    lv_obj_set_pos(wifi, 910, 17);
    lv_obj_set_size(wifi, 174, 44);
    style_box(wifi, PANEL, 22, 1);
    for (int i = 0; i < 4; ++i) {
        g_signal_bars[i] = lv_obj_create(wifi);
        style_box(g_signal_bars[i], BORDER, 1);
        const int height = 5 + i * 3;
        lv_obj_set_size(g_signal_bars[i], 3, height);
        lv_obj_set_pos(g_signal_bars[i], 17 + i * 6, 29 - height);
    }
    g_wifi = label(wifi, "Wi-Fi offline", &lv_font_montserrat_16, TEXT);
    lv_obj_align(g_wifi, LV_ALIGN_LEFT_MID, 55, 0);

    lv_obj_t *battery = lv_obj_create(header);
    lv_obj_set_pos(battery, 1100, 17);
    lv_obj_set_size(battery, 156, 44);
    style_box(battery, PANEL, 22, 1);
    lv_obj_t *icon = lv_obj_create(battery);
    style_box(icon, PANEL);
    lv_obj_set_style_bg_opa(icon, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_size(icon, 42, 24);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 16, 0);
    g_battery_body = lv_obj_create(icon);
    style_box(g_battery_body, PANEL, 4, 2);
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
    lv_obj_set_width(g_battery_label, 62);
    lv_obj_set_style_text_align(g_battery_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(g_battery_label, LV_ALIGN_RIGHT_MID, -18, 0);
}
void show_module(size_t index){if(index>=module_registry_count())return;if(g_active_index<module_registry_count()&&g_active_index!=index){PanelModule *old=module_registry_at(g_active_index);if(old)old->on_deactivate();}if(!g_created[index]){PanelModule *m=module_registry_at(index);if(!m)return;m->create(g_pages[index]);g_created[index]=true;}for(size_t i=0;i<module_registry_count();++i){if(i==index)lv_obj_remove_flag(g_pages[i],LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(g_pages[i],LV_OBJ_FLAG_HIDDEN);style_box(g_nav_buttons[i],i==index?ACCENT:PANEL,10,i==index?0:1);}g_active_index=index;PanelModule *m=module_registry_at(index);if(m){m->on_activate();m->update();}}
void nav_cb(lv_event_t *e){const size_t index=reinterpret_cast<size_t>(lv_event_get_user_data(e));show_module(index);}
}

void ui_shell_begin(){g_screen=lv_screen_active();style_box(g_screen,BG);create_header();
    lv_obj_t *footer=lv_obj_create(g_screen);lv_obj_set_size(footer,SCREEN_W,FOOTER_H);lv_obj_set_pos(footer,0,SCREEN_H-FOOTER_H);style_box(footer,PANEL);const size_t count=module_registry_count();const int gap=8;const int available=SCREEN_W-24-static_cast<int>((count>0?count-1:0)*gap);const int button_w=count?available/static_cast<int>(count):available;int x=12;for(size_t i=0;i<count;++i){PanelModule *m=module_registry_at(i);g_pages[i]=lv_obj_create(g_screen);lv_obj_set_size(g_pages[i],SCREEN_W,PAGE_H);lv_obj_set_pos(g_pages[i],0,PAGE_Y);style_box(g_pages[i],BG);lv_obj_add_flag(g_pages[i],LV_OBJ_FLAG_HIDDEN);g_nav_buttons[i]=lv_button_create(footer);lv_obj_set_size(g_nav_buttons[i],button_w,46);lv_obj_set_pos(g_nav_buttons[i],x,9);style_box(g_nav_buttons[i],i==0?ACCENT:PANEL,10,i==0?0:1);lv_obj_add_event_cb(g_nav_buttons[i],nav_cb,LV_EVENT_CLICKED,reinterpret_cast<void *>(i));lv_obj_t *txt=label(g_nav_buttons[i],m->title(),&lv_font_montserrat_14,TEXT);lv_obj_center(txt);x+=button_w+gap;}g_active_index=0;show_module(0);update_header();g_last_activity_ms=millis();}
void ui_shell_loop(){const PanelConfig &cfg=config_service_get();const uint32_t now=millis();if(board_take_touch_activity()){g_last_activity_ms=now;if(!board_display_awake())board_set_display_awake(true,cfg.backlight);}if(board_display_awake()&&cfg.screen_timeout_seconds>0&&now-g_last_activity_ms>=cfg.screen_timeout_seconds*1000UL)board_set_display_awake(false,cfg.backlight);if(now-g_last_header_ms>=1000UL){g_last_header_ms=now;update_header();PanelModule *m=module_registry_at(g_active_index);if(m)m->update();}}
void ui_shell_refresh_header(){update_header();}

