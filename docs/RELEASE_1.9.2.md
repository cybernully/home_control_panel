# Home Control Panel v1.9.2

## Live Climate tab

The Climate tab is rebuilt around live Home Assistant state and the same visual
language as Room, Overview, Media, Weather, Calendar, and Security. Web Admin can
select, label, order, and remove up to four `climate.*` entities. It also controls
whether supported humidity, fan-mode, and preset controls are shown.

For the selected device, the panel presents:

- current temperature and Home Assistant temperature unit;
- a single setpoint or heat/cool target range;
- large touch targets that honor the reported setpoint step and min/max limits;
- all reported HVAC modes, with distinct heat/cool/off state colors;
- live HVAC action, humidity, fan mode, preset, availability, and entity identity;
- explicit configured-empty and unavailable states.

All Home Assistant climate parsing and commands stay below the UI state-model
boundary. LVGL consumes a bounded view model and never handles JSON, WebSockets,
or service payloads. Temperature, HVAC mode, fan mode, and preset commands are
queued on the existing Home Assistant worker and wait for subscribed state before
the screen changes.

## Dimmable switches

Brightness is now capability-driven for both `light.*` and `switch.*` entities.
A switch that reports brightness receives the established per-control slider in
Room favorites or group sheets. The worker calls `turn_on` or `turn_off` against
the entity's actual domain, preserving compatibility with integrations that
expose dimmers as switches.

## Configuration and migration

Configuration schema 9 adds `climate_devices`, `climate_show_humidity`,
`climate_show_fan`, and `climate_show_presets`. Existing schema-8 installations
migrate with an empty Climate selection and all three optional presentations
enabled. Other saved layout, Security, calendar, weather, media, and room settings
are preserved.

Host coverage includes strict/atomic Climate configuration parsing, state-model
setpoint and option validation, live/empty/unavailable LVGL rendering, climate
interaction callbacks, Web Admin payload serialization, and a dimmable-switch
slider interaction. Firmware builds cover both 2624 and 2635 environments.
Flashing, touch behavior, and live Home Assistant device behavior still require
validation on the physical panel.
