# Runtime configuration

Room panel example:

```json
{
  "schema": 1,
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
  "screen_timeout_seconds": 120
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
  "schema": 1,
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
  "schema": 1,
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
