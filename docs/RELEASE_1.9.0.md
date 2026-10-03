# Home Control Panel v1.9.0

## Alarmo security

The Security tab is now a live Alarmo control and monitoring experience rather
than a static preview. Web Admin selects the `alarm_control_panel` entity and
the panel subscribes to it through the same bounded Home Assistant worker used
by the rest of the application.

### Panel experience

- Live Alarmo state for disarmed, arming, entry delay, Home, Away, Night,
  Vacation, custom bypass, unavailable, and triggered conditions.
- Configurable Home, Away, Night, and Vacation arming buttons.
- Optional arming confirmation and optional code-to-arm behavior.
- Mandatory on-screen numeric keypad for disarming. The code is never saved or
  included in status logs.
- Dynamic attention card showing the abnormal count and the first four devices
  needing attention.
- Up to eight compact monitored-device cards using the shared reusable card
  component and the established panel theme.
- Unavailable configured devices are intentionally treated as abnormal.

### Web Admin

Each monitored device has an entity, display name, icon, abnormal-state list,
normal and abnormal labels, attention color, ordering, and a reverse-abnormal
option. The reverse option supports devices such as connectivity sensors where
`on` means healthy and `off` means attention is required.

### Architecture and safety

- `security_state_model.cpp` owns Alarmo-state interpretation, abnormal-state
  evaluation, and UI-neutral commands.
- `security_module.cpp` owns LVGL presentation, confirmations, and the keypad;
  it does not parse JSON or make network calls.
- `home_assistant.cpp` owns subscription state and standard
  `alarm_control_panel` service calls.
- The Home Assistant entity-domain buffer is now large enough for the full
  `alarm_control_panel` domain, preventing silent truncation.
- Configuration schema 7 migrates existing installations with safe Security
  defaults and no stored PIN.

Host validation covers configuration rejection, direct and reversed abnormal
logic, missing-device behavior, Alarmo action gating, confirmation flow, keypad
entry, live-state rendering, and Web Admin payloads. Physical touch behavior,
Alarmo service acceptance, and live state transitions still require validation
after flashing a panel connected to the target Home Assistant instance.
