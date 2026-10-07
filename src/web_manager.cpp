#include <memory>
#include <algorithm>
#include <new>
#include "web_manager.h"

#include "app_config.h"
#include "battery_service.h"
#include "config_service.h"
#include "firmware_update.h"
#include "home_assistant.h"
#include "network_service.h"
#include "web_ui.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <esp_system.h>
#include <WebServer.h>
#include <WiFi.h>
#include <string.h>

namespace {

WebServer g_server(WEB_MANAGER_PORT);
bool g_routes_registered = false;
bool g_mdns_started = false;
uint32_t g_last_mdns_attempt_ms = 0;
uint32_t g_reboot_at_ms = 0;
char g_boot_id[17] = {};
char g_ota_session[17] = {};
struct FirmwareRequest {
    bool seen = false;
    bool owned = false;
    bool complete = false;
    size_t expected = 0;
    size_t received = 0;
    int code = 400;
    char error[160] = {};
} g_firmware_request;

#if 0 // Replaced by the tabbed 1.5.0 editor in include/web_ui.h.
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
#endif

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

bool firmware_busy() {
    const auto &state = firmware_update_status();
    if (!state.active && !state.verified) return false;
    send_error(409, "A firmware update is in progress. Wait for it to finish.");
    return true;
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
    doc["boot_id"] = g_boot_id;
    doc["uptime_ms"] = millis();
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
    doc["explicit_layout"] = cfg.explicit_layout;
    doc["weather_entity_id"] = cfg.weather_entity_id;
    doc["weather_hourly_entity_id"] = cfg.weather_hourly_entity_id;
    doc["weather_daily_entity_id"] = cfg.weather_daily_entity_id;
    doc["weather_layout"] = cfg.weather_layout;
    doc["weather_show_current"] = cfg.weather_show_current;
    doc["weather_show_hourly"] = cfg.weather_show_hourly;
    doc["weather_show_daily"] = cfg.weather_show_daily;
    doc["weather_header_enabled"] = cfg.weather_header_enabled;
    doc["calendar_entity_id"] = cfg.calendar_entity_id;
    doc["calendar_week_starts_monday"] = cfg.calendar_week_starts_monday;
    doc["calendar_days"] = cfg.calendar_days;
    doc["climate_show_humidity"] = cfg.climate_show_humidity;
    doc["climate_show_fan"] = cfg.climate_show_fan;
    doc["climate_show_presets"] = cfg.climate_show_presets;
    JsonArray climate_devices = doc["climate_devices"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.climate_device_count; ++i) {
        JsonObject item = climate_devices.add<JsonObject>();
        item["entity_id"] = cfg.climate_devices[i].entity_id;
        item["label"] = cfg.climate_devices[i].label;
    }
    doc["alarm_entity_id"] = cfg.alarm_entity_id;
    doc["security_show_abnormal_summary"] = cfg.security_show_abnormal_summary;
    doc["security_confirm_arming"] = cfg.security_confirm_arming;
    doc["security_code_to_arm"] = cfg.security_code_to_arm;
    doc["security_arm_home"] = cfg.security_arm_home;
    doc["security_arm_away"] = cfg.security_arm_away;
    doc["security_arm_night"] = cfg.security_arm_night;
    doc["security_arm_vacation"] = cfg.security_arm_vacation;
    JsonArray security_devices = doc["security_devices"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.security_device_count; ++i) {
        const PanelSecurityDevice &source = cfg.security_devices[i];
        JsonObject item = security_devices.add<JsonObject>();
        item["entity_id"] = source.entity_id;
        item["label"] = source.label;
        item["icon"] = source.icon;
        item["abnormal_states"] = source.abnormal_states;
        item["normal_label"] = source.normal_label;
        item["abnormal_label"] = source.abnormal_label;
        item["color"] = source.color;
        item["reverse_abnormal"] = source.reverse_abnormal;
    }
    JsonArray security_dynamic_devices = doc["security_dynamic_devices"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.security_dynamic_device_count; ++i) {
        const PanelSecurityDevice &source = cfg.security_dynamic_devices[i];
        JsonObject item = security_dynamic_devices.add<JsonObject>();
        item["entity_id"] = source.entity_id;
        item["label"] = source.label;
        item["icon"] = source.icon;
        item["abnormal_states"] = source.abnormal_states;
        item["normal_label"] = source.normal_label;
        item["abnormal_label"] = source.abnormal_label;
        item["color"] = source.color;
        item["reverse_abnormal"] = source.reverse_abnormal;
    }
    JsonArray calendars = doc["calendars"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.calendar_count; ++i) {
        JsonObject item = calendars.add<JsonObject>();
        item["entity_id"] = cfg.calendars[i].entity_id;
        item["label"] = cfg.calendars[i].label;
        item["color"] = cfg.calendars[i].color;
    }
    doc["ha_url"] = home_assistant_base_url();
    doc["ha_token_configured"] = home_assistant_token_configured();
    JsonArray shortcuts = doc["media_shortcuts"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.media_shortcut_count; ++i) {
        JsonObject item = shortcuts.add<JsonObject>();
        item["label"] = cfg.media_shortcuts[i].label;
        item["icon"] = cfg.media_shortcuts[i].icon;
        item["entity_id"] = cfg.media_shortcuts[i].entity_id;
        item["media_content_id"] = cfg.media_shortcuts[i].media_content_id;
        item["media_content_type"] = cfg.media_shortcuts[i].media_content_type;
    }
    JsonArray favorites = doc["media_favorites"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.media_favorite_count; ++i) {
        JsonObject item = favorites.add<JsonObject>();
        item["label"] = cfg.media_favorites[i].label;
        item["icon"] = cfg.media_favorites[i].icon;
        item["entity_id"] = cfg.media_favorites[i].entity_id;
        item["media_content_id"] = cfg.media_favorites[i].media_content_id;
        item["media_content_type"] = cfg.media_favorites[i].media_content_type;
    }
    JsonArray room = doc["room_controls"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.room_control_count; ++i) {
        JsonObject item = room.add<JsonObject>();
        item["entity_id"] = cfg.room_controls[i].entity_id;
        item["label"] = cfg.room_controls[i].label;
        item["placement"] = cfg.room_controls[i].placement;
        item["room_index"] = cfg.room_controls[i].room_index;
        item["device_type"] = cfg.room_controls[i].device_type;
    }
    JsonArray rooms = doc["rooms"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.room_count; ++i) {
        JsonObject item = rooms.add<JsonObject>();
        item["tab_label"] = cfg.rooms[i].tab_label;
        item["header"] = cfg.rooms[i].header;
        item["temperature_entity_id"] = cfg.rooms[i].temperature_entity_id;
        item["humidity_entity_id"] = cfg.rooms[i].humidity_entity_id;
        JsonArray status_slots = item["status_slots"].to<JsonArray>();
        for (uint8_t slot = 0; slot < PANEL_ROOM_STATUS_SLOTS; ++slot) {
            const PanelRoomStatusSlot &source = cfg.rooms[i].status_slots[slot];
            JsonObject status = status_slots.add<JsonObject>();
            status["type"] = source.type;
            status["entity_id"] = source.entity_id;
            status["label"] = source.label;
            status["icon"] = source.icon;
            status["active_states"] = source.active_states;
            status["active_label"] = source.active_label;
            status["inactive_label"] = source.inactive_label;
            status["color"] = source.color;
        }
    }
    JsonArray players = doc["media_players"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.media_player_count; ++i) players.add(cfg.media_players[i]);
    JsonArray widgets = doc["overview_widgets"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.overview_widget_count; ++i) {
        JsonObject item = widgets.add<JsonObject>();
        item["type"] = cfg.overview_widgets[i].type;
        item["span"] = cfg.overview_widgets[i].span;
        item["height"] = cfg.overview_widgets[i].height;
    }
    JsonArray quick_actions = doc["overview_quick_actions"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.overview_quick_action_count; ++i) {
        JsonObject item = quick_actions.add<JsonObject>();
        item["label"] = cfg.overview_quick_actions[i].label;
        item["type"] = cfg.overview_quick_actions[i].type;
        item["entity_id"] = cfg.overview_quick_actions[i].entity_id;
    }
    JsonArray overview_items = doc["overview_items"].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.overview_item_count; ++i) {
        const PanelOverviewItem &source = cfg.overview_items[i];
        JsonObject item = overview_items.add<JsonObject>();
        item["type"] = source.type;
        item["entity_id"] = source.entity_id;
        item["action_entity_id"] = source.action_entity_id;
        item["label"] = source.label;
        item["icon"] = source.icon;
        item["action"] = source.action;
        item["active_states"] = source.active_states;
        item["active_label"] = source.active_label;
        item["inactive_label"] = source.inactive_label;
        item["color"] = source.color;
        item["span"] = source.span;
        item["confirm"] = source.confirm;
    }
    send_json(doc);
}

