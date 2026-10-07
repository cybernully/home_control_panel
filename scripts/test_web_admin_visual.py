"""Render the embedded Web Admin Rooms editor in a local headless browser."""
from pathlib import Path
import re
import subprocess
import time

render_started = time.time()

root = Path(__file__).resolve().parents[1]
source = (root / "include/web_ui.h").read_text(encoding="utf-8")
match = re.search(r'R"HTML\((<!doctype html>[\s\S]*?)\)HTML";', source)
if not match:
    raise SystemExit("Embedded Web Admin HTML was not found.")

html = match.group(1)
live_boot = "status();load().catch(e=>say('Could not load configuration: '+e.message,'warn'));setInterval(status,5000);"
fixture_boot = r"""
cfg={media_shortcuts:[],media_favorites:[],climate_devices:[],security_devices:[],security_dynamic_devices:Array.from({length:10},(_,i)=>({entity_id:`binary_sensor.dynamic_${i+1}`,label:`Dynamic sensor ${i+1}`,icon:'alert',abnormal_states:'on',normal_label:'Normal',abnormal_label:'Attention',color:'red',reverse_abnormal:false}))};
modules=['overview','room','media','weather','calendar','climate','security','settings'];
rooms=[{tab_label:'Office',header:'Office'}];activeRoom=0;
candidates=[
 {entity_id:'light.office_office_ceiling_fan_light',name:'Office Ceiling Fan Light',domain:'light',available:true,brightness:true},
 {entity_id:'fan.office_ceiling_fan',name:'Office Ceiling Fan',domain:'fan',available:true},
 {entity_id:'light.office_bureau_lights',name:'Office Bureau Lights',domain:'light',available:true,brightness:true},
 {entity_id:'switch.hall_light',name:'Hall Light',domain:'switch',available:true}
];
roomRows=[
 {...candidates[2],label:'Bureau Lights',placement:1,room_index:0,device_type:'dimmable'},
 {...candidates[1],label:'Ceiling Fan',placement:1,room_index:0,device_type:'fan'}
];
setTimeout(()=>{
 renderRoom();renderCandidates();nav();tab('rooms');
 $('title').textContent='Home Panel';$('ver').textContent='v2.0.2';$('connection').textContent='192.168.1.42 · Home Assistant connected';
 $('filter').value='office ceiling fan';$('candidate_type').value='all';renderCandidates();
 $('scan_state').textContent='Search for office ceiling fan found 2 matching entities.';
 $('config_submit_guard').disabled=false;$('header_save').disabled=false;
 say('Device search is ready. Select a result or enter an exact entity ID.','ok');
},0);
"""
if live_boot not in html:
    raise SystemExit("Web Admin startup hook changed; update the preview fixture.")
html = html.replace(live_boot, fixture_boot)
html = re.sub(r"loadWeatherV193\(\)\.catch\([^\n]+", "", html)

build = root / ".test-build"
build.mkdir(exist_ok=True)
preview = build / "web-admin-v202.html"
screenshot = build / "web-admin-v202.png"
preview.write_text(html, encoding="utf-8")
security_preview = build / "web-admin-v202-security.html"
security_screenshot = build / "web-admin-v202-security.png"
security_preview.write_text(
    html.replace("nav();tab('rooms');", "nav();tab('security');renderSecurity();"),
    encoding="utf-8",
)
admin_preview = build / "web-admin-v202-administration.html"
admin_screenshot = build / "web-admin-v202-administration.png"
admin_preview.write_text(
    html.replace("nav();tab('rooms');", "nav();renderModules();tab('administration');"),
    encoding="utf-8",
)

edge_candidates = [
    Path(r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"),
    Path(r"C:\Program Files\Microsoft\Edge\Application\msedge.exe"),
]
edge = next((path for path in edge_candidates if path.exists()), None)
if not edge:
    raise SystemExit("Microsoft Edge is unavailable; embedded HTML was extracted but not rendered.")

subprocess.run([
    str(edge), f"--user-data-dir={build / 'edge-v202-preview'}", "--no-first-run", "--virtual-time-budget=2000", "--headless=new", "--disable-gpu", "--hide-scrollbars",
    "--window-size=1440,2400", f"--screenshot={screenshot}", preview.as_uri()
], check=True)
subprocess.run([
    str(edge), f"--user-data-dir={build / 'edge-v202-preview'}", "--no-first-run", "--virtual-time-budget=2000", "--headless=new", "--disable-gpu", "--hide-scrollbars",
    "--window-size=1440,1800", f"--screenshot={security_screenshot}", security_preview.as_uri()
], check=True)
subprocess.run([
    str(edge), f"--user-data-dir={build / 'edge-v202-preview'}", "--no-first-run", "--virtual-time-budget=2000", "--headless=new", "--disable-gpu", "--hide-scrollbars",
    "--window-size=1440,2200", f"--screenshot={admin_screenshot}", admin_preview.as_uri()
], check=True)
print(screenshot)
print(security_screenshot)
print(admin_screenshot)

# OTA acknowledged progress and uncertain activation notice for 2.0.2.
for state in ["upload", "unconfirmed"]:
    ota_preview = build / f"web-admin-v202-ota-{state}.html"
    ota_screenshot = build / f"web-admin-v202-ota-{state}.png"
    if state == "upload":
        setup = "setMaintenanceBusy(true);$('firmware_progress').hidden=false;$('firmware_progress').value=81;$('firmware_progress_text').textContent='Received 81% · 2.81 MB of 3.46 MB';maintenanceMessage('Uploading firmware. Keep the panel powered on.');"
    else:
        setup = "$('firmware_progress').hidden=false;$('firmware_progress').value=100;$('firmware_progress_text').textContent='Update result requires confirmation.';maintenanceMessage('The connection closed during activation. The update result is not confirmed. Check the panel firmware version.','warn');"
    ota_preview.write_text(html.replace("nav();tab('rooms');", "nav();renderModules();tab('administration');").replace("say('Device search is ready. Select a result or enter an exact entity ID.','ok');", setup), encoding="utf-8")
    subprocess.run([str(edge), f"--user-data-dir={build / 'edge-v202-preview'}", "--no-first-run", "--virtual-time-budget=2000", "--headless=new", "--disable-gpu", "--hide-scrollbars", "--window-size=1440,2200", f"--screenshot={ota_screenshot}", ota_preview.as_uri()], check=True)
    print(ota_screenshot)

for rendered in [screenshot, security_screenshot, admin_screenshot] + [build / f"web-admin-v202-ota-{state}.png" for state in ["upload", "unconfirmed"]]:
    if not rendered.exists() or rendered.stat().st_size < 1000 or rendered.stat().st_mtime < render_started:
        raise SystemExit(f"Browser did not produce a usable screenshot: {rendered}")
print("Web Admin screenshots generated and checked.")
