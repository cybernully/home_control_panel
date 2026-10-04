# v2.0.1 validation

## Host checks

- Project structure validation passed.
- Native Room, Overview, Security, Climate, UTF-8, and Home Assistant
  capability regressions passed; Calendar state checks passed.
- Web Admin editor/configuration regressions passed.
- LVGL Room, Overview, Media, Weather, Calendar, Climate, and Security render
  and interaction fixtures passed. Their page headers were visually reviewed.
- The UI shell fixture passed caption font/baseline, icon-row, battery-bound,
  and connected/syncing/offline checks. Switching all eight modules produces
  identical persistent-header pixels, with the same page-heading geometry.
  The Settings heading is exercised through the shared helper in this fixture;
  its full diagnostics page is not rendered by the native harness.
- Saved previews: `previews/header-uniform-2.0.1.png`,
  `previews/media-uniform-2.0.1.png`, and `previews/weather-uniform-2.0.1.png`.

## Firmware builds

The `jc8012p4a1c_2624` build passed: 75,912 bytes RAM (23.2%) and
3,547,342 bytes flash (56.4% of the 6 MB application partition).
The `jc8012p4a1c_2635` build passed: 213,060 bytes RAM (65.0%) and
3,548,684 bytes flash (56.4% of the 6 MB application partition).
Both final binaries contain the v2.0.1 version string.

| Target | Binary bytes | SHA-256 |
| --- | ---: | --- |
| 2624 | 3,623,968 | `b821615a39be4492b31817c0835912abb732251c3318a449ab9866859a08ebb0` |
| 2635 | 3,625,600 | `d35297c59d055610c7fea07e4976e8eaf9cc1da23c98d417bf5bc3b4e4728539` |

Outputs are `.pio/build/jc8012p4a1c_2624/firmware.bin` and
`.pio/build/jc8012p4a1c_2635/firmware.bin`. Dependency warnings about deprecated
WebSockets flushing and Arduino SPI qualifiers do not prevent either build.

The pinned stack is PlatformIO Espressif32 55.3.37, Arduino 3.3.7, LVGL 9.3.0,
and the existing ESP-Hosted 2.11.6 firmware. Builds use a temporary short drive
path to the workspace to avoid Windows archive path-length failures. The
temporary mapping is absent after completion.

## Device boundary

Firmware has not been flashed. Physical touch, on-device rendering, and live
Home Assistant state/actions remain unverified. Configuration schema 10 and
the saved panel layout are unchanged.
