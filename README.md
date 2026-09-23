# Home Control Panel

**Version 1.3.4**

ESP32-P4 / LVGL wall-panel firmware for multiple Home Assistant control panels.
Version 1.3.0 adds live Home Assistant media-player discovery, playback controls,
artwork, sources, and browse-media favorites/playlists while retaining the dual-board
hardware split established in 1.0.0.

## Maintenance update 1.3.4

Version 1.3.4 bypasses LVGL's failing Tiny JPEG streaming path and decodes
Home Assistant JPEG artwork directly into a PSRAM-backed RGB565 image with
JPEGDEC.  Baseline JPEGs are rendered at up to 256 pixels on their longest
side.  Progressive JPEGs use JPEGDEC's supported 1/8-size first-scan thumbnail.
It also converts en and em dashes in live media text to ASCII so the built-in
Montserrat fonts do not emit missing-glyph warnings.

## Maintenance update 1.3.3

Version 1.3.3 enables LVGL 9.3's memory-filesystem adapter required by the
Tiny JPEG decoder when compressed artwork is supplied as an in-memory variable
image.  It also verifies that LVGL accepts the image source before removing the
Media placeholder.  This is an incremental update from v1.3.2.

## Maintenance update 1.3.2

Version 1.3.2 fixes a `loopTask` stack-protection crash that occurred after
Home Assistant artwork downloaded successfully and LVGL began decoding it.  It
configures a 16 KB Arduino loop stack and moves the remaining artwork metadata
work buffers into PSRAM-preferred storage.  This release is based on GitHub
commit `ee7bcabea5ebe49100852e0d3fb31d09d34d203c` (v1.3.1).

## Supported hardware

| Environment | Rear label / batch | PlatformIO board |
|---|---|---|
| `jc8012p4a1c_2624` | 10153001 / 2624 | `esp32-p4` |
| `jc8012p4a1c_2635` | 10153001-V3 / 2635 | `esp32-p4_r3` |

The source tree is shared, but each silicon generation gets its own firmware
build. The current default PlatformIO environment is the V3/2635 development
panel:

```ini
[platformio]
default_envs = jc8012p4a1c_2635
```

Select the 2624 environment before using VS Code's Upload action on an older
panel.

## What's new in 1.3.0

The `media` module now binds to real `media_player` entities assigned to the
configured Home Assistant area:

- discovers up to four area media players through the existing 1.2 area lookup;
- shows live player state, title, artist, album, playlist, source, mute, and volume;
- supports play/pause, previous, next, volume, mute, and source selection;
- uses Home Assistant `media_player/browse_media` to surface up to three playable
  favorites/playlists when the integration exposes browse media;
- downloads `entity_picture` artwork on the existing single HA worker, caches the
  encoded image in PSRAM, and renders JPEG/PNG artwork with LVGL;
- never changes media state optimistically: the screen waits for the subscribed
  Home Assistant state update after commands.

The 1.2 room/light/scene bindings remain unchanged. Climate and security remain
preview-only until their later roadmap release.

## Home Assistant configuration

Use the local web manager to configure:

- Home Assistant base URL, for example `http://homeassistant.local:8123`;
- a Home Assistant long-lived access token;
- the Home Assistant area ID or exact area name, for example `living_room` or
  `Living Room`.

The access token is stored in NVS and is never returned by the web manager.

### Discovery path

```text
Home Assistant WebSocket
        |
        +-- area registry lookup
        |
        +-- extract_from_target(area)
        |
        +-- supported entity filtering
        |
        +-- subscribe_entities
        |
        v
PSRAM-backed state cache
        |
        v
LVGL UI thread
```

Control requests take the reverse path through a FreeRTOS queue to the same
Home Assistant worker. The worker performs the REST service call, and the UI
waits for the subsequent Home Assistant state update rather than changing state
optimistically.

## First setup

```bash
cp include/app_secrets.example.h include/app_secrets.h
```

Edit Wi-Fi and web-management credentials in `include/app_secrets.h`.

The `.gitignore` excludes the expected secrets filename plus common alternate
spellings so credentials are not accidentally committed.

## Build/upload 2635 / V3

```bash
~/.platformio/penv/bin/pio run -e jc8012p4a1c_2635
~/.platformio/penv/bin/pio run -e jc8012p4a1c_2635 -t upload
```

## Build/upload 2624

```bash
~/.platformio/penv/bin/pio run -e jc8012p4a1c_2624
~/.platformio/penv/bin/pio run -e jc8012p4a1c_2624 -t upload
```

## Build both

```bash
./scripts/build_all.sh
```

## Profiles

**Room:** `overview,room,media,climate,security,settings`

**Calendar:** `overview,calendar,weather,security,settings`

**Whole home:** `overview,rooms,media,climate,security,settings`

**Custom:** starts with `overview,settings`.

## Architecture rules retained

- UI first, network second.
- One Home Assistant worker; no second concurrent HTTPS/WebSocket worker.
- Home Assistant service requests are serialized on that worker.
- LVGL calls stay on the UI/main thread.
- Module pages are created lazily and then retained.
- PSRAM-first LVGL allocation and a PSRAM-backed Home Assistant state cache.
- Family Calendar remains a separate project.

See `docs/RELEASE_1.3.0.md`, `docs/ARCHITECTURE.md`, `docs/HARDWARE.md`,
`docs/CONFIGURATION.md`, and `docs/ROADMAP.md`.
