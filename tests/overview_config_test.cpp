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
    std::cout << "Overview widget parser tests passed.\n";
}
