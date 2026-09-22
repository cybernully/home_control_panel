# Roadmap

## 1.0.0 - Dual-board foundation

- ESP32-P4 2624 and V3/2635 build environments.
- Display, touch, PSRAM, ESP-Hosted networking and battery foundation.
- Runtime panel profile/configuration.
- One Home Assistant worker.
- Web management.

## 1.1.0 - Visible Control UI

- Interactive Overview dashboard.
- Room light/fan/shade control cards.
- Brightness and scene controls.
- Media transport, source and volume UI.
- Climate setpoint and mode UI.
- Security/alarm and sensor UI.
- Real local backlight control in Settings.

The non-settings controls are deliberately local UI state in this release.  They
do not claim to represent Home Assistant entity state yet.

## 1.2.0 - Home Assistant discovery + bindings

- area registry
- device registry
- entity registry
- state fetch/cache
- entity classification
- PSRAM-backed state model
- area filtering
- light/switch/fan/cover service calls
- UI state driven by Home Assistant rather than local preview state

Goal: a room profile should eventually need little more than:

```json
{
  "profile": "room",
  "area_id": "living_room"
}
```

## 1.3.0 - Media bindings

- media player discovery
- now playing and artwork
- play/pause, previous/next
- volume/mute
- source and favorites/playlists

## 1.4.0 - Whole Home

- area browser
- aggregate status
- room tiles
- dynamic room control pages
- whole-home favorites/scenes

## 1.5.0 - Climate + Security bindings

- thermostat service/state integration
- temperature/humidity
- Alarmo
- locks
- garage doors
- active/open sensor summary

## Calendar functionality

Family Calendar remains independent.  Selected calendar/weather functionality
can be ported as modules without merging the two projects.

## OTA

Do not import unvalidated OTA code into the base.  Once the hardened calendar
OTA path is proven on-device, port that known-good implementation here with
interruption diagnostics.