bool parse_media_players(PanelConfig &config, String &error) {
    if (!g_server.hasArg("media_players")) return true;
    JsonDocument doc;
    if (deserializeJson(doc, g_server.arg("media_players")) || !doc.is<JsonArray>() ||
        doc.size() > PANEL_MAX_MEDIA_PLAYERS) {
        error = "Media players must be an array of at most six media_player entity IDs.";
        return false;
    }
    config.media_player_count = 0;
    memset(config.media_players, 0, sizeof(config.media_players));
    for (JsonVariant item : doc.as<JsonArray>()) {
        const char *id = item.as<const char *>();
        if (!id || strncmp(id, "media_player.", 13) != 0 || strlen(id) >= PANEL_MEDIA_ENTITY_ID_LEN) {
            error = "Media player IDs must begin with media_player."; return false;
        }
        for (uint8_t i = 0; i < config.media_player_count; ++i)
            if (strcmp(config.media_players[i], id) == 0) { error = "Media player IDs must be unique."; return false; }
        snprintf(config.media_players[config.media_player_count++], PANEL_MEDIA_ENTITY_ID_LEN, "%s", id);
    }
    return true;
}

bool parse_calendars(PanelConfig &config, String &error) {
    config.calendar_count = 0;
    memset(config.calendars, 0, sizeof(config.calendars));
    config.calendar_entity_id[0] = '\0';
    if (!g_server.hasArg("calendars")) return true;
    JsonDocument doc;
    if (deserializeJson(doc, g_server.arg("calendars")) || !doc.is<JsonArray>() ||
        doc.size() > PANEL_MAX_CALENDARS) {
        error = "Calendars must be an array of at most six selections.";
        return false;
    }
    for (JsonObject item : doc.as<JsonArray>()) {
        String entity = item["entity_id"] | "";
        String label = item["label"] | "";
        String color = item["color"] | "cyan";
        entity.trim(); entity.toLowerCase(); label.trim(); color.trim(); color.toLowerCase();
        if (!entity.startsWith("calendar.") || entity.length() >= 96) {
            error = "Calendar entity IDs must begin with calendar.";
            return false;
        }
        for (uint8_t i = 0; i < config.calendar_count; ++i) {
            if (strcmp(config.calendars[i].entity_id, entity.c_str()) == 0) {
                error = "Calendar selections must be unique.";
                return false;
            }
        }
        if (color != "cyan" && color != "green" && color != "yellow" &&
            color != "red" && color != "purple" && color != "blue") color = "cyan";
        PanelCalendarSource &source = config.calendars[config.calendar_count++];
        snprintf(source.entity_id, sizeof(source.entity_id), "%s", entity.c_str());
        snprintf(source.label, sizeof(source.label), "%s",
                 (label.isEmpty() ? entity : label).substring(0, PANEL_CALENDAR_LABEL_LEN - 1).c_str());
        snprintf(source.color, sizeof(source.color), "%s", color.c_str());
    }
    if (config.calendar_count)
        snprintf(config.calendar_entity_id, sizeof(config.calendar_entity_id), "%s",
                 config.calendars[0].entity_id);
    return true;
}

