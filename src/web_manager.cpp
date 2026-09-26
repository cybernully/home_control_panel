#include <memory>
#include <algorithm>
#include <new>
#include "web_manager.h"

#include "app_config.h"
#include "battery_service.h"
#include "config_service.h"
#include "home_assistant.h"
#include "network_service.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <string.h>

namespace {

WebServer g_server(WEB_MANAGER_PORT);
bool g_routes_registered = false;
bool g_mdns_started = false;
uint32_t g_last_mdns_attempt_ms = 0;
uint32_t g_reboot_at_ms = 0;

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Home Control Panel</title>
<style>
:root{color-scheme:dark;font-family:system-ui,-apple-system,sans-serif}
*{box-sizing:border-box}body{margin:0;background:#0f172a;color:#e5e7eb}
.wrap{max-width:1000px;margin:auto;padding:24px}
.card{background:#172033;border:1px solid #334155;border-radius:14px;padding:18px;margin-bottom:16px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px}
.stat,.shortcut{background:#111827;border-radius:10px;padding:12px}
.shortcut{border:1px solid #334155;margin-bottom:12px}
.k{color:#94a3b8;font-size:.82rem}.v{margin-top:4px;font-weight:650;word-break:break-word}
label{display:block;margin:.75rem 0 .3rem;color:#cbd5e1}
input,select,button{font:inherit;border-radius:8px;border:1px solid #475569;padding:10px;background:#0f172a;color:#f8fafc}
input,select{width:100%}button{background:#2563eb;border:0;cursor:pointer;font-weight:650}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
.muted{color:#94a3b8;font-size:.9rem}.result{white-space:pre-wrap;background:#111827;border-radius:8px;padding:10px;margin-top:10px}
h3{margin-top:24px}
</style>
</head>
<body>
<div class="wrap">
<h1>Home Control Panel <span id="ver" class="muted"></span></h1>
<div class="card">
  <h2>Device status</h2>
  <div id="stats" class="grid"></div>
  <div class="row" style="margin-top:14px">
    <button onclick="testHA()">Test Home Assistant</button>
    <button onclick="reboot()">Reboot</button>
  </div>
  <div id="msg" class="result muted">Ready.</div>
</div>
<div class="card">
  <h2>Panel configuration</h2>
  <form id="cfg">
    <div class="grid">
      <div><label>Device ID / hostname</label><input id="device_id" maxlength="31"></div>
      <div><label>Display name</label><input id="display_name" maxlength="47"></div>
      <div><label>Profile</label><select id="profile"><option value="calendar">Calendar</option><option value="room">Room controller</option><option value="whole_home">Whole home</option><option value="custom">Custom</option></select></div>
      <div><label>Home Assistant area ID</label><input id="area_id" maxlength="63" placeholder="living_room"></div>
      <div><label>Modules (comma separated)</label><input id="modules" placeholder="overview,room,media,climate,security,settings"></div>
      <div><label>Backlight</label><input id="backlight" type="number" min="10" max="100"></div>
      <div><label>Screen timeout seconds</label><input id="timeout" type="number" min="0" max="3600"></div>
    </div>
    <h3>Home Assistant</h3>
    <div class="grid">
      <div><label>Base URL</label><input id="ha_url" placeholder="https://homeassistant.local:8123"></div>
      <div><label>Access token</label><input id="ha_token" type="password" placeholder="Leave blank to keep existing token"></div>
    </div>
    <h3>Media shortcuts</h3>
    <p class="muted">Add up to three one-touch actions for the Media tab. Fill all four fields in a slot, then Save configuration. The media player must be assigned to this panel's Home Assistant area.</p>
    <div id="shortcut_fields"></div>
    <h3>Room controls</h3>
    <p class="muted">Choose up to six favorites. Grouped controls live in Lights, Devices, Shades or Scenes. Hidden controls disappear only from this panel's Room screen; scenes and other dashboards can still operate them. Use arrows to set order. Names may use up to 63 UTF-8 bytes.</p>
    <button type="button" onclick="refreshRoom()">Refresh discovered controls</button>
    <div id="room_fields" style="margin-top:12px"></div>
    <p id="room_hint" class="muted"></p>
    <p class="muted">The token is stored in NVS and is never returned to this page. Media shortcuts update after saving; profile and module changes require a reboot.</p>
    <button type="submit" disabled>Save configuration</button>
  </form>
</div>
</div>
<script>
const $=id=>document.getElementById(id);
async function j(url,opt){const r=await fetch(url,opt);let x={};try{x=await r.json()}catch(e){}if(!r.ok)throw new Error(x.error||('HTTP '+r.status));return x}
function esc(s){return String(s??'').replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function shortcutFields(i,s={}){return `<div class="shortcut"><strong>Shortcut ${i+1}</strong><div class="grid"><div><label>Button label</label><input id="shortcut_label_${i}" maxlength="39" value="${esc(s.label)}" placeholder="Skiing"></div><div><label>Media player entity</label><input id="shortcut_entity_${i}" maxlength="95" value="${esc(s.entity_id)}" placeholder="media_player.office_echo_studio"></div><div><label>Media content ID</label><input id="shortcut_id_${i}" maxlength="191" value="${esc(s.media_content_id)}" placeholder="play my skiing playlist"></div><div><label>Media content type</label><input id="shortcut_type_${i}" maxlength="47" value="${esc(s.media_content_type)}" placeholder="AMAZON_MUSIC"></div></div></div>`}
let roomRows=[];
function readRoom(){roomRows.forEach((r,i)=>{r.label=$('room_label_'+i).value;r.placement=Number($('room_place_'+i).value)})}
function renderRoom(){
 $('room_fields').innerHTML=roomRows.map((r,i)=>`<div class="shortcut"><strong>${esc(r.name||r.entity_id)}</strong><div class="muted">${esc(r.entity_id)}${r.discovered?'':' — not currently discovered'}</div><div class="grid"><div><label for="room_label_${i}">Display name</label><input id="room_label_${i}" maxlength="63" value="${esc(r.label)}" placeholder="Use Home Assistant name"></div><div><label for="room_place_${i}">Placement</label><select id="room_place_${i}">${['Grouped','Favorite','Hidden'].map((n,v)=>`<option value="${v}" ${r.placement===v?'selected':''}>${n}</option>`).join('')}</select></div><div class="row"><button type="button" aria-label="Move ${esc(r.name||r.entity_id)} up" onclick="moveRoom(${i},-1)" ${i===0?'disabled':''}>Up</button><button type="button" aria-label="Move ${esc(r.name||r.entity_id)} down" onclick="moveRoom(${i},1)" ${i===roomRows.length-1?'disabled':''}>Down</button><button type="button" onclick="resetRoom(${i})">Reset</button></div></div></div>`).join('');
 $('room_hint').textContent=roomRows.length?'Room changes apply after saving. Reset restores the HA name and grouped placement.':'No room controls discovered yet. Check the area and Home Assistant connection, then refresh.';
}
function moveRoom(i,d){readRoom();const j=i+d;if(j<0||j>=roomRows.length)return;[roomRows[i],roomRows[j]]=[roomRows[j],roomRows[i]];renderRoom()}
function resetRoom(i){readRoom();if(!roomRows[i].discovered)roomRows.splice(i,1);else{roomRows[i].label='';roomRows[i].placement=0}renderRoom()}
async function discoverRoom(){const entities=await j('/api/room/entities');roomRows.forEach(r=>r.discovered=false);entities.forEach(e=>{const old=roomRows.find(r=>r.entity_id===e.entity_id);if(old){old.name=e.name;old.discovered=true}else roomRows.push({...e,label:'',placement:0,discovered:true})});renderRoom()}
async function refreshRoom(){readRoom();try{await discoverRoom()}catch(e){$('msg').textContent=e.message}}
async function status(){try{const s=await j('/api/status');$('ver').textContent='v'+s.version;const p=[['Device',s.device_id],['Profile',s.profile],['Area',s.area||'(none)'],['IP',s.ip],['RSSI',s.rssi+' dBm'],['Battery',s.battery_valid?(s.battery_percent+'% / '+Number(s.battery_voltage).toFixed(3)+' V'):'unavailable'],['Home Assistant',s.ha_message],['Modules',s.modules]];$('stats').innerHTML=p.map(x=>`<div class="stat"><div class="k">${esc(x[0])}</div><div class="v">${esc(x[1])}</div></div>`).join('')}catch(e){}}
async function load(){const c=await j('/api/config');for(const id of ['device_id','display_name','profile','area_id','modules','backlight','timeout','ha_url'])$(id).value=c[id]??'';roomRows=(c.room_controls||[]).map(r=>({...r,discovered:false}));renderRoom();await discoverRoom();const shortcuts=c.media_shortcuts||[];$('shortcut_fields').innerHTML=Array.from({length:3},(_,i)=>shortcutFields(i,shortcuts[i]||{})).join('');$('cfg').querySelector('button[type=submit]').disabled=false}
$('cfg').addEventListener('submit',async e=>{e.preventDefault();const p=new URLSearchParams();for(const id of ['device_id','display_name','profile','area_id','modules','backlight','timeout','ha_url','ha_token'])p.set(id,$(id).value);for(let i=0;i<3;i++){p.set(`shortcut_label_${i}`,$(`shortcut_label_${i}`).value);p.set(`shortcut_entity_${i}`,$(`shortcut_entity_${i}`).value);p.set(`shortcut_id_${i}`,$(`shortcut_id_${i}`).value);p.set(`shortcut_type_${i}`,$(`shortcut_type_${i}`).value)}readRoom();p.set('room_controls',JSON.stringify(roomRows.map(({entity_id,label,placement})=>({entity_id,label,placement}))));try{const r=await j('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:p});$('msg').textContent=r.message;$('ha_token').value=''}catch(e){$('msg').textContent=e.message}});
async function testHA(){try{const r=await j('/api/ha/test',{method:'POST'});$('msg').textContent=r.queued?'HA test queued on the single worker.':'HA test could not be queued.'}catch(e){$('msg').textContent=e.message}}
async function reboot(){try{await j('/api/reboot',{method:'POST'});$('msg').textContent='Rebootingâ€¦'}catch(e){$('msg').textContent=e.message}}
status();load().catch(e=>{$('msg').textContent='Could not load configuration: '+e.message;$('cfg').querySelector('button[type=submit]').disabled=true});setInterval(status,5000);
</script>
</body>
</html>)HTML";

bool ensure_auth() {
    if (g_server.authenticate(WEB_MANAGER_USER, WEB_MANAGER_PASSWORD)) return true;
    g_server.requestAuthentication();
    return false;
}

void send_json(JsonDocument &doc, int code = 200) {
    String payload;
    serializeJson(doc, payload);
    g_server.send(code, "application/json", payload);
}

void send_error(int code, const char *message) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = message;
    send_json(doc, code);
}

void handle_status() {
    if (!ensure_auth()) return;
    const PanelConfig &cfg = config_service_get();
    HomeAssistantStatus ha = {};
    home_assistant_get_status(ha);
    BatteryStatus battery = {};
    const bool battery_ok = battery_service_get_status(battery);

    JsonDocument doc;
    doc["app"] = APP_NAME;
    doc["version"] = APP_VERSION;
    doc["device_id"] = cfg.device_id;
    doc["profile"] = cfg.profile;
    doc["area"] = cfg.area_id;
    doc["modules"] = config_service_modules_csv();
    doc["ip"] = network_service_connected() ? WiFi.localIP().toString() : String("offline");
    doc["rssi"] = network_service_connected() ? WiFi.RSSI() : 0;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["free_psram"] = ESP.getFreePsram();
    doc["battery_valid"] = battery_ok;
    doc["battery_percent"] = battery.percent;
    doc["battery_voltage"] = battery.voltage_v;
    doc["ha_configured"] = ha.configured;
    doc["ha_authenticated"] = ha.authenticated;
    doc["ha_message"] = ha.message;
    doc["ha_last_success_ms"] = ha.last_success_ms;
    send_json(doc);
}

void handle_get_config() {
    if (!ensure_auth()) return;
    const PanelConfig &cfg = config_service_get();
    JsonDocument doc;
    doc["device_id"] = cfg.device_id;
    doc["display_name"] = cfg.display_name;
    doc["profile"] = cfg.profile;
    doc["area_id"] = cfg.area_id;
    doc["modules"] = config_service_modules_csv();
    doc["backlight"] = cfg.backlight;
    doc["timeout"] = cfg.screen_timeout_seconds;
    doc["ha_url"] = home_assistant_base_url();
    doc["ha_token_configured"] = home_assistant_token_configured();
    JsonArray shortcuts = doc["media_shortcuts"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.media_shortcut_count; ++i) {
        JsonObject item = shortcuts.add<JsonObject>();
        item["label"] = cfg.media_shortcuts[i].label;
        item["entity_id"] = cfg.media_shortcuts[i].entity_id;
        item["media_content_id"] = cfg.media_shortcuts[i].media_content_id;
        item["media_content_type"] = cfg.media_shortcuts[i].media_content_type;
    }
    JsonArray room = doc["room_controls"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.room_control_count; ++i) {
        JsonObject item = room.add<JsonObject>();
        item["entity_id"] = cfg.room_controls[i].entity_id;
        item["label"] = cfg.room_controls[i].label;
        item["placement"] = cfg.room_controls[i].placement;
    }
    send_json(doc);
}

bool parse_media_shortcuts(PanelConfig &config, String &error) {
    config.media_shortcut_count = 0;
    memset(config.media_shortcuts, 0, sizeof(config.media_shortcuts));

    for (uint8_t i = 0; i < PANEL_MAX_MEDIA_SHORTCUTS; ++i) {
        char field_name[32] = {};
        snprintf(field_name, sizeof(field_name), "shortcut_label_%u",
                 static_cast<unsigned>(i));
        String label = g_server.arg(field_name);
        snprintf(field_name, sizeof(field_name), "shortcut_entity_%u",
                 static_cast<unsigned>(i));
        String entity = g_server.arg(field_name);
        snprintf(field_name, sizeof(field_name), "shortcut_id_%u",
                 static_cast<unsigned>(i));
        String content_id = g_server.arg(field_name);
        snprintf(field_name, sizeof(field_name), "shortcut_type_%u",
                 static_cast<unsigned>(i));
        String content_type = g_server.arg(field_name);
        label.trim();
        entity.trim();
        entity.toLowerCase();
        content_id.trim();
        content_type.trim();

        if (label.isEmpty() && entity.isEmpty() &&
            content_id.isEmpty() && content_type.isEmpty()) {
            continue;
        }
        if (label.isEmpty() || entity.isEmpty() ||
            content_id.isEmpty() || content_type.isEmpty()) {
            error = "Every populated media shortcut requires all four fields.";
            return false;
        }
        if (!entity.startsWith("media_player.")) {
            error = "Media shortcut entities must begin with media_player.";
            return false;
        }

        PanelMediaShortcut &shortcut =
            config.media_shortcuts[config.media_shortcut_count++];
        snprintf(shortcut.label, sizeof(shortcut.label), "%s",
                 label.substring(0, PANEL_MEDIA_SHORTCUT_LABEL_LEN - 1).c_str());
        snprintf(shortcut.entity_id, sizeof(shortcut.entity_id), "%s",
                 entity.substring(0, PANEL_MEDIA_ENTITY_ID_LEN - 1).c_str());
        snprintf(shortcut.media_content_id, sizeof(shortcut.media_content_id), "%s",
                 content_id.substring(0, HA_MEDIA_CONTENT_ID_LEN - 1).c_str());
        snprintf(shortcut.media_content_type, sizeof(shortcut.media_content_type), "%s",
                 content_type.substring(0, HA_MEDIA_CONTENT_TYPE_LEN - 1).c_str());
    }
    return true;
}

void handle_save_config() {
    if (!ensure_auth()) return;
    std::unique_ptr<PanelConfig> next_storage(new (std::nothrow) PanelConfig(config_service_get()));
    if (!next_storage) { send_error(503, "Insufficient memory."); return; }
    PanelConfig &next = *next_storage;
    String device = g_server.arg("device_id");
    String name = g_server.arg("display_name");
    String profile = g_server.arg("profile");
    String area = g_server.arg("area_id");
    String modules = g_server.arg("modules");
    device.trim();
    name.trim();
    profile.trim();
    profile.toLowerCase();
    area.trim();

    if (device.isEmpty() || name.isEmpty()) {
        send_error(400, "Device ID and display name are required.");
        return;
    }
    if (profile != "calendar" && profile != "room" &&
        profile != "whole_home" && profile != "custom") {
        send_error(400, "Invalid profile.");
        return;
    }

    snprintf(next.device_id, sizeof(next.device_id), "%s",
             device.substring(0, 31).c_str());
    snprintf(next.display_name, sizeof(next.display_name), "%s",
             name.substring(0, 47).c_str());
    snprintf(next.profile, sizeof(next.profile), "%s", profile.c_str());
    snprintf(next.area_id, sizeof(next.area_id), "%s",
             area.substring(0, 63).c_str());
    next.backlight = static_cast<uint8_t>(
        constrain(g_server.arg("backlight").toInt(), 10, 100));
    next.screen_timeout_seconds = static_cast<uint32_t>(
        constrain(g_server.arg("timeout").toInt(), 0, 3600));
    if (!config_service_parse_modules_csv(modules, next)) {
        config_service_set_profile_defaults(next);
    }

    String room_error;
    if (g_server.hasArg("room_controls") &&
        !config_service_parse_room_controls(g_server.arg("room_controls"), next, room_error)) {
        send_error(400, room_error.c_str()); return;
    }
    String shortcut_error;
    if (!parse_media_shortcuts(next, shortcut_error)) {
        send_error(400, shortcut_error.c_str());
        return;
    }
    if (!config_service_save(next)) {
        send_error(500, "Could not save panel configuration.");
        return;
    }

    const String ha_url = g_server.arg("ha_url");
    const String ha_token = g_server.arg("ha_token");
    if (!home_assistant_set_credentials(ha_url.c_str(), ha_token.c_str())) {
        send_error(400, "Home Assistant URL must begin with http:// or https://.");
        return;
    }

    JsonDocument doc;
    doc["ok"] = true;
    doc["reboot_required"] = true;
    doc["message"] = "Saved. Room controls and media shortcuts are active now; reboot to apply profile or module changes.";
    send_json(doc);
}

void handle_room_entities() {
    if (!ensure_auth()) return;
    std::unique_ptr<HomeAssistantEntitySnapshot[]> entities(new (std::nothrow) HomeAssistantEntitySnapshot[HA_MAX_AREA_ENTITIES]);
    if (!entities) { send_error(503, "Insufficient memory."); return; }
    const size_t count = home_assistant_get_room_entities(entities.get(), HA_MAX_AREA_ENTITIES);
    std::sort(entities.get(), entities.get() + count, [](const HomeAssistantEntitySnapshot &a, const HomeAssistantEntitySnapshot &b) {
        const int names = strcmp(a.name, b.name);
        return names ? names < 0 : strcmp(a.entity_id, b.entity_id) < 0;
    });
    JsonDocument doc;
    JsonArray list = doc.to<JsonArray>();
    for (size_t i = 0; i < count; ++i) {
        JsonObject item = list.add<JsonObject>();
        item["entity_id"] = entities[i].entity_id;
        item["name"] = entities[i].name;
    }
    send_json(doc);
}

void handle_ha_test() {
    if (!ensure_auth()) return;
    JsonDocument doc;
    doc["ok"] = true;
    doc["queued"] = home_assistant_request_health_check();
    send_json(doc);
}

void handle_reboot() {
    if (!ensure_auth()) return;
    JsonDocument doc;
    doc["ok"] = true;
    doc["rebooting"] = true;
    send_json(doc);
    g_reboot_at_ms = millis() + 700;
}

void register_routes() {
    if (g_routes_registered) return;
    g_routes_registered = true;
    g_server.on("/", HTTP_GET, []() {
        if (!ensure_auth()) return;
        g_server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
    });
    g_server.on("/api/status", HTTP_GET, handle_status);
    g_server.on("/api/room/entities", HTTP_GET, handle_room_entities);
    g_server.on("/api/config", HTTP_GET, handle_get_config);
    g_server.on("/api/config", HTTP_POST, handle_save_config);
    g_server.on("/api/ha/test", HTTP_POST, handle_ha_test);
    g_server.on("/api/reboot", HTTP_POST, handle_reboot);
    g_server.onNotFound([]() {
        if (!ensure_auth()) return;
        send_error(404, "Not found");
    });
}

}  // namespace

void web_manager_begin() {
    register_routes();
    g_server.begin();
    Serial0.printf("[Web] Management server started on port %u\n",
                   static_cast<unsigned>(WEB_MANAGER_PORT));
}

void web_manager_loop() {
    if (network_service_connected()) {
        const uint32_t now = millis();
        if (!g_mdns_started &&
            (!g_last_mdns_attempt_ms || now - g_last_mdns_attempt_ms >= 5000UL)) {
            g_last_mdns_attempt_ms = now;
            if (MDNS.begin(config_service_get().device_id)) {
                MDNS.addService("http", "tcp", WEB_MANAGER_PORT);
                g_mdns_started = true;
                Serial0.printf("[Web] http://%s.local\n",
                               config_service_get().device_id);
            }
        }
        g_server.handleClient();
    } else if (g_mdns_started) {
        MDNS.end();
        g_mdns_started = false;
        g_last_mdns_attempt_ms = 0;
    }

    if (g_reboot_at_ms &&
        static_cast<int32_t>(millis() - g_reboot_at_ms) >= 0) {
        Serial0.flush();
        delay(50);
        ESP.restart();
    }
}
