#include "room_module.h"
#include "config_service.h"
#include <lvgl.h>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <string>
static PanelConfig config = {};
static HomeAssistantEntitySnapshot entities[48] = {};
static size_t count = 0;
static std::string target;
static int toggles=0, brightness_calls=0;
const PanelConfig &config_service_get() { return config; }
size_t home_assistant_get_room_entities(HomeAssistantEntitySnapshot *out,size_t capacity) {
    size_t n = count < capacity ? count : capacity;
    memcpy(out,entities,n*sizeof(*out));return n;
}
bool home_assistant_get_room_entity(const char *id, HomeAssistantEntitySnapshot &out) {
    for(size_t i=0;i<count;++i) if(strcmp(entities[i].entity_id,id)==0){out=entities[i];return true;}
    return false;
}
void home_assistant_get_discovery_status(HomeAssistantDiscoveryStatus &out) {
    out={};snprintf(out.area_name,sizeof(out.area_name),"Office — upstairs");
    snprintf(out.message,sizeof(out.message),"Connected to Home Assistant");
}
bool home_assistant_commands_ready(){return true;}
bool home_assistant_queue_toggle(const char *id){target=id;++toggles;return true;}
bool home_assistant_queue_scene(const char *id){target=id;return true;}
bool home_assistant_queue_light_brightness(const char *id,uint8_t){target=id;++brightness_calls;return true;}
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
static void click(lv_obj_t *root,const char *text){auto *label=find(root,text);assert(label);lv_obj_send_event(lv_obj_get_parent(label),LV_EVENT_CLICKED,nullptr);}
static void entity(const char *id,const char *name,const char *domain,const char *state,bool available=true) {
    auto &e=entities[count++];snprintf(e.entity_id,sizeof(e.entity_id),"%s",id);snprintf(e.name,sizeof(e.name),"%s",name);
    snprintf(e.domain,sizeof(e.domain),"%s",domain);snprintf(e.state,sizeof(e.state),"%s",state);e.available=available;
    e.supports_brightness=strcmp(domain,"light")==0;e.brightness_pct=62;
}
static void pref(const char *id,const char *label,int placement) {
    auto &p=config.room_controls[config.room_control_count++];snprintf(p.entity_id,sizeof(p.entity_id),"%s",id);
    snprintf(p.label,sizeof(p.label),"%s",label);p.placement=placement;
}
int main() {
    lv_init();auto *display=lv_display_create(1280,658);lv_display_set_color_format(display,LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display,buffer,nullptr,sizeof(buffer),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,flush);
    auto *root=lv_screen_active();
    entity("light.desk","Desk — warm","light","on");entity("light.ceiling","Ceiling","light","off");
    entity("switch.fan","Desk fan","switch","on");entity("cover.window","Window shades","cover","open");
    entity("scene.focus","Focus","scene","scening");entity("light.offline","Reading lamp","light","unavailable",false);
    entity("switch.hidden","Hidden control","switch","on");
    for(int i=0;i<8;++i){std::string id="light.extra"+std::to_string(i);std::string name="Accent light "+std::to_string(i+1);entity(id.c_str(),name.c_str(),"light","off");}
    pref("light.desk","Desk — warm",1);pref("light.ceiling","Ceiling",1);pref("scene.focus","Focus",1);
    pref("cover.window","Window shades",1);pref("switch.fan","Desk fan",1);pref("light.missing","Reading lamp",1);pref("switch.hidden","",2);
    RoomModule room;room.create(root);room.update();
    lv_obj_update_layout(root); assert(find(root,"Office - upstairs"));assert(find(root,"Desk - warm"));
    shot(".test-build/room-favorites.ppm");
    click(root,"Desk - warm");assert(target=="light.desk");
    click(root,"Lights   11");shot(".test-build/room-lights.ppm");
    auto *overlay=lv_obj_get_child(root,-1);
    auto *sheet=lv_obj_get_child(overlay,0);
    auto *desk=lv_obj_get_parent(find(sheet,"Desk - warm"));
    auto *slider=lv_obj_get_child(desk,2);
    assert(!lv_obj_has_flag(slider,LV_OBJ_FLAG_EVENT_BUBBLE));
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
    click(root,"Close");click(root,"Devices   1");assert(!find(root,"Hidden control"));
    room.on_deactivate();
    config.room_control_count=0;room.update();shot(".test-build/room-empty.ppm");
    click(root,"Lights   11");
    for(size_t i=0;i<count;++i) if(strcmp(entities[i].domain,"light")==0)pref(entities[i].entity_id,"",2);
    room.update();assert(find(root,"No visible controls"));
    auto *group_button=lv_obj_get_parent(find(root,"Lights   0"));
    assert(lv_obj_has_state(group_button,LV_STATE_DISABLED));
    room.on_deactivate();assert(lv_obj_has_flag(overlay,LV_OBJ_FLAG_HIDDEN));
    puts("Actual LVGL room render and interaction tests passed.");
}

