# Runtime configuration

Room panel example:

```json
{
  "schema": 10,
  "device_id": "living-room-panel",
  "display_name": "Living Room",
  "profile": "room",
  "area_id": "living_room",
  "modules": ["overview", "room", "media", "climate", "security", "settings"],
  "media_shortcuts": [
    {
      "label": "Skiing",
      "icon": "playlist",
      "entity_id": "media_player.office_echo_studio",
      "media_content_id": "play my skiing playlist",
      "media_content_type": "AMAZON_MUSIC"
    }
  ],
  "backlight": 80,
  "dark_mode": true,
  "screen_timeout_seconds": 120,
  "explicit_layout": true,
  "media_players": ["media_player.office_echo_studio"],
  "room_controls": [
    {"entity_id": "light.desk", "label": "Desk", "placement": 1}
  ]
}
```

Since 1.2.0, `area_id` is active Home Assistant configuration rather than just
display metadata. The panel first matches it against the Home Assistant area
registry. You can provide either the actual area ID (`living_room`) or the
exact area name (`Living Room`). Once resolved, Home Assistant is asked for the
entities referenced by that area.

Supported live area entities in 1.3.0 are:

- `light`
- `switch`
- `fan`
- `cover`
- `scene`
- `media_player`

Media players are live in 1.3.0, including transport, volume, sources, artwork,
and browse-media shortcuts when the integration exposes them. Security is live
in 1.9.1 through Alarmo's standard `alarm_control_panel` entity and services.
Climate is live in 1.9.2 through standard Home Assistant `climate` entities and
services.

## Climate tab (1.9.2)

Choose and order up to four `climate.*` entities in Web Admin. The first item is
selected when the tab opens; the panel renders selector buttons for the rest.
Optional controls hide automatically when the selected entity does not report
the corresponding Home Assistant capability.

```json
"climate_devices": [
  {"entity_id": "climate.office", "label": "Office"},
  {"entity_id": "climate.bedroom", "label": "Bedroom"}
],
"climate_show_humidity": true,
"climate_show_fan": true,
"climate_show_presets": true
```

The panel reads live `current_temperature`, `temperature` or
`target_temp_low`/`target_temp_high`, `min_temp`, `max_temp`, `target_temp_step`,
`hvac_modes`, `hvac_action`, `fan_modes`, and `preset_modes` attributes. It sends
`climate.set_temperature`, `climate.set_hvac_mode`, `climate.set_fan_mode`, and
`climate.set_preset_mode` through the existing Home Assistant worker. No climate
state is changed optimistically in the UI.

Dimmable `switch.*` entities are capability-driven: when Home Assistant reports
a brightness attribute, Room renders the same slider used by a dimmable light.
The resulting brightness command still targets the `switch` domain, so no entity
renaming or helper light is required.

## Weather sources (1.9.3)

Current conditions, hourly forecasts, and daily forecasts can come from three
different `weather.*` entities. The optional hourly and daily values inherit
the current-conditions entity when blank, so schema 9 configurations keep the
same behavior after upgrading.

```json
"weather_entity_id": "weather.home",
"weather_hourly_entity_id": "weather.hourly_station",
"weather_daily_entity_id": "weather.regional_daily"
```

The current source also supplies the optional header temperature. Forecast
service calls are sent independently to their selected hourly or daily source.

## Alarmo security (1.9.1)

Choose the Alarmo `alarm_control_panel` entity and monitored devices in Web
Admin. The PIN is never stored in `panel.json`; disarming always opens the local
keypad and sends the entered code directly through the Home Assistant command
worker.

```json
"alarm_entity_id": "alarm_control_panel.alarmo",
"security_show_abnormal_summary": true,
"security_confirm_arming": true,
"security_code_to_arm": false,
"security_arm_home": true,
"security_arm_away": true,
"security_arm_night": true,
"security_arm_vacation": false,
"security_devices": [
  {
    "entity_id": "binary_sensor.front_door",
    "label": "Front door",
    "icon": "door",
    "abnormal_states": "on,open,opening",
    "normal_label": "Closed",
    "abnormal_label": "Open",
    "color": "red",
    "reverse_abnormal": false
  },
  {
    "entity_id": "binary_sensor.alarm_network",
    "label": "Alarm network",
    "icon": "power",
    "abnormal_states": "on",
    "normal_label": "Online",
    "abnormal_label": "Offline",
    "color": "yellow",
    "reverse_abnormal": true
  }
],
"security_dynamic_devices": [
  {
    "entity_id": "binary_sensor.back_gate",
    "label": "Back gate",
    "icon": "door",
    "abnormal_states": "on",
    "normal_label": "Closed",
    "abnormal_label": "Open",
    "color": "yellow",
    "reverse_abnormal": false
  }
]
```

