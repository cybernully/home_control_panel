# Home Control Panel

**Version 1.1.0**

ESP32-P4 / LVGL wall-panel firmware for multiple Home Assistant control panels.
Version 1.1.0 adds the first real on-screen control UI while retaining the
working dual-board hardware split established in 1.0.0.

## Supported hardware

| Environment | Rear label / batch | PlatformIO board |
|---|---|---|
| `jc8012p4a1c_2624` | 10153001 / 2624 | `esp32-p4` |
| `jc8012p4a1c_2635` | 10153001-V3 / 2635 | `esp32-p4_r3` |

The source tree is shared, but each silicon generation gets its own firmware
build.  The current default PlatformIO environment is the V3/2635 development
panel:

```ini
[platformio]
default_envs = jc8012p4a1c_2635
```

Select the 2624 environment before using VS Code's Upload action on an older
panel.

## What's visible in 1.1.0

The `room` profile now has touch-active screens instead of placeholder text:

- **Overview** - area, light, climate and HA status cards plus quick actions.
- **Room** - Main Lights, Lamps, Ceiling Fan and Shades cards; brightness;
  Relax/Bright/Movie scene buttons.
- **Media** - Previous/Play/Next, volume and source controls.
- **Climate** - current temperature, setpoint +/- and mode selection.
- **Security** - alarm mode controls plus lock/garage/motion/smoke status cards.
- **Settings** - live/persistent display brightness, panel identity and HA status.

The UI controls are intentionally usable locally so touch and layout can be
validated now.  Except for display brightness, they are **not yet bound to Home
Assistant entities**.  1.2.0 will add HA area/entity discovery, state caching and
service calls through the existing single HA worker.

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
- One Home Assistant worker; no second concurrent HTTPS worker.
- LVGL calls stay on the UI/main thread.
- Module pages are created lazily and then retained.
- PSRAM-first LVGL allocation.
- Family Calendar remains a separate project.

See `docs/RELEASE_1.1.0.md`, `docs/ARCHITECTURE.md`, `docs/HARDWARE.md`,
`docs/CONFIGURATION.md`, and `docs/ROADMAP.md`.
