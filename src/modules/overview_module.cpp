#include "overview_module.h"
#include "config_service.h"
#include "home_assistant.h"
#include "module_ui.h"
#include "network_service.h"
#include <Arduino.h>
#include <string.h>

using namespace module_ui;
namespace {
const char *widget_title(const char *type) {
    if (strcmp(type,"home_status")==0) return "HOME STATUS"; if (strcmp(type,"lights")==0) return "LIGHTS";
    if (strcmp(type,"area")==0) return "AREA"; if (strcmp(type,"network")==0) return "NETWORK";
    if (strcmp(type,"quick_actions")==0) return "QUICK ACTIONS"; if (strcmp(type,"weather")==0) return "WEATHER";
    if (strcmp(type,"calendar")==0) return "CALENDAR"; return "PANEL TIP";
}
const HomeAssistantEntitySnapshot *first_domain(const HomeAssistantEntitySnapshot *items,size_t count,const char *domain) {
    for(size_t i=0;i<count;++i) if(strcmp(items[i].domain,domain)==0) return &items[i]; return nullptr;
}
void set_text(lv_obj_t *object,const char *text){if(object)lv_label_set_text(object,text?text:"");}
bool find_slot(bool occupied[4][4],int span,int height,int &column,int &row) {
    for(row=0;row+height<=4;++row) for(column=0;column+span<=4;++column) {
        bool free=true; for(int y=row;y<row+height;++y) for(int x=column;x<column+span;++x) if(occupied[y][x]) free=false;
        if(!free) continue;
        for(int y=row;y<row+height;++y) for(int x=column;x<column+span;++x) occupied[y][x]=true;
        return true;
    } return false;
}
}

void OverviewModule::create(lv_obj_t *parent) {
    box(parent,BG,0,0); const PanelConfig &cfg=config_service_get(); char sub[120];
    snprintf(sub,sizeof(sub),"Your home at a glance - %s",cfg.display_name); module_ui::title(parent,"Home",sub); add_live_badge(parent);
    bool occupied[4][4]={}; widget_count_=0; action_count_=0; action_status_=nullptr;
    for(uint8_t source=0;source<cfg.overview_widget_count;++source) {
        const PanelOverviewWidget &configured=cfg.overview_widgets[source]; int column=0,row=0;
        if(!find_slot(occupied,configured.span,configured.height,column,row)) continue;
        const int width=configured.span*307-12, height=configured.height*132-12;
        lv_obj_t *tile=card(parent,24+column*307,92+row*132,width,height);
        Widget &widget=widgets_[widget_count_++]; snprintf(widget.type,sizeof(widget.type),"%s",configured.type);
        lv_obj_t *name=label(tile,widget_title(widget.type),&lv_font_montserrat_12,MUTED); lv_obj_set_pos(name,16,13);
        widget.value=label(tile,"Loading…",&lv_font_montserrat_20,TEXT); lv_obj_set_pos(widget.value,16,38); lv_obj_set_width(widget.value,width-32); lv_label_set_long_mode(widget.value,LV_LABEL_LONG_DOT);
        widget.detail=label(tile,"",&lv_font_montserrat_14,MUTED); lv_obj_set_pos(widget.detail,16,height-28); lv_obj_set_width(widget.detail,width-32); lv_label_set_long_mode(widget.detail,LV_LABEL_LONG_DOT);
        if(strcmp(widget.type,"quick_actions")!=0) continue;
        action_status_=widget.detail; action_count_=cfg.overview_quick_action_count;
        const int columns=width>=900?3:width>=590?2:1, button_width=(width-32-(columns-1)*6)/columns;
        for(uint8_t i=0;i<action_count_;++i) {
            QuickAction &action=actions_[i]; const PanelOverviewQuickAction &saved=cfg.overview_quick_actions[i];
            action.owner=this; snprintf(action.entity_id,sizeof(action.entity_id),"%s",saved.entity_id); snprintf(action.type,sizeof(action.type),"%s",saved.type);
            const int action_row=i/columns, action_column=i%columns;
            action.button=button(tile,saved.label,16+action_column*(button_width+6),68+action_row*54,button_width,46,CARD_ALT);
            action.label=lv_obj_get_child(action.button,0); lv_obj_set_style_text_font(action.label,&lv_font_montserrat_14,LV_PART_MAIN);
            lv_obj_add_event_cb(action.button,action_cb,LV_EVENT_CLICKED,&action);
        }
    }
}

