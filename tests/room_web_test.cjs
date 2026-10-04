const fs=require('node:fs'),assert=require('node:assert/strict');
const source=fs.readFileSync('include/web_ui.h','utf8');

// The live UI is a self-contained firmware asset. These checks keep the
// layout editor's essential routes and safe rendering helpers from regressing
// without needing a browser or a Home Assistant server in host CI.
for(const feature of [
  "const $=id=>document.getElementById(id)", "function esc(s)",
  "Find and add controls", "Media experience", "Administration", "Search Home Assistant",
  "/api/ha/discover", "/api/ha/entities", "room_controls",
  "media_players", "mediaActionCard", "Favorites bubble menu", "media_favorites", "Save changes",
  "explicit layout", "modules.join(',')", "Overview layout",
  "widget_catalog", "overview_widgets", "overview_items",
  "overviewBuiltinLabels", "weather_current:'Current weather'", "weather_hourly:'Hourly forecast'", "weather_daily:'Multi-day forecast'", "calendar:'Calendar'"
]) assert(source.includes(feature),`missing web manager feature: ${feature}`);
for(const feature of ["v1.9.5 Web Admin design system", "Sync devices", "function roomSearchChanged",
                      "function scanRoomEntities", "function addRoomManual", "mergeEntityCandidates",
                      "encodeURIComponent(query)", "light.office_office_ceiling_fan_light",
                      "Searches run directly in Home Assistant", "#room_fields{display:grid"])
  assert(source.includes(feature),`missing v1.9.5 Web Admin feature: ${feature}`);
for(const feature of ["v1.9.6: larger dynamic-attention collection", "securityDynamicLimit=16",
                     "function reorderRoomFavorite", "function moveRoomFavorite",
                     "function decorateRoomFavoriteOrdering", "Favorite ${position+1}",
                     "Move favorite ${entry.item.name||entry.item.entity_id} earlier"])
  assert(source.includes(feature),`missing v1.9.6 ordering/capacity feature: ${feature}`);
for(const feature of ["v2.0.0: authenticated backup/restore", "Backup, restore, and firmware",
                     "function downloadConfigBackup", "function restoreConfigBackup",
                     "function installFirmwareUpdate", "/api/config/backup",
                     "/api/config/restore", "/api/firmware", "firmware_progress",
                     "correct 2624 or 2635", "reconnectAfterMaintenance"])
  assert(source.includes(feature),`missing v2.0.0 maintenance feature: ${feature}`);
assert(source.includes("known=['overview','room','media','calendar','weather','climate','security']"),
  'Web Admin navigation must omit the placeholder Settings page');
assert(source.includes("$('settings')?.remove()"),
  'the unused Web Admin Settings page must be removed from the DOM');
const embeddedScript=source.match(/<script>([\s\S]*?)<\/script>/)?.[1];
assert(embeddedScript,'embedded Web Admin script must be present');
assert.doesNotThrow(()=>new Function(embeddedScript),'embedded Web Admin JavaScript must parse');
assert(source.includes('renderWidgets=function()'),'Overview cards must have an editable renderer');
assert(!source.includes('renderWidgets=function(){readOverviewItems();'),
  'Overview rendering must not reread stale index-bound DOM after cards have moved');
for(const feature of ["function addOverviewEntity", "overviewEntityDefault", "active_states",
                     "Confirm before running tap action", "overviewDragStart", "overviewDrop",
                     "Garage door", "binary_sensor", "Status only", "addOverviewManual",
                     "Manual cards subscribe to any valid Home Assistant entity",
                     "Action target", "overview_action_entity_id", "overviewActionOptions",
                     "Timers", "e.domain==='timer'", "timer:'Timer'"])
  assert(source.includes(feature),`missing configurable Overview-card feature: ${feature}`);
for(const feature of ["Room status bar", "roomStatusDefaults", "ensureRoomStatus",
                     "readRoomStatusFields", "setRoomStatusType", "applyRoomStatusEntity",
                     "Home Assistant entity", "Devices online", "Room controls health",
                     "sensor, binary_sensor, timer...", "function utf8Trim",
                     "label:utf8Trim(entity.name||entity.entity_id,23)"])
  assert(source.includes(feature),`missing configurable Room-status feature: ${feature}`);
for(const feature of ["Dimmable light (brightness slider)",
                     "Dimmable light forces a brightness slider",
                     "['dimmable','Dimmable light (brightness slider)']"])
  assert(source.includes(feature),`missing explicit dimmable Room-control behavior: ${feature}`);

const reorderSource=source.match(/function reorderOverviewItems\(from,to\)\{[\s\S]*?return true\}/)?.[0];
assert(reorderSource,'Overview reordering must use one complete-object move helper');
const network={label:'Network',type:'network',span:1};
const allLights={label:'All Lights',type:'all_lights',span:2};
const garage={label:'Garage Door',type:'entity',entity_id:'binary_sensor.garage_door',action_entity_id:'switch.garage_relay',span:2,
  active_states:'open,opening',active_label:'Open',inactive_label:'Closed',confirm:true};
const reorderTest=Function('overviewItems',`${reorderSource};return {move:reorderOverviewItems,items:overviewItems}`)([network,allLights,garage]);
assert.equal(reorderTest.move(2,1),true,'a valid Overview card move should succeed');
assert.strictEqual(reorderTest.items[1],garage,'reordering must move the complete Garage Door card object');
assert.strictEqual(reorderTest.items[2],allLights,'reordering must preserve the displaced card object');
assert.equal(reorderTest.items[1].active_states,'open,opening','specialized card state settings must stay attached');
assert.equal(reorderTest.items[1].action_entity_id,'switch.garage_relay','separate action targets must stay attached');
assert.equal(reorderTest.items[1].confirm,true,'specialized card confirmation settings must stay attached');
for(const feature of ["function ensureWidget(type)", "Add Calendar card", "Add current weather",
                     "Edit room controls", "function renderLinkedPages()"])
  assert(source.includes(feature),`missing working panel-tab configuration: ${feature}`);
