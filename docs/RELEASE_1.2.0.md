# Release 1.2.0 - Home Assistant Area Bindings

## Purpose

Version 1.2.0 replaces the Room module's local preview state with actual Home
Assistant discovery, live state, and service calls.

## Home Assistant discovery

The panel authenticates to Home Assistant's WebSocket API and:

1. resolves the configured panel area;
2. asks Home Assistant to extract the devices/entities referenced by that area;
3. filters the result to `light`, `switch`, `fan`, `cover`, and `scene`;
4. subscribes only to those entities with `subscribe_entities`;
5. maintains their compressed state updates in a PSRAM-backed model.

The model is capped at `HA_MAX_AREA_ENTITIES` (24 by default) so a bad area
assignment cannot consume unbounded panel memory.

## Live room controls

The Room screen selects up to four representative controls, preferring one
each from light, switch, fan, and cover before filling remaining slots.

Device state and friendly names come from Home Assistant. Button presses are
queued to the single Home Assistant worker and are not reflected as successful
until Home Assistant reports the resulting state.

Area brightness targets all discovered lights. A value of zero turns the area
lights off; nonzero values call `light.turn_on` with `brightness_pct`.

Up to three Home Assistant scenes assigned to the configured area are shown
automatically.

## Overview

The Overview screen now shows the actual discovered light count/on count and
Home Assistant discovery status. Its all-lights button and up to three area
scene buttons are live.

Climate remains labeled as planned for 1.5.0 rather than displaying a fake
temperature.

## Settings

Settings adds Home Assistant discovery details and a **Rediscover Area**
button. This is useful after assigning or moving entities in Home Assistant
without changing panel configuration.

## Network architecture

There is still only one Home Assistant worker.

- WebSocket discovery/state is handled by that worker.
- REST health checks and REST service calls are handled by that same worker.
- REST calls preserve the existing one-second minimum inter-request spacing.
- The network worker never calls LVGL.
- LVGL reads fixed-size snapshots from the state model on the main/UI thread.

## Not included

Media, climate, and security service bindings are intentionally deferred to
their roadmap releases. Their 1.1 UI remains available as preview screens.

OTA is also still intentionally excluded until the hardened Family Calendar
OTA path is proven and intentionally ported.

## ESP32-P4 / arduinoWebSockets compile note

The ESP32-P4 pioarduino framework does not expose every Arduino framework
library include directory to third-party library compilation automatically.
`platformio.ini` therefore explicitly adds the framework `Network`, `WiFi`, and
`NetworkClientSecure` include roots so `arduinoWebSockets` can resolve
`WiFi.h` and `WiFiClientSecure.h` on both panel environments.
