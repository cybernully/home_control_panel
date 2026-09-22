Import("env")
from pathlib import Path
import ast,re,urllib.request
PROJECT_DIR=Path(env.subst("$PROJECT_DIR"));LIB_DIR=PROJECT_DIR/"lib"/"JC8012P4A1C_display";SRC_DIR=LIB_DIR/"src"
DRIVER_COMMIT="71cc9af19ef63c1761d1139069e3a1ece6682dfa";DRIVER_BASE=f"https://raw.githubusercontent.com/chipguyhere/JC8012P4A1C_display/{DRIVER_COMMIT}/"
ESPHOME_COMMIT="3e3822e5541f3562fae64b29837b79b1027af674";GUITION_URL=f"https://raw.githubusercontent.com/esphome/esphome/{ESPHOME_COMMIT}/esphome/components/mipi_dsi/models/guition.py"
DRIVER_FILES=["src/chipguy_JC8012P4A1C_display.cpp","src/chipguy_JC8012P4A1C_display.h","src/esp_lcd_jd9365.c","src/esp_lcd_jd9365.h","src/esp_lcd_panel_dpi_bb.c","src/esp_lcd_panel_dpi_bb.h","src/esp_lcd_touch.c","src/esp_lcd_touch.h","src/gsl3680_fw.c","src/gsl3680_fw.h","src/gsl3680_touch.cpp","src/gsl3680_touch.h","src/gsl_point_id.c","src/gsl_point_id.h","src/pins_config.h"]
def fetch(url,binary=False):
    req=urllib.request.Request(url,headers={"User-Agent":"HomePanel-PlatformIO/1.0.0"})
    with urllib.request.urlopen(req,timeout=45) as r:
        data=r.read();return data if binary else data.decode("utf-8")
def stage():
    SRC_DIR.mkdir(parents=True,exist_ok=True)
    for rel in DRIVER_FILES:
        dest=LIB_DIR/rel
        if not dest.exists():dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(fetch(DRIVER_BASE+rel,True))
    (LIB_DIR/"library.json").write_text('{\n  "name":"JC8012P4A1C_display_dual",\n  "version":"1.1.0",\n  "build":{"srcDir":"src"}\n}\n')
def extract(text):
    start=text.find('DsiDriverChip(\n    "JC8012P4A1-V2"')
    if start<0:raise RuntimeError("Pinned ESPHome file does not contain JC8012P4A1-V2")
    pos=text.find("initsequence=[",start);ob=text.find("[",pos);depth=0;cb=None
    for i in range(ob,len(text)):
        if text[i]=="[":depth+=1
        elif text[i]=="]":
            depth-=1
            if depth==0:cb=i;break
    seq=ast.literal_eval(text[ob:cb+1])
    if len(seq)<150:raise RuntimeError("V2 sequence unexpectedly short")
    return seq
def table(seq):
    out=[]
    for item in seq:
        cmd=int(item[0]);data=[int(v) for v in item[1:]];txt=", ".join(f"0x{v:02X}" for v in data);out.append(f"    {{0x{cmd:02X}, (uint8_t[]){{{txt}}}, {len(data)}, 0}},")
    return "\n".join(out)
def patch():
    seq=extract(fetch(GUITION_URL));p=SRC_DIR/"esp_lcd_jd9365.c";src=p.read_text();rx=re.compile(r"const jd9365_lcd_init_cmd_t vendor_specific_init_default\[\] = \{.*?\n\};\nconst size_t vendor_specific_init_default_size",re.S);src,n=rx.subn("const jd9365_lcd_init_cmd_t vendor_specific_init_default[] = {\n"+table(seq)+"\n};\nconst size_t vendor_specific_init_default_size",src,count=1)
    if n!=1:raise RuntimeError("Could not replace JD9365 vendor table")
    p.write_text(src)
    cpp=SRC_DIR/"chipguy_JC8012P4A1C_display.cpp";t=cpp.read_text();

    # Both known panels use the JC8012P4A1-V2 JD9365 initialization/timing
    # already proven on the calendar hardware.  What differs is the ESP32-P4
    # silicon generation: the 2635 is production v3.x, so the DSI PHY reference
    # source must follow the actual chip revision.
    if '#include "esp_chip_info.h"' not in t:
        inc='#include "esp_heap_caps.h"\n'
        if inc not in t: raise RuntimeError("Could not add esp_chip_info.h")
        t=t.replace(inc,inc+'#include "esp_chip_info.h"\n',1)

    old_bus='    esp_lcd_dsi_bus_config_t bus_config = JD9365_PANEL_BUS_DSI_2CH_CONFIG();\n    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));\n'
    new_bus='    esp_lcd_dsi_bus_config_t bus_config = JD9365_PANEL_BUS_DSI_2CH_CONFIG();\n    esp_chip_info_t chip_info = {};\n    esp_chip_info(&chip_info);\n    if (chip_info.revision >= 300U) {\n        bus_config.phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_XTAL;\n        ESP_LOGI(TAG, "ESP32-P4 rev %u.%02u: DSI PHY PLL reference = XTAL",\n                 (unsigned)(chip_info.revision / 100U),\n                 (unsigned)(chip_info.revision % 100U));\n    } else {\n        bus_config.phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M;\n        ESP_LOGI(TAG, "ESP32-P4 rev %u.%02u: DSI PHY PLL reference = PLL_F20M",\n                 (unsigned)(chip_info.revision / 100U),\n                 (unsigned)(chip_info.revision % 100U));\n    }\n    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));\n'
    if old_bus in t:
        t=t.replace(old_bus,new_bus,1)
    elif 'chip_info.revision >= 300U' not in t:
        raise RuntimeError("Could not patch revision-aware DSI PHY clock selection")
    for old,new in {".dpi_clock_freq_mhz = 60,":".dpi_clock_freq_mhz = 70,",".vsync_back_porch = 8,":".vsync_back_porch = 10,"}.items():
        if old in t:t=t.replace(old,new,1)
        elif new not in t:raise RuntimeError(f"Could not patch {old}")
    cpp.write_text(t);h=SRC_DIR/"esp_lcd_jd9365.h";t=h.read_text().replace(".dpi_clock_freq_mhz = 60,",".dpi_clock_freq_mhz = 70,").replace(".vsync_back_porch = 8,",".vsync_back_porch = 10,");h.write_text(t)
    (LIB_DIR/"HOME_PANEL_DISPLAY_PATCH.txt").write_text(f"Home Control Panel 1.0.0\nTargets: JC8012P4A1C_I_W_Y batch 2624 and SKU 10153001-V3 (2635)\nJD9365 profile: ESPHome JC8012P4A1-V2\nPCLK: 70 MHz\nLane rate: 1500 Mbps\nVSYNC: 4/10/20\nDSI PHY PLL: runtime-selected (pre-v3 PLL_F20M; v3+ XTAL)\nDriver commit: {DRIVER_COMMIT}\nESPHome profile commit: {ESPHOME_COMMIT}\n")
stage();patch();print("[HomePanel] Display driver/profile ready")
