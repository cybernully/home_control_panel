#!/usr/bin/env python3
from pathlib import Path
import json,sys
root=Path(__file__).resolve().parents[1]
required=[
    "platformio.ini","partitions.csv","include/app_config.h","include/board_lvgl.h",
    "include/config_service.h","include/module.h","include/module_registry.h",
    "include/home_assistant.h","include/web_manager.h","src/main.cpp","src/board_lvgl.cpp",
    "src/lvgl_memory.cpp","src/config_service.cpp","src/module_registry.cpp",
    "src/home_assistant.cpp","src/web_manager.cpp","src/hosted_c6_blob.S","data/panel.json",
    "src/modules/overview_module.cpp","src/modules/room_module.cpp","src/modules/media_module.cpp",
    "src/modules/climate_module.cpp","src/modules/security_module.cpp","src/modules/settings_module.cpp",
    "docs/RELEASE_1.3.0.md"
]
missing=[p for p in required if not(root/p).exists()]
if missing:
    print("Missing:",*missing,sep="\n - ");sys.exit(1)
cfg=json.loads((root/"data/panel.json").read_text())
assert cfg["schema"]==1
assert cfg["profile"] in {"calendar","room","whole_home","custom"}
assert 1<=len(cfg["modules"])<=8
app=(root/"include/app_config.h").read_text()
assert '#define APP_VERSION "1.3.0"' in app
assert '#define HA_HTTP_INTER_REQUEST_GAP_MS 1000UL' in app
assert '#define HA_MAX_MEDIA_PLAYERS 4' in app
assert '#define HA_MEDIA_ARTWORK_MAX_BYTES (256U * 1024U)' in app
main=(root/"src/main.cpp").read_text()
assert main.index("ui_shell_begin();")<main.index("network_service_begin();")
pio=(root/"platformio.ini").read_text()
assert "default_envs = jc8012p4a1c_2635" in pio
assert "[env:jc8012p4a1c_2624]" in pio and "board = esp32-p4\n" in pio
assert "[env:jc8012p4a1c_2635]" in pio and "board = esp32-p4_r3" in pio
assert "55.03.39" in pio
assert "links2004/WebSockets@2.7.3" in pio
assert "-DLV_USE_TJPGD=1" in pio and "-DLV_USE_LODEPNG=1" in pio
registry=(root/"src/module_registry.cpp").read_text()
for name in ["OverviewModule","RoomModule","MediaModule","ClimateModule","SecurityModule","SettingsModule"]:
    assert name in registry
ha=(root/"src/home_assistant.cpp").read_text()
for feature in ["media_player/browse_media","media_play_pause","volume_set","select_source","play_media"]:
    assert feature in ha
media=(root/"src/modules/media_module.cpp").read_text()
for feature in ["home_assistant_get_media_players","home_assistant_request_media_artwork","home_assistant_queue_media_source"]:
    assert feature in media
ignore=(root/".gitignore").read_text()
assert "include/app_secrets.h" in ignore and "include/appsecrets.h" in ignore
print("Home Control Panel v1.3.0 structure validation passed.")
