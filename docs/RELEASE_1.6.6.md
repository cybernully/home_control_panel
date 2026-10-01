# Release 1.6.6 - Configurable Room status bar

Version 1.6.6 makes all four compact status positions beside the Room selector
configurable independently for each panel room.

## Four reusable status slots

- Each room saves exactly four ordered status-slot definitions.
- A slot can show a Home Assistant entity, the room's online-device count, or
  the room-control health summary.
- Entity slots support sensors, binary sensors, timers, and other subscribed
  Home Assistant entities without becoming controls.
- Labels, icons, highlight colors, active-state lists, and optional active and
  inactive labels are configurable in Web Admin.
- Friendly names longer than the compact caption buffer are safely trimmed by
  UTF-8 byte length, and validation errors identify the exact slot and field.
- Sensors display Home Assistant's unit of measurement. Existing temperature
  and humidity selections migrate into the first two slots automatically.
- Active timers use the live local countdown model introduced in 1.6.5;
  paused and idle states retain their existing presentation.

## Architecture and compatibility

Home Assistant state and attributes are converted to four UI-neutral
`RoomStatusViewModel` values. The Room LVGL module only renders those values and
does not parse entity state, attributes, timer timestamps, or configuration
JSON. Icon and color resolution is shared with the reusable Overview cards.

Pre-1.6.6 room records remain valid. Their temperature and humidity entities
become the first two status slots, followed by Devices Online and Room Controls.
The configured status entities are included in the panel's compact Home
Assistant subscription after saving.

Host parser, web-editor syntax, rendered LVGL Room interactions, project
structure, and both ESP32-P4 firmware variants remain release gates. Physical
touch behavior and live Home Assistant updates still require validation after
flashing a panel.
