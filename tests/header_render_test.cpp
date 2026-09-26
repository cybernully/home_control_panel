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

#include "battery_service.h"
#include "ui_shell.h"
static bool battery_valid=true, connected=true;
static uint8_t battery_percent=78;
bool battery_service_get_status(BatteryStatus &out) { out={};out.valid=battery_valid;out.percent=battery_percent;return battery_valid; }
bool network_service_connected(){return connected;}
int network_service_rssi(){return -61;}
bool board_take_touch_activity(){return false;}
bool board_display_awake(){return true;}
void board_set_display_awake(bool,uint8_t){}
class Placeholder : public PanelModule {
    const char *name_;
public:
    explicit Placeholder(const char *name):name_(name){}
    const char *id() const override{return name_;}
    const char *title() const override{return name_;}
    void create(lv_obj_t *) override{}
    void update() override{}
};
static RoomModule room;
static Placeholder overview("Overview"),media("Media"),climate("Climate"),security("Security"),settings("Settings");
static PanelModule *modules[]={&overview,&room,&media,&climate,&security,&settings};
size_t module_registry_count(){return 6;}
PanelModule *module_registry_at(size_t i){return i<6?modules[i]:nullptr;}
// Include the implementation to inspect battery geometry in this host-only harness.
#include "../src/ui_shell.cpp"
static unsigned char full_buffer[1280*800*4];
static void full_shot(const char *name){
    lv_refr_now(nullptr);FILE *file=fopen(name,"wb");assert(file);
    fprintf(file,"P6\n1280 800\n255\n");
    for(int i=0;i<1280*800;++i){fputc(full_buffer[4*i+2],file);fputc(full_buffer[4*i+1],file);fputc(full_buffer[4*i],file);}
    fclose(file);
}
int main(){
    lv_init();auto *display=lv_display_create(1280,800);lv_display_set_color_format(display,LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display,full_buffer,nullptr,sizeof(full_buffer),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,flush);
    snprintf(config.display_name,sizeof(config.display_name),"Home Panel");snprintf(config.profile,sizeof(config.profile),"room");
    snprintf(config.area_id,sizeof(config.area_id),"Office — upstairs");
    entity("light.desk","Desk — warm","light","on");entity("light.ceiling","Ceiling","light","off");
    entity("scene.focus","Focus","scene","scening");entity("cover.window","Window shades","cover","open");
    entity("switch.fan","Desk fan","switch","on");
    pref("light.desk","",1);pref("light.ceiling","",1);pref("scene.focus","",1);
    pref("cover.window","",1);pref("switch.fan","",1);pref("light.missing","Reading lamp",1);
    ui_shell_begin();show_module(1);lv_obj_update_layout(lv_screen_active());
    full_shot(".test-build/header-refined.ppm");
    lv_area_t body,tip,fill,text;
    lv_obj_get_coords(g_battery_body,&body);lv_obj_get_coords(g_battery_tip,&tip);lv_obj_get_coords(g_battery_label,&text);
    assert(body.y1+body.y2==tip.y1+tip.y2);
    assert(abs((body.y1+body.y2)-(text.y1+text.y2))<=1);
    for(int percent : {100,20,10,1,0}){
        battery_percent=percent;ui_shell_refresh_header();lv_obj_update_layout(lv_screen_active());
        lv_obj_get_coords(g_battery_fill,&fill);
        assert(fill.x1>body.x1 && fill.x2<body.x2 && fill.y1>body.y1 && fill.y2<body.y2);
        assert(lv_obj_has_flag(g_battery_fill,LV_OBJ_FLAG_HIDDEN)==(percent==0));
        char path[100];snprintf(path,sizeof(path),".test-build/header-battery-%d.ppm",percent);full_shot(path);
    }
    battery_percent=78;ui_shell_refresh_header();assert(!lv_obj_has_flag(g_battery_fill,LV_OBJ_FLAG_HIDDEN));
    battery_valid=false;connected=false;ui_shell_refresh_header();
    assert(strcmp(lv_label_get_text(g_battery_label),"--")==0);
    assert(lv_obj_has_flag(g_battery_fill,LV_OBJ_FLAG_HIDDEN));
    assert(strcmp(lv_label_get_text(g_wifi),"Wi-Fi offline")==0);
    full_shot(".test-build/header-offline.ppm");
    snprintf(config.display_name,sizeof(config.display_name),"A deliberately long panel name — upstairs");
    snprintf(config.area_id,sizeof(config.area_id),"A very long area name to check header bounds and alignment");
    ui_shell_refresh_header();full_shot(".test-build/header-long-title.ppm");
    puts("Header renders passed: aligned battery, bounded fill, empty/invalid states, offline status and long labels.");
}


