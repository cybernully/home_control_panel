# Architecture

## Core rule

**One source tree, board-specific firmware, many runtime configurations.**

A kitchen calendar panel, living-room controller and whole-home panel can all
run the same application source. Device differences live in runtime
configuration and PlatformIO hardware environments, not application forks.

## Startup order

1. Mount SPIFFS and load `/panel.json`.
2. Initialize display/LVGL.
3. Initialize battery service.
4. Build the module registry from the selected profile/configuration.
5. Render the UI shell and first module.
6. Start ESP-Hosted/Wi-Fi.
7. Start the single Home Assistant worker.
8. Start web management.

This preserves the successful **UI first, network second** behavior learned
from Family Calendar.

## LVGL rules

- LVGL uses a PSRAM-first custom allocator.
- The shell is persistent.
- Page roots are created at startup.
- Each module's object tree is created lazily on first visit and then remains
  persistent.
- Data refresh updates existing objects rather than repeatedly
  deleting/rebuilding screens.
- Network/worker code never calls LVGL.

## Home Assistant - 1.3.0

`home_assistant.cpp` owns the **only Home Assistant network worker**.

The worker maintains a persistent authenticated Home Assistant WebSocket for
area discovery and live room state. It also serializes REST health checks and
service calls. No second TLS worker is created.

### Discovery and state flow

```text
config/area_registry/list
        |
        v
extract_from_target(area)
        |
        v
filter: light / switch / fan / cover / scene / media_player
        |
        v
subscribe_entities(filtered IDs)
        |
        v
single HA worker
        |
        v
PSRAM-backed entity model
        |
        v
snapshot API protected by a short critical section
        |
        v
LVGL UI/main thread
```

This deliberately avoids downloading all entity/device registry metadata to
the panel. Home Assistant resolves the area/device/entity relationships for
the configured target and the panel retains only the supported entities it
needs.

### Control flow

```text
LVGL callback
        |
        v
bounded FreeRTOS action queue
        |
        v
same HA worker
        |
        v
POST /api/services/<domain>/<service>
        |
        v
Home Assistant
        |
        v
subscribe_entities state update
        |
        v
UI refresh
```

The UI does not optimistically change device state. It waits for Home
Assistant's state subscription to confirm the resulting state.

The existing one-second REST inter-request spacing remains in force. WebSocket
processing and REST service calls therefore share the same worker rather than
creating concurrent Home Assistant networking tasks.

### Media artwork

Media metadata stays in the PSRAM-backed HA entity cache. When the selected
`media_player` exposes `entity_picture`, the HA worker fetches the encoded image
with a bounded 256 KB limit. The UI copies a completed cache generation into its
own PSRAM buffer and LVGL decodes JPEG/PNG on the UI thread. Network code never
calls LVGL and LVGL never reads a buffer while the worker is replacing it.

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

Namespace `panel_ha` contains Home Assistant URL/token. The token is write-only
from the management page.

### Compile-time

Wi-Fi and web-management credentials currently remain in `app_secrets.h`.
Provisioning can move those to runtime storage without changing the
profile/module architecture.

## Profiles are presets

Profiles choose a useful module set. They are not separate application forks.
A custom module list can override the preset.

## Hardware abstraction

Two PlatformIO environments currently target the JC8012P4A1C hardware family:

- `jc8012p4a1c_2624` for the earlier P4 target.
- `jc8012p4a1c_2635` for production P4 v3.x silicon.

If another display family is added later, create another board
implementation/PlatformIO environment while leaving config, modules, Home
Assistant and application logic shared.
