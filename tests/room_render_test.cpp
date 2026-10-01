#include "room_module.h"
#include "config_service.h"
#include "home_assistant.h"
#include <lvgl.h>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <string>
static PanelConfig config = {};
static HomeAssistantEntitySnapshot entities[48] = {};
static size_t count = 0;
static std::string target;
static int toggles=0, brightness_calls=0, fan_speed_calls=0;
static uint8_t fan_speed=0;
const PanelConfig &config_service_get() { return config; }
size_t home_assistant_get_room_entities(HomeAssistantEntitySnapshot *out,size_t capacity) {
    size_t n = count < capacity ? count : capacity;
    memcpy(out,entities,n*sizeof(*out));return n;
}
size_t home_assistant_get_layout_entities(HomeAssistantEntitySnapshot *out,size_t capacity) {
    return home_assistant_get_room_entities(out,capacity);
}
bool home_assistant_get_room_entity(const char *id, HomeAssistantEntitySnapshot &out) {
    for(size_t i=0;i<count;++i) if(strcmp(entities[i].entity_id,id)==0){out=entities[i];return true;}
    return false;
}
void home_assistant_get_discovery_status(HomeAssistantDiscoveryStatus &out) {
    out={};snprintf(out.area_name,sizeof(out.area_name),"Office — upstairs");
    snprintf(out.message,sizeof(out.message),"Connected to Home Assistant");
    out.websocket_authenticated=true;out.discovery_complete=true;
}
bool network_service_connected(){return true;}
void ui_shell_report_status(const char *){}
bool home_assistant_commands_ready(){return true;}
bool home_assistant_queue_toggle(const char *id){target=id;++toggles;return true;}
bool home_assistant_queue_scene(const char *id){target=id;return true;}
bool home_assistant_queue_light_brightness(const char *id,uint8_t){target=id;++brightness_calls;return true;}
bool home_assistant_queue_fan_speed(const char *id,uint8_t speed){target=id;fan_speed=speed;++fan_speed_calls;return true;}
static unsigned char buffer[1280*658*4];
static void flush(lv_display_t *display,const lv_area_t *,uint8_t *){lv_display_flush_ready(display);}
static void shot(const char *name) {
    lv_refr_now(nullptr);
    FILE *file=fopen(name,"wb");assert(file);
    fprintf(file,"P6\n1280 658\n255\n");
    for(int i=0;i<1280*658;++i){fputc(buffer[4*i+2],file);fputc(buffer[4*i+1],file);fputc(buffer[4*i],file);}
    fclose(file);
}
static lv_obj_t *find(lv_obj_t *root,const char *text) {
    if(lv_obj_check_type(root,&lv_label_class) && strcmp(lv_label_get_text(root),text)==0)return root;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i) if(auto *r=find(lv_obj_get_child(root,i),text))return r;
    return nullptr;
}
static lv_obj_t *find_slider(lv_obj_t *root){if(lv_obj_check_type(root,&lv_slider_class))return root;for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto *r=find_slider(lv_obj_get_child(root,i)))return r;return nullptr;}
static lv_obj_t *find_dropdown(lv_obj_t *root){if(lv_obj_check_type(root,&lv_dropdown_class))return root;for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto *r=find_dropdown(lv_obj_get_child(root,i)))return r;return nullptr;}
static void click(lv_obj_t *root,const char *text){auto *label=find(root,text);assert(label);lv_obj_send_event(lv_obj_get_parent(label),LV_EVENT_CLICKED,nullptr);}
static void entity(const char *id,const char *name,const char *domain,const char *state,bool available=true) {
    auto &e=entities[count++];snprintf(e.entity_id,sizeof(e.entity_id),"%s",id);snprintf(e.name,sizeof(e.name),"%s",name);
    snprintf(e.domain,sizeof(e.domain),"%s",domain);snprintf(e.state,sizeof(e.state),"%s",state);e.available=available;
    e.supports_brightness=strcmp(domain,"light")==0;e.brightness_pct=62;
    e.supports_fan_speed=strcmp(domain,"fan")==0;e.fan_speed_pct=66;
}
static void pref(const char *id,const char *label,int placement) {
    auto &p=config.room_controls[config.room_control_count++];snprintf(p.entity_id,sizeof(p.entity_id),"%s",id);
    snprintf(p.label,sizeof(p.label),"%s",label);p.placement=placement;
}
int main() {
    lv_init();auto *display=lv_display_create(1280,658);lv_display_set_color_format(display,LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display,buffer,nullptr,sizeof(buffer),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,flush);
    auto *root=lv_screen_active();config.room_count=2;snprintf(config.rooms[0].tab_label,sizeof(config.rooms[0].tab_label),"Office — upstairs");snprintf(config.rooms[0].header,sizeof(config.rooms[0].header),"Office");snprintf(config.rooms[0].temperature_entity_id,sizeof(config.rooms[0].temperature_entity_id),"sensor.office_temperature");snprintf(config.rooms[0].humidity_entity_id,sizeof(config.rooms[0].humidity_entity_id),"sensor.office_humidity");snprintf(config.rooms[1].tab_label,sizeof(config.rooms[1].tab_label),"Hall");snprintf(config.rooms[1].header,sizeof(config.rooms[1].header),"Hall");
    entity("light.desk","Desk — warm","light","on");entity("light.ceiling","Ceiling","light","off");
    entity("fan.ceiling","Ceiling fan with an intentionally long upstairs office name","fan","on");entity("switch.fan","Desk fan","switch","on");entity("switch.lamp","Desk lamp","switch","off");entity("cover.window","Window shades","cover","open");
    entity("scene.focus","Focus","scene","scening");entity("light.offline","Reading lamp","light","unavailable",false);
    entity("switch.hidden","Hidden control","switch","on");
    entity("sensor.office_temperature","Office temperature","sensor","72");entity("sensor.office_humidity","Office humidity","sensor","45");
    for(int i=0;i<8;++i){std::string id="light.extra"+std::to_string(i);std::string name="Accent light "+std::to_string(i+1);entity(id.c_str(),name.c_str(),"light","off");}
    pref("light.desk","Desk — warm",1);pref("light.ceiling","Ceiling",0);pref("fan.ceiling","Ceiling fan with an intentionally long upstairs office name",1);pref("scene.focus","Focus",1);
    pref("cover.window","Window shades",1);pref("switch.fan","Desk fan",1);pref("switch.lamp","Desk lamp",1);pref("light.missing","Reading lamp",1);pref("switch.hidden","",2);
    for(int i=0;i<8;++i){std::string id="light.extra"+std::to_string(i);pref(id.c_str(),"",0);}
    RoomViewModel model={};RoomControlViewModel model_controls[48]={};size_t model_count=0;assert(ui_state_model_snapshot_room(model,model_controls,48,model_count));assert(model.favorite_count==4);assert(strcmp(model.favorites[0].title,"Desk — warm")==0);
    RoomModule room;room.create(root);room.update();
    lv_obj_update_layout(root);assert(find(root,"Desk - warm"));assert(find(root,"Favorite Controls"));assert(!find(root,"Room Status"));assert(find(root,"72°"));assert(find(root,"45%"));assert(find(root,"1 offline"));auto *room_dropdown=find_dropdown(root);assert(room_dropdown&&lv_dropdown_get_option_count(room_dropdown)==2);
    auto *favorite=lv_obj_get_parent(find(root,"Desk - warm"));assert(lv_obj_get_height(favorite)==132);
    auto *favorite_high=find(root,"High");assert(favorite_high);
    auto *fan_card=lv_obj_get_parent(lv_obj_get_parent(favorite_high));
    auto *fan_title=lv_obj_get_child(fan_card,1);assert(lv_obj_check_type(fan_title,&lv_label_class));
    assert(lv_label_get_long_mode(fan_title)==LV_LABEL_LONG_DOT);
    assert(lv_obj_get_height(fan_title)==lv_font_montserrat_18.line_height);
    assert(!find_slider(fan_card));assert(find(fan_card,"Off")&&find(fan_card,"Low")&&find(fan_card,"Med")&&find(fan_card,"High"));
    shot(".test-build/room-favorites.ppm");
    click(root,"Desk - warm");assert(target=="light.desk");
    click(fan_card,"High");assert(fan_speed_calls==1&&fan_speed==100&&target=="fan.ceiling");
    click(fan_card,"Off");assert(fan_speed_calls==2&&fan_speed==0&&target=="fan.ceiling");
    click(root,"Lights");shot(".test-build/room-lights.ppm");
    auto *overlay=lv_obj_get_child(root,-1);
    auto *sheet=lv_obj_get_child(overlay,0);
    auto *desk=lv_obj_get_parent(find(sheet,"Desk - warm"));
    auto *slider=find_slider(desk);assert(slider);
    assert(!lv_obj_has_flag(slider,LV_OBJ_FLAG_EVENT_BUBBLE));
    auto *device_icon=lv_obj_get_child(desk,0);
    assert(!lv_obj_has_flag(device_icon,LV_OBJ_FLAG_CLICKABLE));
    assert(lv_obj_has_flag(device_icon,LV_OBJ_FLAG_EVENT_BUBBLE));
    lv_obj_send_event(slider,LV_EVENT_PRESSED,nullptr);
    lv_slider_set_value(slider,45,LV_ANIM_OFF);
    room.update();assert(lv_slider_get_value(slider)==45); // Refresh must not fight a drag.
    lv_obj_send_event(slider,LV_EVENT_RELEASED,nullptr);
    assert(brightness_calls==1 && toggles==1 && target=="light.desk");
    lv_obj_send_event(slider,LV_EVENT_PRESSED,nullptr);
    lv_obj_send_event(slider,LV_EVENT_PRESS_LOST,nullptr);
    lv_obj_send_event(slider,LV_EVENT_RELEASED,nullptr);
    assert(brightness_calls==1); // Lost touch never sends a brightness action.
    click(root,"Next");shot(".test-build/room-lights-page2.ppm");assert(find(root,"2 / 2"));
    click(root,"Close");click(root,"Devices");assert(!find(root,"Hidden control"));
    auto *popup_high=find(sheet,"High");assert(popup_high);
    auto *popup_fan=lv_obj_get_parent(lv_obj_get_parent(popup_high));
    assert(!find_slider(popup_fan));
    assert(find(popup_fan,"Off"));
    assert(find(popup_fan,"Low"));
    assert(find(popup_fan,"Med"));
    assert(find(popup_fan,"High"));
    room.on_deactivate();
    config.room_control_count=0;room.update();assert(find(root,"No controls"));shot(".test-build/room-empty.ppm");
    click(root,"Lights");
    for(size_t i=0;i<count;++i) if(strcmp(entities[i].domain,"light")==0)pref(entities[i].entity_id,"",2);
    room.update();assert(find(root,"No controls in this group"));
    auto *group_button=lv_obj_get_parent(find(root,"Lights"));
    assert(lv_obj_has_state(group_button,LV_STATE_DISABLED));
    room.on_deactivate();assert(lv_obj_has_flag(overlay,LV_OBJ_FLAG_HIDDEN));
    lv_dropdown_set_selected(room_dropdown,1);lv_obj_send_event(room_dropdown,LV_EVENT_VALUE_CHANGED,nullptr);assert(ui_state_model_active_room()==1);lv_dropdown_set_selected(room_dropdown,0);lv_obj_send_event(room_dropdown,LV_EVENT_VALUE_CHANGED,nullptr);assert(ui_state_model_active_room()==0);
    puts("Actual LVGL room render and interaction tests passed.");
}