bool parse_media_shortcuts(PanelConfig &config, String &error) {
    config.media_shortcut_count = 0;
    memset(config.media_shortcuts, 0, sizeof(config.media_shortcuts));

    if (g_server.hasArg("media_shortcuts")) {
        JsonDocument doc;
        if (deserializeJson(doc, g_server.arg("media_shortcuts")) ||
            !doc.is<JsonArray>() || doc.size() > PANEL_MAX_MEDIA_SHORTCUTS) {
            error = "Media shortcuts must be an array of at most six actions.";
            return false;
        }
        for (JsonObject item : doc.as<JsonArray>()) {
            String label = item["label"] | "";
            String icon = item["icon"] | "music";
            String entity = item["entity_id"] | "";
            String content_id = item["media_content_id"] | "";
            String content_type = item["media_content_type"] | "";
            label.trim(); icon.trim(); entity.trim(); entity.toLowerCase();
            content_id.trim(); content_type.trim();
            if (label.isEmpty() || entity.isEmpty() || content_id.isEmpty() ||
                content_type.isEmpty()) {
                error = "Every media shortcut requires a label, player, content ID, and content type.";
                return false;
            }
            if (!entity.startsWith("media_player.")) {
                error = "Media shortcut entities must begin with media_player.";
                return false;
            }
            PanelMediaShortcut &shortcut =
                config.media_shortcuts[config.media_shortcut_count++];
            snprintf(shortcut.label, sizeof(shortcut.label), "%s", label.substring(0, PANEL_MEDIA_SHORTCUT_LABEL_LEN - 1).c_str());
            snprintf(shortcut.icon, sizeof(shortcut.icon), "%s", (icon.isEmpty() ? String("music") : icon).substring(0, sizeof(shortcut.icon) - 1).c_str());
            snprintf(shortcut.entity_id, sizeof(shortcut.entity_id), "%s", entity.substring(0, PANEL_MEDIA_ENTITY_ID_LEN - 1).c_str());
            snprintf(shortcut.media_content_id, sizeof(shortcut.media_content_id), "%s", content_id.substring(0, HA_MEDIA_CONTENT_ID_LEN - 1).c_str());
            snprintf(shortcut.media_content_type, sizeof(shortcut.media_content_type), "%s", content_type.substring(0, HA_MEDIA_CONTENT_TYPE_LEN - 1).c_str());
        }
        return true;
    }

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
        snprintf(shortcut.icon, sizeof(shortcut.icon), "music");
        snprintf(shortcut.entity_id, sizeof(shortcut.entity_id), "%s",
                 entity.substring(0, PANEL_MEDIA_ENTITY_ID_LEN - 1).c_str());
        snprintf(shortcut.media_content_id, sizeof(shortcut.media_content_id), "%s",
                 content_id.substring(0, HA_MEDIA_CONTENT_ID_LEN - 1).c_str());
        snprintf(shortcut.media_content_type, sizeof(shortcut.media_content_type), "%s",
                 content_type.substring(0, HA_MEDIA_CONTENT_TYPE_LEN - 1).c_str());
    }
    return true;
}

