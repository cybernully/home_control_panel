# Roadmap

## 1.0.x - Base stabilization

- Confirm a clean PlatformIO build on JC8012P4A1C.
- Validate display/touch/network/battery.
- Validate profile switching and SPIFFS persistence.
- Validate one Home Assistant worker under repeated health tests.
- Add reset/diagnostic history as needed.

## 1.1.0 - Home Assistant discovery

- area registry
- device registry
- entity registry
- entity state fetch/cache
- entity classification
- PSRAM-backed state model
- area filtering

Goal: a room profile should eventually need little more than:

```json
{
  "profile": "room",
  "area_id": "living_room"
}
```

## 1.2.0 - Room Controls

- lights
- switches
- fans
- covers
- locks
- sensors
- scenes
- favorites
- entity detail overlay

## 1.3.0 - Media

Home Assistant media players first:

- now playing
- artwork
- play/pause
- previous/next
- volume/mute
- source
- favorites/playlists

Local audio decoding remains a separate optional service rather than a core requirement.

## 1.4.0 - Whole Home

- area browser
- aggregate status
- room tiles
- dynamic room control pages
- whole-home favorites/scenes

## 1.5.0 - Climate + Security

- thermostat controls
- temperature/humidity
- Alarmo
- locks
- garage doors
- active/open sensor summary

## Calendar functionality

Family Calendar remains independent. Selected calendar/weather code can later be ported as modules after the new base is stable.

## OTA

Do not import unvalidated OTA code into the base. Once the hardened calendar OTA path is proven on-device, port that known-good implementation here with interruption diagnostics.
