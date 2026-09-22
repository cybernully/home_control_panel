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
  "backlight": 80,
  "dark_mode": true,
  "screen_timeout_seconds": 120
}
```

In 1.2.0, `area_id` is active Home Assistant configuration rather than just
display metadata. The panel first matches it against the Home Assistant area
registry. You can provide either the actual area ID (`living_room`) or the
exact area name (`Living Room`). Once resolved, Home Assistant is asked for the
entities referenced by that area.

Supported live room entities in 1.2.0 are:

- `light`
- `switch`
- `fan`
- `cover`
- `scene`

Media, climate, and security remain preview modules until their roadmap
releases.

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