bool parse_media_favorites(PanelConfig &config, String &error) {
    config.media_favorite_count = 0;
    memset(config.media_favorites, 0, sizeof(config.media_favorites));

    if (g_server.hasArg("media_favorites")) {
        JsonDocument doc;
        if (deserializeJson(doc, g_server.arg("media_favorites")) ||
            !doc.is<JsonArray>() || doc.size() > PANEL_MAX_MEDIA_FAVORITES) {
            error = "Media favorites must be an array of at most six actions.";
            return false;
        }
        for (JsonObject item : doc.as<JsonArray>()) {
            String label = item["label"] | "";
            String icon = item["icon"] | "star";
            String entity = item["entity_id"] | "";
            String content_id = item["media_content_id"] | "";
            String content_type = item["media_content_type"] | "";
            label.trim(); icon.trim(); entity.trim(); entity.toLowerCase();
            content_id.trim(); content_type.trim();
            if (label.isEmpty() || entity.isEmpty() || content_id.isEmpty() ||
                content_type.isEmpty()) {
                error = "Every media favorite requires a label, player, content ID, and content type.";
                return false;
            }
            if (!entity.startsWith("media_player.")) {
                error = "Browse favorite entities must begin with media_player.";
                return false;
            }
            PanelMediaShortcut &favorite =
                config.media_favorites[config.media_favorite_count++];
            snprintf(favorite.label, sizeof(favorite.label), "%s", label.substring(0, PANEL_MEDIA_SHORTCUT_LABEL_LEN - 1).c_str());
            snprintf(favorite.icon, sizeof(favorite.icon), "%s", (icon.isEmpty() ? String("star") : icon).substring(0, sizeof(favorite.icon) - 1).c_str());
            snprintf(favorite.entity_id, sizeof(favorite.entity_id), "%s", entity.substring(0, PANEL_MEDIA_ENTITY_ID_LEN - 1).c_str());
            snprintf(favorite.media_content_id, sizeof(favorite.media_content_id), "%s", content_id.substring(0, HA_MEDIA_CONTENT_ID_LEN - 1).c_str());
            snprintf(favorite.media_content_type, sizeof(favorite.media_content_type), "%s", content_type.substring(0, HA_MEDIA_CONTENT_TYPE_LEN - 1).c_str());
        }
        return true;
    }

    for (uint8_t i = 0; i < PANEL_MAX_MEDIA_FAVORITES; ++i) {
        char field_name[32] = {};
        snprintf(field_name, sizeof(field_name), "favorite_label_%u", static_cast<unsigned>(i));
        String label = g_server.arg(field_name);
        snprintf(field_name, sizeof(field_name), "favorite_entity_%u", static_cast<unsigned>(i));
        String entity = g_server.arg(field_name);
        snprintf(field_name, sizeof(field_name), "favorite_id_%u", static_cast<unsigned>(i));
        String content_id = g_server.arg(field_name);
        snprintf(field_name, sizeof(field_name), "favorite_type_%u", static_cast<unsigned>(i));
        String content_type = g_server.arg(field_name);
        label.trim(); entity.trim(); entity.toLowerCase(); content_id.trim(); content_type.trim();

        if (label.isEmpty() && entity.isEmpty() && content_id.isEmpty() && content_type.isEmpty()) continue;
        if (label.isEmpty() || entity.isEmpty() || content_id.isEmpty() || content_type.isEmpty()) {
            error = "Every populated Browse favorite requires all four fields."; return false;
        }
        if (!entity.startsWith("media_player.")) {
            error = "Browse favorite entities must begin with media_player."; return false;
        }
        PanelMediaShortcut &favorite = config.media_favorites[config.media_favorite_count++];
        snprintf(favorite.label, sizeof(favorite.label), "%s", label.substring(0, PANEL_MEDIA_SHORTCUT_LABEL_LEN - 1).c_str());
        snprintf(favorite.icon, sizeof(favorite.icon), "star");
        snprintf(favorite.entity_id, sizeof(favorite.entity_id), "%s", entity.substring(0, PANEL_MEDIA_ENTITY_ID_LEN - 1).c_str());
        snprintf(favorite.media_content_id, sizeof(favorite.media_content_id), "%s", content_id.substring(0, HA_MEDIA_CONTENT_ID_LEN - 1).c_str());
        snprintf(favorite.media_content_type, sizeof(favorite.media_content_type), "%s", content_type.substring(0, HA_MEDIA_CONTENT_TYPE_LEN - 1).c_str());
    }
    return true;
}

