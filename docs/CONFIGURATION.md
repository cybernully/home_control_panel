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
