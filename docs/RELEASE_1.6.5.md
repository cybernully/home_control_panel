# Release 1.6.5 - Reliable fan Off and countdown timers

Version 1.6.5 fixes the Room fan Off action and adds reusable Home Assistant
timer cards to every panel's configurable Overview screen.

## Fan control

- Off now calls `fan.turn_off` with only the fan entity ID.
- Low, Medium, and High continue to call `fan.set_percentage` with their
  configured percentage.
- The Room interaction test now exercises High followed by Off so the zero-speed
  intent cannot silently disappear from the UI command path.

## Countdown timer cards

- Home Assistant `timer` entities are included in authenticated entity search
  and have a dedicated Timer filter and icon in the web editor.
- Timer cards are status-only Overview cards and can be added to any panel
  configuration, including `timer.office_energy_saver_countdown`.
- Active timers count down locally from Home Assistant's `finishes_at` timestamp,
  so the display advances smoothly without requiring a state event every second.
- Paused timers display the fixed `remaining` value with a Paused label. Idle
  timers display Idle.
- Timer state is translated into the UI-neutral Overview card model; the LVGL
  card does not parse Home Assistant JSON or calculate protocol state.

## Release verification

The release gates cover the configuration parser, embedded web editor and
JavaScript syntax, rendered Room and Overview interactions, project structure,
and both ESP32-P4 firmware variants. Physical touch behavior, the live Home
Assistant service call, and long-duration on-device countdown accuracy remain
hardware integration checks after flashing.
