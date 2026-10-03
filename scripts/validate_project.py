#!/usr/bin/env python3
from pathlib import Path
import json,sys
root=Path(__file__).resolve().parents[1]
required=[
    "platformio.ini","partitions.csv","include/app_config.h","include/board_lvgl.h",
    "include/config_service.h","include/module.h","include/module_registry.h",
    "include/home_assistant.h","include/web_manager.h","src/main.cpp","src/board_lvgl.cpp",
    "src/lvgl_memory.cpp","src/stb_image_impl.cpp","src/ha_icons_font.c","include/ha_icons_font.h","src/config_service.cpp","src/module_registry.cpp","src/runtime_stats.cpp","include/runtime_stats.h",
    "src/home_assistant.cpp","src/web_manager.cpp","src/hosted_c6_blob.S","data/panel.json",
    "src/modules/overview_module.cpp","src/modules/room_module.cpp","src/modules/media_module.cpp","src/modules/weather_module.cpp","src/modules/weather_module.h","src/modules/calendar_module.cpp","src/modules/calendar_module.h",
    "src/ui_state_model.cpp","src/overview_state_model.cpp","src/weather_state_model.cpp","src/calendar_state_model.cpp","src/ui_card.cpp","src/ui_theme.cpp",
    "src/modules/climate_module.cpp","src/modules/security_module.cpp","src/modules/settings_module.cpp",
    "src/security_config.cpp","src/security_state_model.cpp",
    "docs/RELEASE_1.3.0.md","docs/RELEASE_1.3.2.md","docs/RELEASE_1.3.3.md",
    "docs/RELEASE_1.3.4.md","docs/RELEASE_1.3.5.md","docs/RELEASE_1.3.6.md",
    "docs/RELEASE_1.3.7.md","docs/RELEASE_1.3.8.md","docs/RELEASE_1.3.9.md",
    "docs/RELEASE_1.3.10.md", "docs/RELEASE_1.4.0.md", "docs/RELEASE_1.4.1.md", "docs/RELEASE_1.4.2.md", "docs/RELEASE_1.4.3.md", "docs/RELEASE_1.4.4.md", "docs/RELEASE_1.5.0.md", "docs/RELEASE_1.5.2.md", "docs/RELEASE_1.5.3.md", "docs/RELEASE_1.6.0.md", "docs/RELEASE_1.6.1.md", "docs/RELEASE_1.6.2.md", "docs/RELEASE_1.6.3.md", "docs/RELEASE_1.6.4.md", "docs/RELEASE_1.6.5.md", "docs/RELEASE_1.6.6.md", "docs/RELEASE_1.7.0.md", "docs/RELEASE_1.7.1.md", "docs/RELEASE_1.7.2.md", "docs/RELEASE_1.8.0.md", "docs/RELEASE_1.8.1.md", "docs/RELEASE_1.8.2.md", "docs/RELEASE_1.9.0.md", "include/display_text.h", "include/web_ui.h", "include/ui_state_model.h", "include/ui_card.h", "include/ui_theme.h", "src/room_config.cpp", "src/overview_config.cpp",
    "lib/stb/stb_image.h","lib/stb/README.md"
]
missing=[p for p in required if not(root/p).exists()]
if missing:
    print("Missing:",*missing,sep="\n - ");sys.exit(1)
cfg=json.loads((root/"data/panel.json").read_text())
assert cfg["schema"]==7
assert cfg["calendar_days"] in {1,3,7}
assert cfg["profile"] in {"calendar","room","whole_home","custom"}
assert 1<=len(cfg["modules"])<=8
assert isinstance(cfg["media_shortcuts"],list) and len(cfg["media_shortcuts"])<=6
assert isinstance(cfg.get("media_favorites",[]),list) and len(cfg.get("media_favorites",[]))<=6
assert all(len(room.get("status_slots",[]))==4 for room in cfg["rooms"])
app=(root/"include/app_config.h").read_text()
assert '#define APP_VERSION "1.9.0"' in app
assert '#define PANEL_ROOM_STATUS_SLOTS 4' in app
assert '#define APP_LOOP_TASK_STACK_BYTES (16U * 1024U)' in app
assert '#define PANEL_MAX_MEDIA_SHORTCUTS 6' in app
assert '#define PANEL_MAX_MEDIA_FAVORITES 6' in app
assert '#define PANEL_MAX_MEDIA_PLAYERS 6' in app
assert '#define PANEL_MAX_CALENDARS 6' in app
assert '#define PANEL_MAX_SECURITY_DEVICES 8' in app
assert '#define HA_MAX_CALENDAR_EVENTS 48' in app
assert '#define HA_HTTP_INTER_REQUEST_GAP_MS 1000UL' in app
assert '#define HA_COMMAND_MAX_ATTEMPTS 2U' in app
assert '#define HA_COMMAND_RESULT_TIMEOUT_MS 6000UL' in app
assert '#define HA_MAX_MEDIA_PLAYERS 6' in app
assert '#define HA_MEDIA_ARTWORK_MAX_BYTES (1024U * 1024U)' in app
main=(root/"src/main.cpp").read_text()
assert main.index("ui_shell_begin();")<main.index("network_service_begin();")
assert "SET_LOOP_TASK_STACK_SIZE(APP_LOOP_TASK_STACK_BYTES);" in main
pio=(root/"platformio.ini").read_text()
assert "default_envs = jc8012p4a1c_2635" in pio
assert "[env:jc8012p4a1c_2624]" in pio and "board = esp32-p4\n" in pio
assert "[env:jc8012p4a1c_2635]" in pio and "board = esp32-p4_r3" in pio
assert "55.03.37" in pio
assert "links2004/WebSockets@2.7.3" in pio
assert "JPEGDEC.git#430cf789ec96a28fd65c38f0cc3818ab4728fe57" in pio
assert "-DLV_USE_LODEPNG=1" in pio
assert "-DLV_USE_FS_MEMFS=1" not in pio
registry=(root/"src/module_registry.cpp").read_text()
for name in ["OverviewModule","RoomModule","MediaModule","WeatherModule","CalendarModule","ClimateModule","SecurityModule","SettingsModule"]:
    assert name in registry
