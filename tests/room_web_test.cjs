const fs=require('node:fs'),assert=require('node:assert/strict');
const source=fs.readFileSync('include/web_ui.h','utf8');

// The live UI is a self-contained firmware asset. These checks keep the
// layout editor's essential routes and safe rendering helpers from regressing
// without needing a browser or a Home Assistant server in host CI.
for(const feature of [
  "const $=id=>document.getElementById(id)", "function esc(s)",
  "Room controls", "Media players", "Administration", "Scan Home Assistant area",
  "/api/ha/discover", "/api/ha/entities", "room_controls",
  "media_players", "shortcut_label_", "Save layout and configuration",
  "explicit layout", "modules.join(',')", "Available widgets",
  "widget_catalog", "overview_widgets", "Full width · 4 columns",
  "widgetLabels={home_status", "weather:'Weather'", "calendar:'Calendar'"
]) assert(source.includes(feature),`missing web manager feature: ${feature}`);
assert(source.includes('function renderWidgets()'),'overview widgets must have an editable renderer');
for(const feature of ["Configured buttons", "function addQuickAction()", "overview_quick_actions",
                     "Display height", "All Lights", "Toggle entity", "Activate scene"])
  assert(source.includes(feature),`missing configurable quick actions feature: ${feature}`);
for(const feature of ["function ensureWidget(type)", "Add Calendar widget", "Add Weather widget",
                     "Edit room controls", "function renderLinkedPages()"])
  assert(source.includes(feature),`missing working panel-tab configuration: ${feature}`);
assert(source.includes('function showModule(id)'),'show-tab buttons must update the web navigation immediately');
assert(source.includes('onclick="showModule(\'${m}\')"'),'show-tab buttons must use the explicit handler');
assert(source.match(/<section id="media"[\s\S]*?Scan Home Assistant area/),'Media must expose its own HA scan trigger');
assert(!source.includes('https://cdn.'),'the management UI must not require a public CDN');
assert(source.indexOf("function esc(s)")<source.indexOf('function renderCandidates()'),
       'escape helper must be defined before entity HTML rendering');
console.log('Web layout manager tests passed: local tabbed editor, typed HA scan, explicit layout payload, and safe rendering hooks.');