void handle_save_config() {
    if (!ensure_auth() || firmware_busy()) return;
    std::unique_ptr<PanelConfig> next_storage(new (std::nothrow) PanelConfig(config_service_get()));
    if (!next_storage) { send_error(503, "Insufficient memory."); return; }
    PanelConfig &next = *next_storage;
    String device = g_server.arg("device_id");
    String name = g_server.arg("display_name");
    String profile = g_server.arg("profile");
    String modules = g_server.arg("modules");
    device.trim();
    name.trim();
    profile.trim();
    profile.toLowerCase();

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
    // Area membership is not a panel-wide setting. Keep the legacy field
    // empty so prior configurations cannot silently reintroduce filtering.
    next.area_id[0] = '\0';
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
    String rooms_error;
    if (g_server.hasArg("rooms") && !config_service_parse_rooms(g_server.arg("rooms"), next, rooms_error)) { send_error(400, rooms_error.c_str()); return; }
    auto save_weather_entity = [&](const char *arg_name, char *target, size_t target_len,
                                   const char *label) -> bool {
        String value = g_server.arg(arg_name); value.trim(); value.toLowerCase();
        if (!value.isEmpty() && !value.startsWith("weather.")) {
            String message = String(label) + " must be a weather.* Home Assistant entity.";
            send_error(400, message.c_str()); return false;
        }
        snprintf(target, target_len, "%s", value.substring(0, 95).c_str());
        return true;
    };
    if (!save_weather_entity("weather_entity_id", next.weather_entity_id,
                             sizeof(next.weather_entity_id), "Current conditions selection") ||
        !save_weather_entity("weather_hourly_entity_id", next.weather_hourly_entity_id,
                             sizeof(next.weather_hourly_entity_id), "Hourly forecast selection") ||
        !save_weather_entity("weather_daily_entity_id", next.weather_daily_entity_id,
                             sizeof(next.weather_daily_entity_id), "Daily forecast selection")) return;
    String weather_layout = g_server.arg("weather_layout"); weather_layout.trim(); weather_layout.toLowerCase();
    if (weather_layout != "balanced" && weather_layout != "current_focus" && weather_layout != "forecast_focus") {
        send_error(400, "Weather layout must be balanced, current focus, or forecast focus."); return;
    }
    snprintf(next.weather_layout, sizeof(next.weather_layout), "%s", weather_layout.c_str());
    next.weather_show_current = g_server.arg("weather_show_current") == "1";
    next.weather_show_hourly = g_server.arg("weather_show_hourly") == "1";
    next.weather_show_daily = g_server.arg("weather_show_daily") == "1";
    next.weather_header_enabled = g_server.arg("weather_header_enabled") == "1";
    if (!next.weather_show_current && !next.weather_show_hourly && !next.weather_show_daily) {
        send_error(400, "Show at least one Weather tab section."); return;
    }
    String calendars_error;
    if (!parse_calendars(next, calendars_error)) { send_error(400, calendars_error.c_str()); return; }
    next.calendar_week_starts_monday = g_server.arg("calendar_week_starts_monday") != "0";
    const int calendar_days = g_server.hasArg("calendar_days") ?
        g_server.arg("calendar_days").toInt() : next.calendar_days;
    if (calendar_days != 1 && calendar_days != 3 && calendar_days != 7) {
        send_error(400, "Calendar days shown must be 1, 3, or 7."); return;
    }
    next.calendar_days = static_cast<uint8_t>(calendar_days);
    next.climate_show_humidity = g_server.arg("climate_show_humidity") != "0";
    next.climate_show_fan = g_server.arg("climate_show_fan") != "0";
    next.climate_show_presets = g_server.arg("climate_show_presets") != "0";
    String climate_error;
    if (g_server.hasArg("climate_devices") &&
        !config_service_parse_climate_devices(g_server.arg("climate_devices"), next,
                                              climate_error)) {
        send_error(400, climate_error.c_str()); return;
    }
    String alarm_entity = g_server.arg("alarm_entity_id"); alarm_entity.trim(); alarm_entity.toLowerCase();
    if (!alarm_entity.isEmpty() && !alarm_entity.startsWith("alarm_control_panel.")) {
        send_error(400, "Alarmo selection must be an alarm_control_panel.* Home Assistant entity."); return;
    }
    snprintf(next.alarm_entity_id, sizeof(next.alarm_entity_id), "%s",
             alarm_entity.substring(0, 95).c_str());
    next.security_show_abnormal_summary = g_server.arg("security_show_abnormal_summary") != "0";
    next.security_confirm_arming = g_server.arg("security_confirm_arming") != "0";
    next.security_code_to_arm = g_server.arg("security_code_to_arm") == "1";
    next.security_arm_home = g_server.arg("security_arm_home") != "0";
    next.security_arm_away = g_server.arg("security_arm_away") != "0";
    next.security_arm_night = g_server.arg("security_arm_night") != "0";
    next.security_arm_vacation = g_server.arg("security_arm_vacation") == "1";
    if (!next.security_arm_home && !next.security_arm_away &&
        !next.security_arm_night && !next.security_arm_vacation) {
        send_error(400, "Enable at least one Alarmo arming mode."); return;
    }
    String security_error;
    if (g_server.hasArg("security_devices") &&
        !config_service_parse_security_devices(g_server.arg("security_devices"), next, security_error)) {
        send_error(400, security_error.c_str()); return;
    }
    if (g_server.hasArg("security_dynamic_devices") &&
        !config_service_parse_security_dynamic_devices(g_server.arg("security_dynamic_devices"),
                                                       next, security_error)) {
        send_error(400, security_error.c_str()); return;
    }
    if (!config_service_validate_security_device_uniqueness(next, security_error)) {
        send_error(400, security_error.c_str()); return;
    }
    String widgets_error;
    if (g_server.hasArg("overview_widgets") &&
        !config_service_parse_overview_widgets(g_server.arg("overview_widgets"), next, widgets_error)) {
        send_error(400, widgets_error.c_str()); return;
    }
    String quick_actions_error;
    if (g_server.hasArg("overview_quick_actions") &&
        !config_service_parse_overview_quick_actions(g_server.arg("overview_quick_actions"), next, quick_actions_error)) {
        send_error(400, quick_actions_error.c_str()); return;
    }
    String overview_items_error;
    if (g_server.hasArg("overview_items") &&
        !config_service_parse_overview_items(g_server.arg("overview_items"), next, overview_items_error)) {
        send_error(400, overview_items_error.c_str()); return;
    }
    String shortcut_error;
    if (!parse_media_shortcuts(next, shortcut_error)) {
        send_error(400, shortcut_error.c_str());
        return;
    }
    String favorite_error;
    if (!parse_media_favorites(next, favorite_error)) {
        send_error(400, favorite_error.c_str());
        return;
    }
    String players_error;
    if (!parse_media_players(next, players_error)) {
        send_error(400, players_error.c_str()); return;
    }
    const String ha_url = g_server.arg("ha_url");
    if (!ha_url.isEmpty() && !ha_url.startsWith("http://") && !ha_url.startsWith("https://")) {
        send_error(400, "Home Assistant URL must begin with http:// or https://."); return;
    }
    // The 1.5 editor is the explicit opt-in migration point for installations
    // that previously showed every supported entity in the configured area.
    next.explicit_layout = true;
    if (!config_service_save(next)) {
        send_error(500, "Could not save panel configuration.");
        return;
    }

    const String ha_token = g_server.arg("ha_token");
    if (!home_assistant_set_credentials(ha_url.c_str(), ha_token.c_str())) {
        send_error(400, "Home Assistant URL must begin with http:// or https://.");
        return;
    }
    const bool discovery_queued = home_assistant_request_discovery();

    JsonDocument doc;
    doc["ok"] = true;
    doc["reboot_required"] = true;
    doc["message"] = discovery_queued ?
        "Saved. Home Assistant is refreshing the configured layout; reboot to apply Overview cards or tab-order changes." :
        "Saved. Reboot the panel to reconnect and apply the Overview and explicit layouts.";
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

void handle_ha_entities() {
    if (!ensure_auth()) return;
    std::unique_ptr<HomeAssistantEntitySnapshot[]> entities(
        new (std::nothrow) HomeAssistantEntitySnapshot[HA_MAX_AREA_ENTITIES]);
    if (!entities) { send_error(503, "Insufficient memory."); return; }
    const size_t room_count = home_assistant_get_layout_entities(entities.get(), HA_MAX_AREA_ENTITIES);
    HomeAssistantDiscoveryStatus discovery = {};
    home_assistant_get_discovery_status(discovery);
    JsonDocument doc;
    doc["complete"] = discovery.discovery_complete;
    doc["message"] = discovery.message;
    JsonArray list = doc["entities"].to<JsonArray>();
    for (size_t i = 0; i < room_count; ++i) {
        JsonObject item = list.add<JsonObject>();
        item["entity_id"] = entities[i].entity_id; item["name"] = entities[i].name;
        item["domain"] = entities[i].domain; item["available"] = entities[i].available;
        item["brightness"] = entities[i].supports_brightness; item["position"] = entities[i].supports_position;
    }
    send_json(doc);
}

void handle_ha_discover() {
    if (!ensure_auth()) return;
    const String query = g_server.arg("q");
    const String domain = g_server.arg("domain");
    JsonDocument doc;
    doc["ok"] = true;
    doc["queued"] = home_assistant_request_entity_search(query.c_str(), domain.c_str());
    doc["query"] = query;
    doc["domain"] = domain;
    send_json(doc);
}

void handle_ha_test() {
    if (!ensure_auth()) return;
    JsonDocument doc;
    doc["ok"] = true;
    doc["queued"] = home_assistant_request_health_check();
    send_json(doc);
}

void handle_config_backup() {
    if (!ensure_auth()) return;
    String backup;
    if (!config_service_export_json(backup)) {
        send_error(500, "Could not read the current panel configuration.");
        return;
    }
    String safe_device = config_service_get().device_id;
    for (size_t i = 0; i < safe_device.length(); ++i) {
        const char c = safe_device[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) safe_device.setCharAt(i, '_');
    }
    const String filename = "home-panel-" + safe_device + "-v" APP_VERSION "-config.json";
    g_server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send(200, "application/json", backup);
}

void handle_config_restore() {
    if (!ensure_auth()) return;
    if (firmware_busy()) return;
    const String &payload = g_server.arg("plain");
    String error;
    if (!config_service_restore_json(payload, error)) {
        send_error(400, error.c_str());
        return;
    }
    JsonDocument doc;
    doc["ok"] = true;
    doc["rebooting"] = true;
    doc["message"] = "Configuration restored and validated. Rebooting to apply it.";
    send_json(doc);
    g_reboot_at_ms = millis() + 1200;
}

// Multipart callbacks never activate an image: activation follows the complete HTTP request.
void request_error(const char *message, int code = 400) {
    g_firmware_request.code = code;
    snprintf(g_firmware_request.error, sizeof(g_firmware_request.error), "%s", message);
    if (g_firmware_request.owned) firmware_update_abort(g_firmware_request.error);
}

bool read_size_arg(const char *name, size_t &value) {
    const String text = g_server.arg(name);
    if (!text.length() || text.length() > 8) return false;
    value = 0;
    for (size_t i = 0; i < text.length(); ++i) {
        if (text[i] < '0' || text[i] > '9') return false;
        value = value * 10 + (text[i] - '0');
    }
    return true;
}

bool session_matches() {
    return g_ota_session[0] && g_server.arg("session") == g_ota_session;
}

void send_firmware_state(bool rebooting = false) {
    const auto &state = firmware_update_status();
    JsonDocument doc;
    doc["ok"] = true;
    doc["active"] = state.active;
    doc["verified"] = state.verified;
    doc["bytes"] = state.written;
    doc["expected"] = state.expected;
    doc["error"] = state.error;
    doc["session"] = g_ota_session;
    doc["chunk_bytes"] = WEB_OTA_CHUNK_BYTES;
    doc["boot_id"] = g_boot_id;
    doc["version"] = APP_VERSION;
    doc["rebooting"] = rebooting;
    if (rebooting) doc["message"] = "Firmware verified. Rebooting into the update.";
    g_server.sendHeader("Cache-Control", "no-store");
    send_json(doc);
}

void handle_firmware_begin() {
    if (!ensure_auth() || firmware_busy()) return;
    if (g_reboot_at_ms) { send_error(409, "The panel is already rebooting."); return; }
    size_t size = 0;
    if (!read_size_arg("size", size) || !size || size > WEB_OTA_MAX_BYTES) {
        send_error(400, "Firmware must be between 1 byte and 6 MB."); return;
    }
    if (!firmware_update_begin(size)) {
        send_error(400, firmware_update_status().error); return;
    }
    snprintf(g_ota_session, sizeof(g_ota_session), "%08lx%08lx",
             static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
    send_firmware_state();
}

void handle_firmware_status() {
    if (!ensure_auth()) return;
    if (!session_matches()) { send_error(409, "Firmware session is no longer available."); return; }
    send_firmware_state();
}

void handle_firmware_abort() {
    if (!ensure_auth()) return;
    if (!session_matches()) { send_error(409, "Firmware session is no longer available."); return; }
    firmware_update_abort("Firmware update canceled before activation.");
    send_firmware_state();
}

void handle_firmware_finish() {
    if (!ensure_auth()) return;
    if (!session_matches()) { send_error(409, "Firmware session is no longer available."); return; }
    if (!firmware_update_status().verified && !firmware_update_finish()) {
        send_error(400, firmware_update_status().error); return;
    }
    send_firmware_state(true);
    g_reboot_at_ms = millis() + 1500;
}

void handle_firmware_data(bool chunked) {
    auto &request = g_firmware_request;
    HTTPUpload &upload = g_server.upload();
    delay(1); // Also yield while consuming a rejected upload.
    if (upload.status == UPLOAD_FILE_START) {
        if (request.seen) { request_error("Only one firmware file is allowed per request."); return; }
        request = FirmwareRequest{};
        request.seen = true;
        if (!g_server.authenticate(WEB_MANAGER_USER, WEB_MANAGER_PASSWORD)) {
            request_error("Authentication is required for firmware updates.", 401); return;
        }
        String filename = upload.filename;
        filename.toLowerCase();
        if (!filename.endsWith(".bin")) { request_error("Firmware file must use the .bin extension."); return; }
        if (chunked) {
            size_t offset = 0;
            const auto &state = firmware_update_status();
            if (!session_matches() || !state.active || state.verified) {
                request_error("Firmware session is no longer active.", 409); return;
            }
            if (!read_size_arg("offset", offset) || offset != state.written) {
                request_error("Firmware chunk offset does not match acknowledged bytes.", 409); return;
            }
            if (!read_size_arg("size", request.expected) || !request.expected ||
                request.expected > WEB_OTA_CHUNK_BYTES || request.expected > state.expected - state.written ||
                g_server.clientContentLength() > request.expected + 2048) {
                request_error("Invalid firmware chunk size."); return;
            }
        } else {
            if (g_reboot_at_ms || firmware_update_status().active || firmware_update_status().verified) {
                request_error("A firmware update or reboot is already in progress.", 409); return;
            }
            g_ota_session[0] = '\0';
            if (!firmware_update_begin_legacy()) {
                request_error(firmware_update_status().error); return;
            }
        }
        request.owned = true;
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!request.owned || request.error[0]) return;
        if (chunked && upload.currentSize > request.expected - request.received) {
            request_error("Firmware chunk exceeds the declared size."); return;
        }
        if (!firmware_update_write(upload.buf, upload.currentSize)) {
            request_error(firmware_update_status().error); return;
        }
        request.received += upload.currentSize;
    } else if (upload.status == UPLOAD_FILE_END) {
        if (!request.owned || request.error[0]) return;
        if (!request.received || (chunked && request.received != request.expected)) {
            request_error("Firmware upload ended before all declared bytes arrived."); return;
        }
        request.complete = true;
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        if (request.owned) firmware_update_abort("Firmware upload connection closed before completion.");
        request = FirmwareRequest{};
    }
}

void handle_firmware_upload() { handle_firmware_data(false); }
void handle_firmware_chunk_upload() { handle_firmware_data(true); }

void complete_firmware_request(bool chunked) {
    const FirmwareRequest request = g_firmware_request;
    g_firmware_request = FirmwareRequest{};
    if (!ensure_auth()) return;
    if (request.error[0]) { send_error(request.code, request.error); return; }
    if (!request.owned || !request.complete) {
        if (request.owned) firmware_update_abort("Firmware upload did not complete.");
        send_error(400, "Firmware upload did not complete."); return;
    }
    if (!chunked && !firmware_update_finish()) {
        send_error(400, firmware_update_status().error); return;
    }
    send_firmware_state(!chunked);
    if (!chunked) g_reboot_at_ms = millis() + 1500;
}

void handle_firmware_complete() { complete_firmware_request(false); }
void handle_firmware_chunk_complete() { complete_firmware_request(true); }

void handle_reboot() {
    if (!ensure_auth() || firmware_busy()) return;
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
    g_server.on("/api/ha/entities", HTTP_GET, handle_ha_entities);
    g_server.on("/api/ha/discover", HTTP_POST, handle_ha_discover);
    g_server.on("/api/config", HTTP_GET, handle_get_config);
    g_server.on("/api/config", HTTP_POST, handle_save_config);
    g_server.on("/api/config/backup", HTTP_GET, handle_config_backup);
    g_server.on("/api/config/restore", HTTP_POST, handle_config_restore);
    g_server.on("/api/firmware/begin", HTTP_POST, handle_firmware_begin);
    g_server.on("/api/firmware/status", HTTP_GET, handle_firmware_status);
    g_server.on("/api/firmware/abort", HTTP_POST, handle_firmware_abort);
    g_server.on("/api/firmware/finish", HTTP_POST, handle_firmware_finish);
    g_server.on("/api/firmware/chunk", HTTP_POST, handle_firmware_chunk_complete,
                handle_firmware_chunk_upload);
    g_server.on("/api/firmware", HTTP_POST, handle_firmware_complete,
                handle_firmware_upload);
    g_server.on("/api/ha/test", HTTP_POST, handle_ha_test);
    g_server.on("/api/reboot", HTTP_POST, handle_reboot);
    g_server.onNotFound([]() {
        if (!ensure_auth()) return;
        send_error(404, "Not found");
    });
}

}  // namespace

void web_manager_begin() {
    snprintf(g_boot_id, sizeof(g_boot_id), "%08lx%08lx",
             static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
    register_routes();
    g_server.begin();
    Serial0.printf("[Web] Management server started on port %u\n",
                   static_cast<unsigned>(WEB_MANAGER_PORT));
}

void web_manager_loop() {
    firmware_update_expire(millis());
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
        // Malformed multipart envelopes can bypass both upload completion callbacks.
        if (g_firmware_request.seen) {
            if (g_firmware_request.owned)
                firmware_update_abort("Firmware HTTP request did not complete.");
            g_firmware_request = FirmwareRequest{};
        }
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
