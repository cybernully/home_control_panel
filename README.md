# Home Control Panel

**Version 1.7.1**

ESP32-P4 / LVGL wall-panel firmware for multiple Home Assistant control panels.
Version 1.3.0 adds live Home Assistant media-player discovery, playback controls,
artwork, sources, and browse-media favorites/playlists while retaining the dual-board
hardware split established in 1.0.0.

## Web Admin header 1.7.1

The Web Admin identity, connection state, navigation, Save, and Reboot controls
remain visible in a fixed application header while long configuration pages
scroll. Save uses the existing validated form submission path and remains
disabled until configuration loading succeeds. See
[1.7.1 release notes](docs/RELEASE_1.7.1.md).

## Configurable Overview 1.6.4

Overview now uses the same reusable card language as Room. Its ordered four-column
layout is configured from Web Admin and can mix built-in summaries, status-only
Home Assistant entities, and actionable controls. Per-card settings include label,
width, icon family, active states and color, state labels, tap behavior, and optional
confirmation. Covers support garage-door presentation and open/close confirmation;
locks, binary sensors, lights, fans, switches, scenes, weather, and selected sensors
use the same extensible state/UI contract. Existing Overview widgets and quick actions
are migrated automatically. See [1.6.4 release notes](docs/RELEASE_1.6.4.md).

## Home Assistant stability 1.4.4

The Room page now has six explicitly chosen favorites and four touch-friendly
bubble groups: Lights, Devices (switches/fans), Shades, and Scenes. Each group
opens a paginated popup with six controls per page. Configure favorites, hidden
controls, display names, and order in the local web manager. Changes apply after
saving, without a reboot. Existing configurations load with all controls grouped
and no arbitrary favorites. Dimmable lights have individual brightness sliders
in the Lights popup; the old room-wide brightness slider is replaced to avoid
changing hidden lights. En/em dashes are normalized for the bundled font.

The header has also been refined with aligned battery geometry, matching status
capsules, Wi-Fi signal bars, and a cleaner title/subtitle hierarchy.

Media follows the same visual language: one spacious now-playing card, icon-led
playback and volume controls, up to six optional quick-play cards, and rounded
Players, Sources, and Favorites bubbles. Web Admin orders up to six players and
configures shortcut/favorite labels, icons, targets, content IDs, and types.
Unused slots are hidden, long metadata is bounded, canceled volume drags do not
send a command, and title/artist changes refresh artwork even when an integration
reuses the same proxy URL.

See [1.7.0 release notes](docs/RELEASE_1.7.0.md) for the complete Media rebuild,
[1.4.4 release notes](docs/RELEASE_1.4.4.md) for seamless media artwork reconnects,
and [1.4.0 release notes](docs/RELEASE_1.4.0.md) for room configuration.

## Maintenance update 1.3.10

Version 1.3.10 adds the panel's current Wi-Fi IP address to the on-device
Settings page.  The address is refreshed with the rest of the Settings status
and changes to `Offline` whenever the network is disconnected.

## Maintenance update 1.3.9

Version 1.3.9 adds three persistent Media Shortcut buttons configured from the
local web manager.  Each shortcut stores its label, target `media_player`, media
content ID, and media content type.  Shortcuts use Home Assistant's current
nested `media_player.play_media` payload and appear in their own row above the
three automatically discovered Browse favorites.  Existing `panel.json` files
remain valid and load with no shortcuts until they are configured.

## Maintenance update 1.3.8

Version 1.3.8 refines the Media dashboard after the full-card artwork design in
v1.3.7.  Artwork now occupies a dedicated 320x320 well on the left while the
title, metadata, playback controls, and status return to a focused right-hand
column.  Images preserve their aspect ratio, are centered, and may be reduced
to fit, but are never enlarged beyond their decoded resolution.  This avoids
wide cropping and keeps 256x256 Home Assistant artwork pixel-sharp.

## Maintenance update 1.3.7

Version 1.3.7 redesigns the Media dashboard around a full-card, cover-cropped
artwork background with a translucent readability layer and a clearer metadata
hierarchy.  Playback controls are larger and centered.  Volume retains its
slider while adding dedicated decrement and increment buttons backed by Home
Assistant's native `volume_down` and `volume_up` services, and mute is now a
compact secondary control.  The page continues to reuse one PSRAM-backed RGB565
artwork buffer and adds no second decoded image.

## Maintenance update 1.3.6

Version 1.3.6 replaces the low-resolution progressive-JPEG fallback with a
full-resolution progressive decoder.  Progressive artwork is decoded with the
vendored stb_image v2.30 implementation, converted to PSRAM-backed RGB565, and
then fitted to the 202x202 artwork viewport.  Baseline JPEGs continue using the
faster JPEGDEC path introduced in v1.3.4.

## Maintenance update 1.3.5

Version 1.3.5 scales decoded artwork both down and up to fit the 202x202 Media
artwork viewport while preserving its aspect ratio.  This specifically enlarges
the 1/8-size thumbnail produced for progressive JPEG artwork instead of leaving
it at its native 32x32 size.

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

The 1.4 Room page refines the existing room/light/scene bindings. Climate and security remain
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
