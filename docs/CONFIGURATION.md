# Runtime configuration

Room panel example:

```json
{
  "schema": 2,
  "device_id": "living-room-panel",
  "display_name": "Living Room",
  "profile": "room",
  "area_id": "living_room",
  "modules": ["overview", "room", "media", "climate", "security", "settings"],
  "media_shortcuts": [
    {
      "label": "Skiing",
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
and browse-media shortcuts when the integration exposes them. Climate and
security remain preview modules until their roadmap releases.

## Media shortcuts

Version 1.3.9 supports up to three persistent one-touch media actions.  They
can be entered in the local web manager or added to `media_shortcuts` in
`panel.json`.  All four values are required for each shortcut:

- `label`: text displayed on the panel button;
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
separate rows on the Media page, with three buttons available in each row.

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
  "modules": ["overview", "rooms", "media", "climate", "security", "settings"],
  "backlight": 80,
  "dark_mode": true,
  "screen_timeout_seconds": 120
}
```

The local web-management page writes the same configuration format to SPIFFS.

## Room layout (1.4.0)

`room_controls` is an optional array of up to 48 entity preferences. Each entry
has `entity_id`, `label` (empty uses the HA name), and `placement` (0 grouped,
1 favorite, 2 hidden). Array order controls display order. Six favorites maximum;
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
