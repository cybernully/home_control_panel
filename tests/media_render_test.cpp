#include "media_module.h"
#include "config_service.h"
#include "battery_service.h"
#include <lvgl.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
static PanelConfig config = {};
static HomeAssistantMediaSnapshot players[4] = {};
static HomeAssistantMediaFavorite favorites[3] = {};
static size_t player_count=4, favorite_count=3;
static std::vector<uint8_t> cover;
static std::string target, action;
static int volume_calls=0;
const PanelConfig &config_service_get(){return config;}
size_t home_assistant_get_media_players(HomeAssistantMediaSnapshot *out,size_t capacity){size_t n=player_count<capacity?player_count:capacity;memcpy(out,players,n*sizeof(*out));return n;}
size_t home_assistant_get_media_favorites(HomeAssistantMediaFavorite *out,size_t capacity){size_t n=favorite_count<capacity?favorite_count:capacity;memcpy(out,favorites,n*sizeof(*out));return n;}
void home_assistant_get_discovery_status(HomeAssistantDiscoveryStatus &out){out={};snprintf(out.area_name,sizeof(out.area_name),"Office");snprintf(out.area_id,sizeof(out.area_id),"office");}
bool home_assistant_commands_ready(){return true;}
bool home_assistant_request_media_browse(const char *){return true;}
bool home_assistant_request_media_artwork(const char *){return true;}
void home_assistant_get_media_artwork_info(HomeAssistantMediaArtworkInfo &out){out={};out.generation=1;out.data_size=cover.size();out.width=out.height=256;out.format=HomeAssistantArtworkFormat::Jpeg;snprintf(out.entity_id,sizeof(out.entity_id),"media_player.office");}
bool home_assistant_copy_media_artwork(uint8_t *out,size_t capacity,HomeAssistantMediaArtworkInfo &info){if(capacity<cover.size())return false;memcpy(out,cover.data(),cover.size());home_assistant_get_media_artwork_info(info);return true;}
bool home_assistant_queue_media_play_pause(const char *id){target=id;action="play";return true;}
bool home_assistant_queue_media_previous(const char *id){target=id;action="previous";return true;}
bool home_assistant_queue_media_next(const char *id){target=id;action="next";return true;}
bool home_assistant_queue_media_volume(const char *id,uint8_t){target=id;action="volume";++volume_calls;return true;}
bool home_assistant_queue_media_volume_step(const char *id,bool){target=id;action="volume_step";return true;}
bool home_assistant_queue_media_mute(const char *id,bool){target=id;action="mute";return true;}
bool home_assistant_queue_media_source(const char *id,const char *source){target=id;action=source;return true;}
bool home_assistant_queue_media_favorite(const HomeAssistantMediaFavorite &f){target=f.entity_id;action=f.media_content_id;return true;}
bool battery_service_get_status(BatteryStatus &out){out={};out.valid=true;out.percent=78;return true;}
bool network_service_connected(){return true;}
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
static MediaModule media;
static Placeholder overview("Overview"),room("Room"),climate("Climate"),security("Security"),settings("Settings");
static PanelModule *modules[]={&overview,&room,&media,&climate,&security,&settings};
size_t module_registry_count(){return 6;}
PanelModule *module_registry_at(size_t i){return i<6?modules[i]:nullptr;}
#include "../src/ui_shell.cpp"
static unsigned char pixels[1280*800*4];
static void flush(lv_display_t *display,const lv_area_t *,uint8_t *){lv_display_flush_ready(display);}
static void shot(const char *name){lv_refr_now(nullptr);FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n1280 800\n255\n");for(int i=0;i<1280*800;++i){fputc(pixels[4*i+2],f);fputc(pixels[4*i+1],f);fputc(pixels[4*i],f);}fclose(f);}
static lv_obj_t *find(lv_obj_t *root,const char *text){if(lv_obj_has_flag(root,LV_OBJ_FLAG_HIDDEN))return nullptr;if(lv_obj_check_type(root,&lv_label_class)&&strcmp(lv_label_get_text(root),text)==0)return root;for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto *found=find(lv_obj_get_child(root,i),text))return found;return nullptr;}
static lv_obj_t *slider(lv_obj_t *root){if(lv_obj_check_type(root,&lv_slider_class))return root;for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto *found=slider(lv_obj_get_child(root,i)))return found;return nullptr;}
static void click(lv_obj_t *root,const char *text){auto *obj=find(root,text);assert(obj);lv_obj_send_event(lv_obj_get_parent(obj),LV_EVENT_CLICKED,nullptr);}
int main(){ setbuf(stdout,nullptr);
    FILE *f=fopen("tests/fixtures/media-cover.jpg","rb");assert(f);fseek(f,0,SEEK_END);cover.resize(ftell(f));rewind(f);fread(cover.data(),1,cover.size(),f);fclose(f);
    const char *names[]={"Office speaker","Kitchen display","Living room audio","Patio speaker"};
    const char *ids[]={"media_player.office","media_player.kitchen","media_player.living","media_player.patio"};
    for(int i=0;i<4;++i){auto &p=players[i];snprintf(p.entity_id,sizeof(p.entity_id),"%s",ids[i]);snprintf(p.name,sizeof(p.name),"%s",names[i]);snprintf(p.state,sizeof(p.state),"playing");p.available=i!=3;p.supports_volume=p.supports_mute=true;p.volume_pct=32;}
    auto &p=players[0];snprintf(p.title,sizeof(p.title),"Midnight City — Live Session");snprintf(p.artist,sizeof(p.artist),"The Northline Collective");snprintf(p.album,sizeof(p.album),"Night Drive / Vol. 02");snprintf(p.source,sizeof(p.source),"Music");snprintf(p.entity_picture,sizeof(p.entity_picture),"fixture://cover");p.source_count=4;
    const char *sources[]={"Music","Bluetooth","Line in","Radio"};for(int i=0;i<4;++i)snprintf(p.sources[i],sizeof(p.sources[i]),"%s",sources[i]);
    const char *shortcuts[]={"Evening mix","Morning radio","Focus"};
    for(int i=0;i<3;++i){auto &s=config.media_shortcuts[i];snprintf(s.entity_id,sizeof(s.entity_id),"%s",ids[0]);snprintf(s.label,sizeof(s.label),"%s",shortcuts[i]);snprintf(s.media_content_id,sizeof(s.media_content_id),"playlist-%d",i);snprintf(s.media_content_type,sizeof(s.media_content_type),"playlist");
        auto &fav=favorites[i];snprintf(fav.entity_id,sizeof(fav.entity_id),"%s",ids[0]);snprintf(fav.title,sizeof(fav.title),"Browse playlist %d",i+1);snprintf(fav.media_content_id,sizeof(fav.media_content_id),"browse-%d",i);snprintf(fav.media_content_type,sizeof(fav.media_content_type),"playlist");}
    config.media_shortcut_count=3;snprintf(config.display_name,sizeof(config.display_name),"Home Panel");snprintf(config.profile,sizeof(config.profile),"room");snprintf(config.area_id,sizeof(config.area_id),"office");
    lv_init();auto *display=lv_display_create(1280,800);lv_display_set_color_format(display,LV_COLOR_FORMAT_XRGB8888);lv_display_set_buffers(display,pixels,nullptr,sizeof(pixels),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,flush);
    ui_shell_begin();show_module(2);auto *page=g_pages[2];lv_obj_update_layout(lv_screen_active());shot(".test-build/media-refined.ppm");
    auto *track=find(page,"Midnight City - Live Session");assert(track);click(page,"Pause");assert(action=="play"&&target==ids[0]);click(page,"Evening mix");assert(action=="playlist-0");
    auto *volume=slider(page);assert(volume);lv_obj_send_event(volume,LV_EVENT_PRESSED,nullptr);lv_slider_set_value(volume,47,LV_ANIM_OFF);lv_obj_send_event(volume,LV_EVENT_VALUE_CHANGED,nullptr);media.update();assert(lv_slider_get_value(volume)==47&&find(page,"47%"));lv_obj_send_event(volume,LV_EVENT_PRESS_LOST,nullptr);lv_obj_send_event(volume,LV_EVENT_RELEASED,nullptr);assert(volume_calls==0);
    lv_obj_send_event(volume,LV_EVENT_PRESSED,nullptr);lv_slider_set_value(volume,41,LV_ANIM_OFF);lv_obj_send_event(volume,LV_EVENT_RELEASED,nullptr);assert(volume_calls==1&&target==ids[0]);
    click(page,"Players");auto *popup=lv_obj_get_child(page,-1);shot(".test-build/media-players.ppm");click(popup,"Kitchen display");assert(lv_obj_has_flag(popup,LV_OBJ_FLAG_HIDDEN));click(page,"Pause");assert(target==ids[1]);
    click(page,"Players");click(popup,"Office speaker");
    click(page,"Sources");shot(".test-build/media-sources.ppm");click(popup,"Radio");assert(action=="Radio"&&target==ids[0]);click(popup,"Close");
    click(page,"Browse");shot(".test-build/media-browse.ppm");click(popup,"Browse playlist 2");assert(action=="browse-1");media.on_deactivate();assert(lv_obj_has_flag(popup,LV_OBJ_FLAG_HIDDEN));
    snprintf(p.title,sizeof(p.title),"A very long track title that should remain bounded — extended live recording from the evening");snprintf(p.artist,sizeof(p.artist),"A very long artist credit featuring several guests and a live backing ensemble");p.available=false;media.update();shot(".test-build/media-unavailable.ppm");assert(lv_obj_get_height(track)==lv_font_montserrat_28.line_height);assert(lv_obj_has_state(lv_obj_get_parent(find(page,"Pause")),LV_STATE_DISABLED));
    player_count=0;config.media_shortcut_count=0;media.update();shot(".test-build/media-empty.ppm");click(page,"Browse");shot(".test-build/media-empty-browse.ppm");
    puts("Media render/interaction checks passed: artwork, playback, shortcuts, selectors, canceled volume drags, unavailable and empty states.");
}







