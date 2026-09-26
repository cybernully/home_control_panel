# Home Control Panel 1.5.0

## Web layout manager

- Replaces the single configuration page with local tabs matching the panel:
  Overview, Room, Media, enabled panel tabs, and Administration.
- Adds an authenticated, on-demand Home Assistant area scan. The editor shows
  the entity name, ID, domain, availability, and applicable capabilities so
  the correct panel control can be selected without manual entity-ID entry.
- Room controls can be added, removed, named, ordered, and placed as grouped,
  favorite, or hidden. Media players and one-touch media actions are managed
  separately on the Media tab.

## Stable, explicit discovery

- Saving a 1.5.0 layout migrates the panel to an explicit SPIFFS-backed
  entity set. Startup and normal reconnects subscribe only to the selected
  room controls, media players, and shortcut targets instead of every
  supported entity in the Home Assistant area.
- Existing schema-1 installations keep legacy automatic discovery until their
  owner saves a new layout, preventing an upgrade from blanking the panel.
- The full-area scan is temporary and used only by the authenticated editor;
  saving returns the live subscription to the explicit layout.

## Operational notes

- The UI is self-contained in firmware rather than depending on a CDN, so it
  remains usable during an Internet outage. The token stays in NVS and is
  never sent back to the browser.
- Panel tab order and visibility are edited in Administration. Reboot after
  a tab-order change; control changes refresh the Home Assistant layout live.

## Overview widgets

- The Overview tab in the web manager configures the panel start screen as a
  four-column widget grid. Add, remove, and reorder built-in Home status,
  lights, area, network, quick actions, weather, calendar, and panel-tip
  widgets.
- Every widget can be Small (one column), Wide (two columns), or Full width
  (four columns). Weather and Calendar use the first matching Home Assistant
  entity in the selected area and are included in the explicit subscription
  when their widget is present.
- Quick actions is now a configurable sub-panel: add up to six named All
  Lights, entity-toggle, or scene-activation buttons. Set the target entity
  from the Home Assistant scan and choose a two-to-four-row widget height.
  Reboot after saving an Overview layout change so LVGL can rebuild its widget
  grid and button geometry.
