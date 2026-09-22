#!/usr/bin/env python3
from pathlib import Path
import json,sys
root=Path(__file__).resolve().parents[1]
required=["platformio.ini","partitions.csv","include/app_config.h","include/board_lvgl.h","include/config_service.h","include/module.h","include/module_registry.h","include/home_assistant.h","include/web_manager.h","src/main.cpp","src/board_lvgl.cpp","src/lvgl_memory.cpp","src/config_service.cpp","src/module_registry.cpp","src/home_assistant.cpp","src/web_manager.cpp","src/hosted_c6_blob.S","data/panel.json"]
missing=[p for p in required if not(root/p).exists()]
if missing:print("Missing:",*missing,sep="\n - ");sys.exit(1)
cfg=json.loads((root/"data/panel.json").read_text());assert cfg["schema"]==1;assert cfg["profile"] in {"calendar","room","whole_home","custom"};assert 1<=len(cfg["modules"])<=8
app=(root/"include/app_config.h").read_text();assert '#define APP_VERSION "1.0.0"' in app;assert '#define HA_HTTP_INTER_REQUEST_GAP_MS 1000UL' in app
main=(root/"src/main.cpp").read_text();assert main.index("ui_shell_begin();")<main.index("network_service_begin();")
pio = (root / "platformio.ini").read_text()
assert "[env:jc8012p4a1c_2624]" in pio
assert "board = esp32-p4\n" in pio
assert "[env:jc8012p4a1c_2635]" in pio
assert "board = esp32-p4_r3" in pio
assert "55.03.39" in pio

print("Home Control Panel v1.0.0 structure validation passed.")