for(const feature of ["weather_layout", "weather_show_current", "weather_show_hourly",
                     "weather_show_daily", "weather_header_enabled", "weather_entities",
                     "weather_hourly_entity_id", "weather_daily_entity_id",
                     "Blank forecast sources inherit current conditions"])
  assert(source.includes(feature),`missing v1.7.2 Weather configuration: ${feature}`);
for(const feature of ["Calendar tab", "calendar_sources", "calendar_add_entity",
                     "calendar_week_starts_monday", "calendar_days", "Days shown",
                     "1 day", "3 days", "Full 7-day week", "function addCalendar",
                     "function moveCalendar", "configured calendar view"])
  assert(source.includes(feature),`missing v1.8.1 Calendar configuration: ${feature}`);
assert(source.includes('function showModule(id)'),'show-tab buttons must update the web navigation immediately');
assert(source.includes('onclick="showModule(\'${m}\')"'),'show-tab buttons must use the explicit handler');
assert(source.match(/<section id="media"[\s\S]*?Refresh Home Assistant media/),'Media must expose its own HA search trigger');
for(const feature of ["Up to six icon-led shortcuts", "moveMediaPlayer", "moveMediaAction",
                     "mediaShortcuts", "mediaFavorites", "media_shortcuts",
                     "media_favorites", "DEFAULT PLAYER", "POPUP FAVORITES"])
  assert(source.includes(feature),`missing v1.7 Media editor feature: ${feature}`);
for(const feature of ["class=\"app-header\"", "position:sticky", "id=\"header_save\"",
                     "onclick=\"savePanelConfig()\"", "onclick=\"reboot()\"",
                     "config_submit_guard", "MutationObserver(syncHeaderSave)"])
  assert(source.includes(feature),`missing v1.7.1 fixed-header feature: ${feature}`);
for(const feature of ["Alarmo security", "alarm_entity_id", "security_devices",
                     "security_show_abnormal_summary", "security_confirm_arming",
                     "security_code_to_arm", "security_arm_vacation",
                     "function renderSecurity", "function addSecurityDevice",
                     "reverse_abnormal", "Reverse abnormal logic",
                     "PINs are entered only on the panel keypad and are never stored"])
  assert(source.includes(feature),`missing v1.9.0 Security configuration: ${feature}`);
assert(source.includes("p.set('security_devices',JSON.stringify(securityDevices))"),
  'Security device entries must be serialized as complete objects');
for(const feature of ["Dynamic attention devices", "securityDynamicDevices",
                     "security_dynamic_devices", "addSecurityDynamicDevice",
                     "attention-only devices reuse the rule model"])
  assert(source.includes(feature),`missing v1.9.1 dynamic Security feature: ${feature}`);
assert(source.includes("o.body.set('security_dynamic_devices',JSON.stringify(securityDynamicDevices))"),
  'Dynamic Security device entries must be serialized as complete objects');
const roomFavoriteSource=source.match(/function reorderRoomFavorite\(items,roomIndex,index,direction\)\{[\s\S]*?return true\}/)?.[0];
assert(roomFavoriteSource,'Room favorites must use one room-scoped complete-object reorder helper');
const officeA={entity_id:'light.office_a',room_index:0,placement:1,label:'A'};
const grouped={entity_id:'switch.grouped',room_index:0,placement:0,label:'Grouped'};
const bedroom={entity_id:'light.bedroom',room_index:1,placement:1,label:'Bedroom'};
const officeB={entity_id:'light.office_b',room_index:0,placement:1,label:'B'};
const favoriteOrderTest=Function(`${roomFavoriteSource};return reorderRoomFavorite`)([officeA,grouped,bedroom,officeB]);
const favoriteRows=[officeA,grouped,bedroom,officeB];
assert.equal(favoriteOrderTest(favoriteRows,0,3,-1),true,'a valid Room favorite move should succeed');
assert.strictEqual(favoriteRows[0],officeB,'later favorite must move earlier as one complete object');
assert.strictEqual(favoriteRows[1],grouped,'grouped control must keep its relative slot');
assert.strictEqual(favoriteRows[2],bedroom,'another room must remain untouched');
assert.strictEqual(favoriteRows[3],officeA,'displaced favorite must retain its complete object');
for(const feature of ["Climate tab", "climateDevices", "climate_devices",
                     "climate_show_humidity", "climate_show_fan", "climate_show_presets",
                     "function renderClimateAdmin", "function addClimateDevice",
                     "capability-driven panel controls"])
  assert(source.includes(feature),`missing v1.9.2 Climate configuration: ${feature}`);
assert(source.includes("o.body.set('climate_devices',JSON.stringify(climateDevices))"),
  'Climate devices must be serialized as complete ordered objects');
assert.equal((source.match(/>Reboot</g)||[]).length,1,'Reboot must have one visible Web Admin action');
assert(!source.includes('https://cdn.'),'the management UI must not require a public CDN');
assert(source.indexOf("function esc(s)")<source.indexOf('function renderCandidates()'),
       'escape helper must be defined before entity HTML rendering');
console.log('Web layout manager tests passed: v2 maintenance, Settings-page removal, responsive layout, scoped Room favorites, and safe rendering hooks.');
