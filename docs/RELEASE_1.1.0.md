# Release 1.1.0 - Visible Control UI

Version 1.1.0 turns the room profile from placeholder text into an interactive
control-panel UI while preserving the dual-board hardware architecture proven in
1.0.0.

## New UI modules

- **Overview** - status cards and quick-action buttons.
- **Room** - light/fan/shade toggle cards, brightness slider, scene buttons.
- **Media** - transport controls, volume slider and source buttons.
- **Climate** - current temperature, setpoint +/- and HVAC mode buttons.
- **Security** - alarm mode buttons and sensor-status cards.
- **Settings** - live display-brightness control plus panel and HA status.

## Functional scope

The controls are touch-active locally so layout, navigation and interaction can
be validated on the wall panel.  The Settings brightness slider is connected to
real panel hardware and persists its value.

Room, media, climate, security and overview quick actions are intentionally not
yet bound to Home Assistant entities.  They are UI controls, not fake HA state.
The next milestone is area/entity discovery plus service calls through the
existing single Home Assistant worker.

## Hardware

No display/touch bring-up changes were made from the working 1.0.0 dual-board
baseline:

- `jc8012p4a1c_2624` -> `esp32-p4`
- `jc8012p4a1c_2635` -> `esp32-p4_r3`

The default VS Code / PlatformIO environment is set to `jc8012p4a1c_2635` for
the current development panel.  Select the 2624 environment explicitly before
uploading to an older board.