void OverviewModule::update() {
    HomeAssistantLightStats lights={}; home_assistant_get_light_stats(lights); HomeAssistantStatus health={}; home_assistant_get_status(health); HomeAssistantDiscoveryStatus discovery={}; home_assistant_get_discovery_status(discovery);
    static HomeAssistantEntitySnapshot entities[HA_MAX_AREA_ENTITIES]={}; const size_t count=home_assistant_get_layout_entities(entities,HA_MAX_AREA_ENTITIES);
    const HomeAssistantEntitySnapshot *weather=first_domain(entities,count,"weather"),*calendar=first_domain(entities,count,"calendar"); const PanelConfig &cfg=config_service_get();
    for(uint8_t i=0;i<widget_count_;++i) { Widget &widget=widgets_[i]; char value[112]={},detail[160]={};
        if(strcmp(widget.type,"home_status")==0){snprintf(value,sizeof(value),"%s",discovery.discovery_complete?"Home Assistant live":health.configured?"Connecting…":"Not configured");snprintf(detail,sizeof(detail),"%u selected entities ready",static_cast<unsigned>(discovery.entity_count));}
        else if(strcmp(widget.type,"lights")==0){snprintf(value,sizeof(value),lights.total?"%u on / %u":"No lights",static_cast<unsigned>(lights.on),static_cast<unsigned>(lights.total));snprintf(detail,sizeof(detail),lights.total?"Selected panel lights":"Add lights in Room configuration");}
        else if(strcmp(widget.type,"area")==0){snprintf(value,sizeof(value),"%s",cfg.area_id[0]?cfg.area_id:"No area");snprintf(detail,sizeof(detail),"%s",cfg.display_name);}
        else if(strcmp(widget.type,"network")==0){snprintf(value,sizeof(value),network_service_connected()?"%d dBm  online":"Offline",network_service_rssi());snprintf(detail,sizeof(detail),network_service_connected()?"Panel network connected":"Check Wi-Fi connection");}
        else if(strcmp(widget.type,"weather")==0){snprintf(value,sizeof(value),"%s",weather?weather->name:"No weather entity");snprintf(detail,sizeof(detail),"%s",weather?weather->state:"Scan and save this layout");}
        else if(strcmp(widget.type,"calendar")==0){snprintf(value,sizeof(value),"%s",calendar?calendar->name:"No calendar entity");snprintf(detail,sizeof(detail),"%s",calendar?calendar->state:"Scan and save this layout");}
        else if(strcmp(widget.type,"quick_actions")==0){snprintf(value,sizeof(value),action_count_?"%u configured actions":"No configured actions",static_cast<unsigned>(action_count_));snprintf(detail,sizeof(detail),"%s",discovery.last_action[0]?discovery.last_action:"Configure actions in the web manager.");}
        else {snprintf(value,sizeof(value),"Make this panel yours");snprintf(detail,sizeof(detail),"Add, remove, reorder, resize, and set widget height in the web manager.");}
        set_text(widget.value,value); if(widget.detail!=action_status_)set_text(widget.detail,detail);
    }
    for(uint8_t i=0;i<action_count_;++i) { QuickAction &action=actions_[i];
        if(strcmp(action.type,"all_lights")==0) { action.bound=lights.total>0; set_enabled(action.button,action.bound); lv_obj_set_style_bg_color(action.button,lv_color_hex(lights.on?ACCENT:CARD_ALT),LV_PART_MAIN); }
        else { HomeAssistantEntitySnapshot current={}; action.bound=home_assistant_get_room_entity(action.entity_id,current)&&current.available; set_enabled(action.button,action.bound); }
    }
}

void OverviewModule::action_cb(lv_event_t *event) {
    auto *action=static_cast<QuickAction *>(lv_event_get_user_data(event)); if(!action||!action->owner||!action->bound)return; bool queued=false;
    if(strcmp(action->type,"all_lights")==0){HomeAssistantLightStats lights={};home_assistant_get_light_stats(lights);queued=lights.total&&home_assistant_queue_all_lights(lights.on==0);}
    else if(strcmp(action->type,"scene")==0) queued=home_assistant_queue_scene(action->entity_id);
    else if(strcmp(action->type,"toggle")==0) queued=home_assistant_queue_toggle(action->entity_id);
    if(action->owner->action_status_)set_text(action->owner->action_status_,queued?"Command queued; waiting for Home Assistant.":"Could not queue Home Assistant command.");
}