`reverse_abnormal` means the listed states are healthy and every other live
state is abnormal. An unavailable or missing configured device is always shown
as abnormal so communication failures cannot look like an all-clear condition.
Entries in `security_devices` receive persistent cards. Entries in
`security_dynamic_devices` are evaluated continuously but use no card space
while normal; they appear only in Dynamic Attention when abnormal. Up to eight
persistent monitored devices and sixteen dynamic attention devices may be
configured.

## Media shortcuts

Version 1.7.0 supports up to six persistent one-touch media actions. They
can be entered in the local web manager or added to `media_shortcuts` in
`panel.json`.  All four values are required for each shortcut:

- `label`: text displayed on the panel card;
- `icon`: `music`, `radio`, `podcast`, `playlist`, `favorite`, or `speaker`;
- `entity_id`: target Home Assistant `media_player` entity;
- `media_content_id`: integration-specific media identifier or command;
- `media_content_type`: integration-specific content type.

The target media player must be assigned to the panel's configured Home
Assistant area so it is included in the panel's subscribed entity cache.  A
shortcut is shown but disabled when its target is unavailable or not discovered.

The panel sends shortcuts through `media_player.play_media` using Home
Assistant's nested media structure:

```yaml
data:
  media:
    media_content_id: play my skiing playlist
    media_content_type: AMAZON_MUSIC
    metadata: {}
```

Configured shortcuts and automatically discovered Browse favorites have
separate rows on the Media page. Version 1.5.2 also adds a separate
`media_favorites` list: up to six configured actions displayed first in the
Media > Favorites bubble, followed by any favorites the selected integration
returns dynamically. Each item uses the same fields as a media shortcut.

```json
"media_favorites": [
  {
    "label": "Focus playlist",
    "icon": "favorite",
    "entity_id": "media_player.office_echo_studio",
    "media_content_id": "play my focus playlist",
    "media_content_type": "AMAZON_MUSIC"
  }
]
```

Home Assistant URL/token are stored separately in NVS namespace `panel_ha`.
The token is write-only from the local web manager and is not stored in
`panel.json`.

Calendar-oriented panel:

```json
{
  "schema": 2,
  "device_id": "kitchen-calendar",
  "display_name": "Kitchen",
  "profile": "calendar",
  "area_id": "kitchen",
  "modules": ["overview", "calendar", "weather", "security", "settings"],
  "backlight": 80,
  "dark_mode": true,
  "screen_timeout_seconds": 120
}
```

Whole-home panel:

```json
{
  "schema": 2,
  "device_id": "main-control-panel",
  "display_name": "Whole Home",
  "profile": "whole_home",
  "area_id": "",
  "modules": ["overview", "room", "media", "climate", "security", "settings"],
  "backlight": 80,
  "dark_mode": true,
  "screen_timeout_seconds": 120
}
```

The local web-management page writes the same configuration format to SPIFFS.

## Room layout (1.4.0)

`room_controls` is an optional array of up to 48 entity preferences. Each entry
has `entity_id`, `label` (empty uses the HA name), `placement` (0 grouped,
1 favorite, 2 hidden), and an optional `device_type`. Set `device_type` to
`dimmable` to force a brightness slider when Home Assistant reports the entity
as on/off only. `auto` retains capability-based behavior; `light`, `switch`,
`fan`, `cover`, and `scene` force the corresponding presentation. Array order
controls display order. Six favorites maximum;
labels have a 63-byte UTF-8 limit. Use the authenticated web manager's Room
controls editor; changes apply live after Save. Existing files without this
array load with all supported controls grouped and no favorites.

See [release 1.4.0](RELEASE_1.4.0.md) for examples and hidden-control semantics.

## Web layout manager (1.5.0)

The local web manager now has tabs corresponding to the enabled panel tabs and
an Administration tab. Select **Scan Home Assistant area** from Room to run a
temporary, authenticated area scan; the editor identifies each entity's domain
and presents suitable Room or Media controls. Saving enables `explicit_layout`.
From then on the panel subscribes only to configured `room_controls`,
`media_players`, and media-shortcut targets rather than all supported entities
in the area. Schema-1 files remain in legacy auto-discovery mode until saved by
the 1.5.0 editor.
