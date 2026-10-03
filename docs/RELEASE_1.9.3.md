# Home Control Panel v1.9.3

## Panel experience

- Removes the redundant `HA LIVE` badge from Climate.
- Rebuilds Climate as three balanced cards for live conditions, temperature
  adjustment, and HVAC/fan/preset controls, matching the visual language used
  by the other primary tabs.
- Adds clearer vertical breathing room between footer navigation icons and
  their captions while retaining the existing touch targets.
- Separates the Climate device selector from the subtitle and content cards,
  with ellipsis truncation for long device labels.

## Dimmable-light correction

- Adds an explicit **Dimmable light (brightness slider)** behavior to every
  Room control in Web Admin. This persists as `device_type: "dimmable"` and
  overrides integrations that expose a real dimmer as an on/off-only device.
- Detects dimming support from Home Assistant's `supported_color_modes`, so a
  light remains dimmable while off even when its current `brightness` attribute
  is absent.
- Keeps that learned capability across compressed state updates and avoids a
  stale off-state update rejecting an already queued brightness command.
- Commits a slider drag when a finger is lifted just outside the narrow track,
  while guaranteeing that a following release event cannot send a duplicate.

## Weather sources

- Adds optional, independent `weather.*` selections for hourly and daily
  forecasts in Web Admin.
- Keeps `weather_entity_id` as the current-conditions and header source.
- Leaves existing installations migration-safe: either blank forecast source
  inherits the current-conditions entity.
- Subscribes to all configured providers and sends each `weather.get_forecasts`
  request to the entity selected for that forecast type.
- Overview weather cards inherit the matching current, hourly, or daily source
  when they are added.

Configuration schema 10 adds `weather_hourly_entity_id` and
`weather_daily_entity_id`.

## Verification boundary

Host tests, rendered screen review, and both board builds verify source and
layout integration. Flashing, physical touch behavior, and responses from the
configured live Home Assistant weather integrations still require device-side
verification.
