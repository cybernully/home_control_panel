# Home Control Panel

**Version 1.0.0**

A clean ESP32-P4 / LVGL base firmware for multiple wall displays.  The project
uses one source tree and runtime panel configuration, with two PlatformIO build
environments for the two known JC8012P4A1C hardware generations.

## Supported hardware

| Environment | Rear label / batch | PlatformIO board | Notes |
|---|---|---|---|
| `jc8012p4a1c_2624` | 10153001 / 2624 | `esp32-p4` | Existing/original panel target |
| `jc8012p4a1c_2635` | 10153001-V3 / 2635 | `esp32-p4_r3` | Production ESP32-P4 v3.x; tested unit reports v3.2 |

Both use the same application code and the same `JC8012P4A1-V2` JD9365 panel
initialization/profile.  The display bootstrap selects the MIPI-DSI PHY PLL
reference source from the actual ESP32-P4 silicon revision at runtime.

The important distinction is compile-time: the 2624 and 2635 use different
PlatformIO ESP32-P4 board targets, so they produce two `firmware.bin` files.

## Why this is separate from Family Calendar

Family Calendar remains its own project and can continue evolving independently.
This project reuses the hardware, networking, LVGL memory, battery and UI-thread
lessons from the calendar without turning that application into a universal
controller.

## v1.0.0 foundation

- One source tree for all control panels.
- Board-specific firmware builds for 2624 and 2635 P4 silicon.
- Per-device profile + Home Assistant area + module configuration.
- Structured non-secret configuration in SPIFFS `/panel.json`.
- Home Assistant URL/token in NVS; the token is never returned by the management API.
- One low-priority Home Assistant worker.
- UI-first startup: LVGL renders before ESP-Hosted/TLS work begins.
- Lazy persistent module pages.
- Web management for device identity, profile, area, modules and HA credentials.
- Battery monitoring on GPIO52.
- Proven JC8012P4A1C framebuffer/touch path.
- PSRAM-first LVGL allocator.
- Dual 6 MB application slots retained for future OTA.

The feature modules are intentionally boundaries/placeholders in this base:

`overview, calendar, weather, room, rooms, media, climate, security, settings`

## Profiles

**Calendar:** `overview,calendar,weather,security,settings`

**Room controller:** `overview,room,media,climate,security,settings`

**Whole home:** `overview,rooms,media,climate,security,settings`

**Custom:** starts with `overview,settings`, then you choose modules.

## First setup

```bash
cp include/app_secrets.example.h include/app_secrets.h
```

Edit Wi-Fi and web-management credentials in `include/app_secrets.h`.

## Build and upload the 2624

```bash
~/.platformio/penv/bin/pio run -e jc8012p4a1c_2624
~/.platformio/penv/bin/pio run -e jc8012p4a1c_2624 -t upload
```

Firmware output:

```text
.pio/build/jc8012p4a1c_2624/firmware.bin
```

## Build and upload the 2635 / V3

```bash
~/.platformio/penv/bin/pio run -e jc8012p4a1c_2635
~/.platformio/penv/bin/pio run -e jc8012p4a1c_2635 -t upload
```

Firmware output:

```text
.pio/build/jc8012p4a1c_2635/firmware.bin
```

The 2635 first flash uses the production-silicon `esp32-p4_r3` target and the
55.03.39 pioarduino platform that successfully flashed the v3.2 device.

## Build both

```bash
./scripts/build_all.sh
```

## Configuration

On first boot, firmware creates `/panel.json` automatically if SPIFFS is empty.
The same runtime profiles/configuration work on either board build.

## OTA

OTA is intentionally not included in the 1.0.0 base yet.  Once the hardened
Family Calendar OTA path is proven, the known-good implementation can be ported
without changing the dual-board architecture.

See `docs/ARCHITECTURE.md`, `docs/HARDWARE.md`, `docs/CONFIGURATION.md`, and
`docs/ROADMAP.md`.
