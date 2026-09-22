#include "module_registry.h"
#include "config_service.h"
#include "modules/info_module.h"
#include <string.h>

namespace {
InfoModule g_overview("overview", "Overview", "Profile-aware landing page for scenes, presence and high-value controls.");
InfoModule g_calendar("calendar", "Calendar", "Calendar module boundary. The existing Family Calendar remains a separate project and can continue evolving independently.");
InfoModule g_weather("weather", "Weather", "Weather module boundary for current conditions and forecasts from Home Assistant.");
InfoModule g_room("room", "Room Controls", "Future area-driven light, switch, fan, cover, lock and scene controls.");
InfoModule g_rooms("rooms", "All Rooms", "Future whole-home room browser generated from Home Assistant areas.");
InfoModule g_media("media", "Media", "Future Home Assistant media-player controller for playback, volume, source, favorites and artwork.");
InfoModule g_climate("climate", "Climate", "Future thermostat, room temperature and humidity controls.");
InfoModule g_security("security", "Security", "Future Alarmo, lock, garage and active/open sensor summary.");
InfoModule g_settings("settings", "Settings", "Profile, area, module and Home Assistant configuration is managed from the local web interface in v1.0.0.");
PanelModule *g_known[] = {&g_overview,&g_calendar,&g_weather,&g_room,&g_rooms,&g_media,&g_climate,&g_security,&g_settings};
PanelModule *g_active[PANEL_MAX_MODULES] = {};
size_t g_active_count = 0;
}

void module_registry_begin() {
    g_active_count = 0;
    const PanelConfig &cfg = config_service_get();
    for (uint8_t i = 0; i < cfg.module_count && g_active_count < PANEL_MAX_MODULES; ++i) {
        for (PanelModule *candidate : g_known) if (strcmp(candidate->id(), cfg.modules[i]) == 0) { g_active[g_active_count++] = candidate; break; }
    }
    if (g_active_count == 0) g_active[g_active_count++] = &g_settings;
    Serial0.printf("[Modules] %u active modules\n", static_cast<unsigned>(g_active_count));
}
size_t module_registry_count() { return g_active_count; }
PanelModule *module_registry_at(size_t index) { return index < g_active_count ? g_active[index] : nullptr; }
PanelModule *module_registry_find(const char *id) {
    if (!id) return nullptr;
    for (size_t i = 0; i < g_active_count; ++i) if (strcmp(g_active[i]->id(), id) == 0) return g_active[i];
    return nullptr;
}
