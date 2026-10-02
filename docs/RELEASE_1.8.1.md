# Home Control Panel v1.8.1

## Configurable calendar range

- Web Admin can display one day, a rolling three-day period, or a full seven-day week.
- Previous and next advance by the configured period; Today returns to the current period.
- The seven-day view continues to honor the Monday or Sunday week-start preference.
- Home Assistant requests are limited to the visible period, reducing unnecessary calendar payloads in the one-day and three-day views.

## Expanded event details

- The event sheet is wider and taller, with dedicated calendar/date metadata, location, and details sections.
- Event descriptions now retain up to 511 bytes instead of 159 bytes.
- The details region scrolls independently, so longer notes remain accessible without crowding the agenda.
- Missing location and description fields have clear fallback text.

## Compatibility and validation

- Configuration schema 6 migrates earlier installations to the seven-day view by default.
- The calendar state model remains independent from LVGL and Home Assistant transport code.
- Host state and render tests cover the one-day, three-day, and seven-day range behavior. Both ESP32-P4 panel targets remain release build checks; live Home Assistant data and physical touch behavior require on-device verification.
