# Home Control Panel v1.9.5

## Reliable Home Assistant entity search

- Moves friendly-name, entity-ID, and domain filtering into Home Assistant's
  template engine before the bounded result is returned to the panel.
- Keeps the bearer token on the ESP and returns only compact entity metadata to
  the authenticated Web Admin browser.
- Makes exact IDs such as `light.office_office_ceiling_fan_light` discoverable
  even when an installation contains more supported entities than the panel's
  160-entry picker cache.
- Preserves previously found browser results while additional targeted searches
  run, making it practical to configure several unrelated devices in one pass.
- Carries brightness and cover-position capability hints in search results.
- Adds a validated exact-ID fallback to Rooms for lights, switches, fans,
  covers, and scenes when discovery is incomplete or temporarily unavailable.

## Web Admin redesign

- Introduces one professional responsive visual system for page cards, forms,
  navigation, notices, buttons, status, and editable item collections.
- Keeps Sync devices, Save changes, and Reboot together in the sticky header.
- Uses compact multi-column entity results and selected Room-control layouts on
  wider screens, with a single-column touch-friendly mobile layout.
- Removes repeated per-page sync buttons from the visual hierarchy while
  retaining the same underlying configuration workflows.
- Gives Rooms a focused sequence: choose a room, search or enter a device, then
  configure the controls already assigned to that room.
- Adds debounced server searches to Room and Overview filters while preserving
  explicit Search and Sync actions.

## Compatibility and verification boundary

Configuration schema 10 is unchanged. Existing rooms, cards, device rules,
media, weather, calendar, climate, Security, and panel settings remain intact.
Host parser, Web Admin, LVGL regression, and dual-target firmware builds verify
the integration. Physical flashing, touch behavior, and live search results from
the configured Home Assistant instance still require panel-side validation.
