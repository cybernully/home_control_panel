# Release 1.6.4 - Configurable Overview cards

Version 1.6.4 rebuilds Overview as a configurable live dashboard using the same
reusable card component and state-model separation as the Room screen.

## One card model

- Built-in home, light, network, weather, calendar, all-lights, and helper cards
  share one ordered four-column layout with Home Assistant entity cards.
- Entity cards can be status-only or run a toggle, cover, lock, or scene action.
- Labels, width, icon family, active-state list, active and inactive text, active
  color, action, and confirmation behavior are configurable per card.
- An entity card can use one Home Assistant entity as its live status source and
  a different light, switch, fan, cover, lock, or scene as its tap-action target.
  This allows a sensor card, such as a garage-door or motion sensor, to control
  its related relay or light without changing the state shown on the card.
- Inactive status icons are deliberately muted; matching active states use the
  selected color and active glyph.
- Existing Overview widgets and quick actions migrate into the new model when a
  stored 1.6.3 or earlier configuration is loaded.
- Common status domains appear in authenticated discovery, and a manual entity-ID
  entry supports status-only cards for other Home Assistant domains without
  expanding the panel's whole-installation discovery payload.
- Reordering and removal now snapshot edited fields before changing the ordered
  model, then render in one direction from that model. This keeps every card's
  entity, label, width, icon, states, colors, actions, and confirmation setting
  together through both button and drag reordering.

## Specialized devices without specialized screens

Garage doors are the reference configuration: a cover card can use closed and
open garage glyphs, color itself for `open,opening`, show Closed/Open state text,
and require confirmation before sending the current-state-aware open/close action.
Locks, doors, windows, motion sensors, cameras, lights, fans, switches, weather,
and sensor cards use the same model, so new device presentations do not require
duplicating an LVGL component.

## Architecture and safety

`ui_state_model` owns the conversion from Home Assistant state and saved
configuration to UI-neutral card snapshots. Overview widgets only render those
snapshots and send user intents back through the model. Confirmation dialogs keep
a stable target while visible, and actions revalidate live availability before
using the bounded Home Assistant command queue. Home Assistant discovery now
includes binary sensors and locks while retaining the compact authenticated
`/api/template` search.

Host parser, web-editor, rendered LVGL interaction, project-structure, and both
ESP32-P4 firmware builds are release gates. Physical touch, live Home Assistant
service behavior, and long-duration hardware stability still require device tests.
