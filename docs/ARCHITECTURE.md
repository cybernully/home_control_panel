# Architecture

## Core rule

**One source tree, board-specific firmware, many runtime configurations.**

A kitchen calendar panel, living-room controller and whole-home panel can all run the same application image. Device differences live in runtime configuration, not forks of the firmware.

## Startup order

1. Mount SPIFFS and load `/panel.json`.
2. Initialize display/LVGL.
3. Initialize battery service.
4. Build the module registry from the selected profile/configuration.
5. Render the UI shell and first module.
6. Start ESP-Hosted/Wi-Fi.
7. Start the single Home Assistant worker.
8. Start web management.

This preserves the successful **UI first, network second** behavior learned from Family Calendar.

## LVGL rules

- LVGL uses a PSRAM-first custom allocator.
- The shell is persistent.
- Page roots are created at startup.
- Each module's object tree is created lazily on first visit and then remains persistent.
- Data refresh updates existing objects rather than repeatedly deleting/rebuilding screens.
- Network/worker code never calls LVGL.

## Home Assistant

`home_assistant.cpp` owns one FreeRTOS worker at idle priority. v1.0.0 only implements connection/authentication health checks.

Future Home Assistant work should follow this path:

```text
Home Assistant registry/state APIs
        ↓
single HA worker
        ↓
PSRAM-backed model/cache
        ↓
change flags / snapshots
        ↓
UI thread
        ↓
active module update
```

Do not create a second concurrent HTTP/TLS worker.

## Configuration

### SPIFFS

`/panel.json` contains structured non-secret configuration:

- device ID / mDNS hostname
- display name
- profile
- Home Assistant area ID
- enabled modules
- display/backlight settings

### NVS

Namespace `panel_ha` contains Home Assistant URL/token. The token is write-only from the management page.

### Compile-time

v1.0.0 keeps Wi-Fi and web-management credentials in `app_secrets.h`. A later provisioning release can move those to runtime storage without changing the profile/module architecture.

## Profiles are presets

Profiles choose a useful module set. They are not separate firmware variants. A custom module list can override the preset.

## Hardware abstraction

v1.0.0 targets the proven JC8012P4A1C first. If another display family is added later, create another board implementation/PlatformIO environment while leaving config, modules, Home Assistant and application logic shared.
