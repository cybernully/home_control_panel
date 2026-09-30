# Release 1.6.0 - Room-first control surface

Version 1.6.0 rebuilds the panel around a consistent room-first visual system while preserving the existing display driver, storage, OTA, Home Assistant worker, WebSocket subscription, and bounded command queue.

## Architecture

- `ui_state_model` converts configuration and Home Assistant snapshots into UI-neutral room and control view models. It exposes user intents without exposing JSON, HTTP, or WebSocket details to LVGL modules.
- `ui_card` is the shared native-LVGL card primitive. CONTROL, SLIDER, ACTION, NAVIGATION, and STATUS variants share structure, styling, disabled-state behavior, and icon treatment.
- `ui_theme` supplies the navy/cyan visual language used by the shell and existing modules.
- Room widgets are created once and rebound in place. Sliders send a single intent on release and ignore periodic refresh while dragged.

## Room experience

- Four focused favorite controls, four summarized quick-access groups, and a compact room-status strip follow the v1.6 design reference.
- The persistent header shows the selected room, date/time, configured temperature and humidity, online-device count, Home Assistant health, Wi-Fi, battery, and Settings access.
- Tapping the room name cycles configured rooms. Controls remain freely assignable across physical Home Assistant areas.
- Web Admin supports optional per-room temperature and humidity sensor entities and searches their friendly names and entity IDs.

## Validation boundary

Host rendering, interaction tests, parser tests, web checks, and both PlatformIO firmware targets are the intended release gates. These checks do not prove physical touch calibration, live Home Assistant command execution, Wi-Fi quality, battery accuracy, or long-duration device stability; those require an on-device soak test.
