#!/usr/bin/env python3
from pathlib import Path
import json,sys
root=Path(__file__).resolve().parents[1]
required=[
    "platformio.ini","partitions.csv","include/app_config.h","include/board_lvgl.h",
    "include/config_service.h","include/module.h","include/module_registry.h",
    "include/home_assistant.h","include/web_manager.h","src/main.cpp","src/board_lvgl.cpp",
    "src/lvgl_memory.cpp","src/stb_image_impl.cpp","src/config_service.cpp","src/module_registry.cpp",
    "src/home_assistant.cpp","src/web_manager.cpp","src/hosted_c6_blob.S","data/panel.json",
    "src/modules/overview_module.cpp","src/modules/room_module.cpp","src/modules/media_module.cpp",
    "src/modules/climate_module.cpp","src/modules/security_module.cpp","src/modules/settings_module.cpp",
    "docs/RELEASE_1.3.0.md","docs/RELEASE_1.3.2.md","docs/RELEASE_1.3.3.md",
    "docs/RELEASE_1.3.4.md","docs/RELEASE_1.3.5.md","docs/RELEASE_1.3.6.md",
    "docs/RELEASE_1.3.7.md","docs/RELEASE_1.3.8.md","docs/RELEASE_1.3.9.md",
    "lib/stb/stb_image.h","lib/stb/README.md"
]
missing=[p for p in required if not(root/p).exists()]
if missing:
    print("Missing:",*missing,sep="\n - ");sys.exit(1)
cfg=json.loads((root/"data/panel.json").read_text())
assert cfg["schema"]==1
assert cfg["profile"] in {"calendar","room","whole_home","custom"}
assert 1<=len(cfg["modules"])<=8
assert isinstance(cfg["media_shortcuts"],list) and len(cfg["media_shortcuts"])<=3
app=(root/"include/app_config.h").read_text()
assert '#define APP_VERSION "1.3.9"' in app
assert '#define APP_LOOP_TASK_STACK_BYTES (16U * 1024U)' in app
assert '#define PANEL_MAX_MEDIA_SHORTCUTS 3' in app
assert '#define HA_HTTP_INTER_REQUEST_GAP_MS 1000UL' in app
assert '#define HA_MAX_MEDIA_PLAYERS 4' in app
assert '#define HA_MEDIA_ARTWORK_MAX_BYTES (256U * 1024U)' in app
main=(root/"src/main.cpp").read_text()
assert main.index("ui_shell_begin();")<main.index("network_service_begin();")
assert "SET_LOOP_TASK_STACK_SIZE(APP_LOOP_TASK_STACK_BYTES);" in main
pio=(root/"platformio.ini").read_text()
assert "default_envs = jc8012p4a1c_2635" in pio
assert "[env:jc8012p4a1c_2624]" in pio and "board = esp32-p4\n" in pio
assert "[env:jc8012p4a1c_2635]" in pio and "board = esp32-p4_r3" in pio
assert "55.03.39" in pio
assert "links2004/WebSockets@2.7.3" in pio
assert "JPEGDEC.git#430cf789ec96a28fd65c38f0cc3818ab4728fe57" in pio
assert "-DLV_USE_LODEPNG=1" in pio
assert "-DLV_USE_FS_MEMFS=1" not in pio
registry=(root/"src/module_registry.cpp").read_text()
for name in ["OverviewModule","RoomModule","MediaModule","ClimateModule","SecurityModule","SettingsModule"]:
    assert name in registry
ha=(root/"src/home_assistant.cpp").read_text()
for feature in ["media_player/browse_media","media_play_pause","volume_set","volume_up",
                "volume_down","select_source","play_media"]:
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
                "home_assistant_queue_media_volume_step"]:
    assert feature in media
for feature in ["PANEL_MAX_MEDIA_SHORTCUTS", "Media shortcuts", "Browse favorites",
                "panel_config.media_shortcut_count"]:
    assert feature in media
config=(root/"src/config_service.cpp").read_text()
assert 'doc["media_shortcuts"]' in config
web=(root/"src/web_manager.cpp").read_text()
for feature in ["shortcut_label_", "shortcut_entity_", "shortcut_id_",
                "shortcut_type_", "parse_media_shortcuts"]:
    assert feature in web
stb_impl=(root/"src/stb_image_impl.cpp").read_text()
for feature in ["STBI_ONLY_JPEG", "STB_IMAGE_IMPLEMENTATION", "MALLOC_CAP_SPIRAM"]:
    assert feature in stb_impl
ignore=(root/".gitignore").read_text()
assert "include/app_secrets.h" in ignore and "include/appsecrets.h" in ignore
print("Home Control Panel v1.3.9 structure validation passed.")