ha=(root/"src/home_assistant.cpp").read_text()
for feature in ["media_player/browse_media","media_play_pause","volume_set","volume_up",
                "volume_down","select_source","play_media","get_forecasts","get_events","return_response"]:
    assert feature in ha
for feature in ['JsonObject media = doc["media"].to<JsonObject>();',
                'media["media_content_id"]', 'media["media_content_type"]',
                'media["metadata"].to<JsonObject>();']:
    assert feature in ha
media=(root/"src/modules/media_module.cpp").read_text()
for feature in ["home_assistant_get_media_players","home_assistant_request_media_artwork","home_assistant_queue_media_source"]:
    assert feature in media
for feature in ["decode_jpeg_artwork", "decode_progressive_jpeg_artwork",
                "LV_COLOR_FORMAT_RGB565", "set_media_label_text",
                "progressive full", "if (scale > 256U) scale = 256U;",
                "home_assistant_queue_media_volume_step", "lv_obj_has_flag(popup_, LV_OBJ_FLAG_HIDDEN)"]:
    assert feature in media
for feature in ["PANEL_MAX_MEDIA_SHORTCUTS", "PANEL_MAX_MEDIA_FAVORITES", "QUICK PLAY", "Browse favorites",
                "panel_config.media_shortcut_count", "panel_config.media_favorite_count"]:
    assert feature in media
config=(root/"src/config_service.cpp").read_text()
assert 'doc["media_shortcuts"]' in config and 'doc["media_favorites"]' in config
assert 'doc["explicit_layout"]' in config and 'doc["media_players"]' in config
assert 'doc["overview_widgets"]' in config and 'doc["overview_items"]' in config
assert 'doc["security_devices"]' in config and 'doc["alarm_entity_id"]' in config
web=(root/"src/web_manager.cpp").read_text()
for feature in ["shortcut_label_", "shortcut_entity_", "shortcut_id_",
                "shortcut_type_", "favorite_label_", "favorite_entity_", "parse_media_shortcuts", "parse_media_favorites"]:
    assert feature in web
for feature in ["handle_ha_entities", "handle_ha_discover", "parse_media_players", "home_assistant_request_full_discovery"]:
    assert feature in web
web_ui=(root/"include/web_ui.h").read_text()
for feature in ["Panel display manager", "Rooms and controls", "Media experience", "Administration", "Search Home Assistant"]:
    assert feature in web_ui
for feature in ["Overview layout", "Home Assistant entity", "active_states", "overview_items", "overviewDrop"]:
    assert feature in web_ui
for feature in ["Room status bar", "roomStatusDefaults", "status_slots", "binary sensors, timers"]:
    assert feature in web_ui
for feature in ["Calendar tab", "calendar_sources", "calendar_week_starts_monday", "calendar_days", "addCalendar", "moveCalendar"]:
    assert feature in web_ui
for feature in ["Alarmo security", "security_devices", "reverse_abnormal", "security_code_to_arm", "addSecurityDevice"]:
    assert feature in web_ui
assert "home_assistant_request_full_discovery" in ha and "is_layout_entity" in ha
settings=(root/"src/modules/settings_module.cpp").read_text()
for feature in ['#include "network_service.h"', '#include "runtime_stats.h"',
                "network_service_ip()", "SPIFFS.totalBytes()", "ESP.getFreeHeap()",
                "runtime_stats_app_loop_percent()"]:
    assert feature in settings
stb_impl=(root/"src/stb_image_impl.cpp").read_text()
for feature in ["STBI_ONLY_JPEG", "STB_IMAGE_IMPLEMENTATION", "MALLOC_CAP_SPIRAM"]:
    assert feature in stb_impl
ignore=(root/".gitignore").read_text()
assert "include/app_secrets.h" in ignore and "include/appsecrets.h" in ignore
weather=(root/"src/modules/weather_module.cpp").read_text()
for feature in ["HOURLY FORECAST", "MULTI-DAY FORECAST", "weather_layout", "ui_state_model_snapshot_weather"]:
    assert feature in weather
calendar=(root/"src/modules/calendar_module.cpp").read_text()
for feature in ["select_today", "period_offset_", "selected_day_", "EVENT DETAILS", "detail_body_", "ui_state_model_snapshot_calendar"]:
    assert feature in calendar
shell=(root/"src/ui_shell.cpp").read_text()
for feature in ["navigation_glyph", "0xF0A1D", "0xF156D", "0xF0387", "0xF0393", "0xF0CCB", "0xF0595", "0xF0E18"]:
    assert feature in shell
security=(root/"src/modules/security_module.cpp").read_text()
for feature in ["Alarmo protection", "DYNAMIC ATTENTION", "Enter code to", "ui_state_model_security_action"]:
    assert feature in security
for feature in ["AlarmControl", "alarm_arm_home", "alarm_arm_away", "alarm_arm_night", "alarm_arm_vacation", "alarm_disarm"]:
    assert feature in ha
print("Home Control Panel v1.9.0 structure validation passed.")

