#include "config_service.h"
#include "display_text.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main() {
    PanelConfig config = {}; String error;
    auto parse = [&](const String &s) { return config_service_parse_room_controls(s, config, error); };
    assert(parse("[]") && config.room_control_count == 0);
    assert(parse(R"([{"entity_id":"light.desk","label":"Desk — warm","placement":1},{"entity_id":"switch.fan","label":"","placement":2}])"));
    assert(config.room_control_count == 2 && config.room_controls[1].placement == 2);
    const auto before = config;
    for (const auto *bad : {"{}", "null", "[1]", "[{}]",
        R"([{"entity_id":"light.a","label":"","placement":3}])",
        R"([{"entity_id":"light.a","label":"","placement":-1}])",
        R"([{"entity_id":"light.a","label":"","placement":"1"}])",
        R"([{"entity_id":"light.a","label":"","placement":1.5}])",
        R"([{"entity_id":"light.a","label":"","placement":true}])",
        R"([{"entity_id":"light.a.b","label":"","placement":0}])",
        R"([{"entity_id":"light.","label":"","placement":0}])",
        R"([{"entity_id":"lock.front","label":"","placement":0}])",
        R"([{"entity_id":"scene.a","label":"","placement":0},{"entity_id":"scene.a","label":"","placement":2}])"}) {
        assert(!parse(bad)); assert(memcmp(&config,&before,sizeof(config))==0);
    }
    auto entries=[](int n,int placement) {
        String json="[";
        for(int i=0;i<n;++i){if(i)json+=",";json+="{\"entity_id\":\"light.l"+std::to_string(i)+"\",\"label\":\"\",\"placement\":"+std::to_string(placement)+"}";}
        return json+"]";
    };
    assert(parse(entries(6,1))); assert(!parse(entries(7,1)));
    assert(parse(entries(48,0))); assert(!parse(entries(49,0)));
    assert(parse("[{\"entity_id\":\"light.a\",\"label\":\""+String(63,'a')+"\",\"placement\":0}]"));
    assert(!parse("[{\"entity_id\":\"light.a\",\"label\":\""+String(64,'a')+"\",\"placement\":0}]"));
    char out[64];
    panel_display_text(out,sizeof(out),"Desk — warm – café"); assert(strcmp(out,"Desk - warm - café")==0);
    panel_display_text(out,sizeof(out),nullptr); assert(out[0]==0);
    panel_display_text(out,1,"anything"); assert(out[0]==0);
    panel_display_text(out,3,"éé"); assert(strcmp(out,"é")==0);
    panel_display_text(out,sizeof(out),"bad\xe2\x80"); assert(strcmp(out,"bad")==0);
    panel_display_text(out,2,"—"); assert(strcmp(out,"-")==0);
    std::cout << "Room parser and UTF-8 regression tests passed.\n";
}
