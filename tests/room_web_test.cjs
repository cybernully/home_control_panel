const fs=require('node:fs'),assert=require('node:assert/strict');
const source=fs.readFileSync('include/web_ui.h','utf8');

// The live UI is a self-contained firmware asset. These checks keep the
// layout editor's essential routes and safe rendering helpers from regressing
// without needing a browser or a Home Assistant server in host CI.
for(const feature of [
  "const $=id=>document.getElementById(id)", "function esc(s)",
  "Rooms and controls", "Media experience", "Administration", "Search Home Assistant",
  "/api/ha/discover", "/api/ha/entities", "room_controls",
  "media_players", "mediaActionCard", "Favorites bubble menu", "media_favorites", "Save changes",
  "explicit layout", "modules.join(',')", "Overview layout",
  "widget_catalog", "overview_widgets", "overview_items",
  "overviewBuiltinLabels", "weather:'Weather'", "calendar:'Calendar'"
]) assert(source.includes(feature),`missing web manager feature: ${feature}`);
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
for(const feature of ["function ensureWidget(type)", "Add Calendar widget", "Add Weather widget",
                     "Edit room controls", "function renderLinkedPages()"])
  assert(source.includes(feature),`missing working panel-tab configuration: ${feature}`);
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
assert.equal((source.match(/>Reboot</g)||[]).length,1,'Reboot must have one visible Web Admin action');
assert(!source.includes('https://cdn.'),'the management UI must not require a public CDN');
assert(source.indexOf("function esc(s)")<source.indexOf('function renderCandidates()'),
       'escape helper must be defined before entity HTML rendering');
console.log('Web layout manager tests passed: unified Overview cards, configurable Room status slots, typed HA scan, explicit layout payload, and safe rendering hooks.');
