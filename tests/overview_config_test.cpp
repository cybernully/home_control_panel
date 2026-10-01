#include "config_service.h"
#include <cassert>
#include <cstring>
#include <iostream>

int main() {
    PanelConfig config = {}; String error;
    assert(config_service_parse_overview_widgets(
        R"([{"type":"home_status","span":2,"height":1},{"type":"weather","span":4,"height":2},{"type":"calendar","span":1,"height":1}])",
        config, error));
    assert(config.overview_widget_count == 3);
    assert(config.overview_widgets[1].span == 4);
    assert(config.overview_widgets[1].height == 2);
    const PanelConfig before = config;
    for (const char *bad : {"[]", "{}", R"([{"type":"unknown","span":2}])",
                            R"([{"type":"weather","span":3}])",
                            R"([{"type":"quick_actions","span":4,"height":1}])",
                            R"([{"type":"quick_actions","span":2,"height":2}])",
                            R"([{"type":"weather","span":2},{"type":"weather","span":1}])"}) {
        assert(!config_service_parse_overview_widgets(bad, config, error));
        assert(memcmp(&config, &before, sizeof(config)) == 0);
    }
    config_service_set_overview_defaults(config);
    assert(config.overview_widget_count >= 4);
    assert(config_service_parse_overview_quick_actions(R"([{"label":"All","type":"all_lights","entity_id":""},{"label":"Focus","type":"scene","entity_id":"scene.focus"}])", config, error));
    assert(config.overview_quick_action_count == 2);
    assert(!config_service_parse_overview_quick_actions(R"([{"label":"Bad","type":"scene","entity_id":"light.desk"}])", config, error));
    assert(config_service_parse_overview_items(
        R"([{"type":"home_status","entity_id":"","label":"Home","icon":"shield","action":"none","active_states":"","active_label":"All good","inactive_label":"Attention","color":"green","span":2,"confirm":false},{"type":"entity","entity_id":"cover.garage","label":"Garage","icon":"garage","action":"toggle","active_states":"open,opening","active_label":"Open","inactive_label":"Closed","color":"yellow","span":2,"confirm":true},{"type":"entity","entity_id":"binary_sensor.hall_motion","action_entity_id":"switch.hall_light","label":"Hall motion","icon":"motion","action":"toggle","active_states":"on","active_label":"Motion","inactive_label":"Clear","color":"yellow","span":1,"confirm":false},{"type":"entity","entity_id":"timer.office_energy_saver_countdown","label":"Office energy saver","icon":"timer","action":"none","active_states":"active,paused","active_label":"","inactive_label":"","color":"cyan","span":1,"confirm":false}])",
        config, error));
    assert(config.overview_item_count == 4);
    assert(config.overview_items[1].confirm);
    assert(strcmp(config.overview_items[1].active_states, "open,opening") == 0);
    assert(strcmp(config.overview_items[2].entity_id, "binary_sensor.hall_motion") == 0);
    assert(strcmp(config.overview_items[2].action_entity_id, "switch.hall_light") == 0);
    assert(strcmp(config.overview_items[3].icon, "timer") == 0);
    const PanelConfig items_before = config;
    for (const char *bad : {
             "[]",
             R"([{"type":"entity","entity_id":"cover.garage","label":"Garage","icon":"garage","action":"toggle","active_states":"open","active_label":"Open","inactive_label":"Closed","color":"orange","span":2}])",
             R"([{"type":"entity","entity_id":"binary_sensor.door","label":"Door","icon":"door","action":"toggle","active_states":"on","active_label":"Open","inactive_label":"Closed","color":"yellow","span":1}])",
             R"([{"type":"entity","entity_id":"binary_sensor.door","action_entity_id":"sensor.temperature","label":"Door","icon":"door","action":"toggle","active_states":"on","active_label":"Open","inactive_label":"Closed","color":"yellow","span":1}])",
             R"([{"type":"entity","entity_id":"cover.garage","label":"Garage","icon":"garage","action":"toggle","active_states":"open","active_label":"Open","inactive_label":"Closed","color":"yellow","span":4},{"type":"home_status","label":"Home","icon":"shield","action":"none","color":"green","span":4},{"type":"network","label":"Network","icon":"power","action":"none","color":"cyan","span":4},{"type":"lights","label":"Lights","icon":"light","action":"none","color":"yellow","span":4},{"type":"panel_tip","label":"Tip","icon":"auto","action":"none","color":"cyan","span":1}])"}) {
        assert(!config_service_parse_overview_items(bad, config, error));
        assert(memcmp(&config, &items_before, sizeof(config)) == 0);
    }
    std::cout << "Overview legacy and unified-card parser tests passed.\n";
}
