# Home Control Panel

**Version 1.2.0**

ESP32-P4 / LVGL wall-panel firmware for multiple Home Assistant control panels.
Version 1.2.0 turns the room and overview screens into live Home Assistant
controls while retaining the dual-board hardware split established in 1.0.0.

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

## What's new in 1.2.0

The `room` profile now discovers and controls Home Assistant entities assigned
to the configured area:

- resolves the configured Home Assistant area;
- resolves devices/entities in that area without downloading every registry
  entry to the panel;
- subscribes only to supported room entities for live state;
- stores the room state model in PSRAM;
- discovers `light`, `switch`, `fan`, `cover`, and `scene` entities;
- drives the Room and Overview UI from Home Assistant state instead of local
  preview values;
- sends light/switch/fan/cover/scene service calls through the existing single
  Home Assistant worker;
- provides area-wide light brightness and all-lights controls;
- adds a Settings **Rediscover Area** action and discovery status.

Media, climate, and security remain preview-only in this release. Their real
bindings remain scheduled for later roadmap releases.

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

See `docs/RELEASE_1.2.0.md`, `docs/ARCHITECTURE.md`, `docs/HARDWARE.md`,
`docs/CONFIGURATION.md`, and `docs/ROADMAP.md`.
