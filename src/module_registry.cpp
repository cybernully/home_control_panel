#include "module_registry.h"
#include "config_service.h"
#include "modules/climate_module.h"
#include "modules/info_module.h"
#include "modules/media_module.h"
#include "modules/overview_module.h"
#include "modules/room_module.h"
#include "modules/security_module.h"
#include "modules/settings_module.h"
#include <string.h>

namespace {
OverviewModule g_overview;
InfoModule g_calendar("calendar", "Calendar", "Calendar module boundary. Family Calendar remains a separate project; this panel will later consume selected calendar data without replacing that application.");
InfoModule g_weather("weather", "Weather", "Weather module boundary for current conditions and forecasts from Home Assistant.");
RoomModule g_room;
InfoModule g_rooms("rooms", "All Rooms", "Whole-home room browser placeholder. Area discovery and entity binding are the next implementation step.");
MediaModule g_media;
ClimateModule g_climate;
SecurityModule g_security;
SettingsModule g_settings;
PanelModule *g_known[] = {&g_overview,&g_calendar,&g_weather,&g_room,&g_rooms,&g_media,&g_climate,&g_security,&g_settings};
PanelModule *g_active[PANEL_MAX_MODULES] = {};
size_t g_active_count = 0;
}

void module_registry_begin() {
    g_active_count = 0;
    const PanelConfig &cfg = config_service_get();
    for (uint8_t i = 0; i < cfg.module_count && g_active_count < PANEL_MAX_MODULES; ++i) {
        for (PanelModule *candidate : g_known) {
            if (strcmp(candidate->id(), cfg.modules[i]) == 0) {
                g_active[g_active_count++] = candidate;
                break;
            }
        }
    }
    if (g_active_count == 0) g_active[g_active_count++] = &g_settings;
    Serial0.printf("[Modules] %u active modules\n", static_cast<unsigned>(g_active_count));
}

size_t module_registry_count() { return g_active_count; }
PanelModule *module_registry_at(size_t index) { return index < g_active_count ? g_active[index] : nullptr; }
PanelModule *module_registry_find(const char *id) {
    if (!id) return nullptr;
    for (size_t i = 0; i < g_active_count; ++i) {
        if (strcmp(g_active[i]->id(), id) == 0) return g_active[i];
    }
    return nullptr;
}
