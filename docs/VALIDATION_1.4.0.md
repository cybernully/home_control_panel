# 1.4.0 validation

## Passed

- `scripts/validate_project.py`: version, project structure, retained media and
  single-worker architecture checks.
- `tests/room_web_test.cjs`: discovery merge, missing favorite preservation,
  HTML escaping, UTF-8 names, reorder, hide, reset, and serialized save payload.
- `scripts/test_room_native.py`: compiles the real room preference parser with
  ArduinoJson 7.4.3. Covers empty/valid input, supported domains, duplicate and
  invalid IDs, type checks, six-favorite and 48-preference bounds, byte-length
  limits, and no mutation on rejection. Shared display text tests cover en/em
  dashes, accented UTF-8, truncated sequences, null text, and small buffers.
- `scripts/test_room_render.py`: compiles the real Room module against LVGL 9.3.0
  with a deterministic, offline Home Assistant fixture. Checks popup paging,
  correct action targets, hidden controls, disabling empty groups, preserving
  slider values during refresh, no accidental toggle from sliders, canceled
  drags, and popup dismissal on navigation.
- Visual inspection of four actual LVGL content-area renders at 1280x658:
  favorites, first/second Lights popup pages, and unconfigured favorites. No
  overlapping text or controls observed. Off-state slider track contrast was
  improved after inspection. These are sample-data renders, not device photos.

## Previews

![Favorites](previews/room-favorites.png)
![Lights popup](previews/room-lights.png)
![Second popup page](previews/room-lights-page2.png)
![Empty favorites](previews/room-empty.png)

## Firmware compilation

Both pinned PlatformIO firmware environments built successfully:

| Board | Static RAM | Application flash | Result |
|---|---:|---:|---|
| 2624 / ESP32-P4 | 90,848 / 327,680 bytes (27.7%) | 3,284,604 / 6,291,456 bytes (52.2%) | PASS |
| 2635 / ESP32-P4 R3 | 126,556 / 327,680 bytes (38.6%) | 3,285,988 / 6,291,456 bytes (52.2%) | PASS |

Firmware outputs are `.pio/build/jc8012p4a1c_2624/firmware.bin` and
`.pio/build/jc8012p4a1c_2635/firmware.bin`. These target different silicon revisions;
use the build matching the panel's rear label. Upstream ESP-IDF headers emitted
non-fatal literal-suffix warnings. No application compile errors remain.

## Hardware validation remaining

No firmware upload, physical touch test, or live Home Assistant command was
performed. After a firmware-only update, check the panel's actual area discovery,
web-save/restart persistence, live light/cover/scene actions, and disconnect/
reconnect behavior. Keep the existing SPIFFS configuration when upgrading.

The clean Windows build required a temporary short drive alias to avoid the
host's disabled long-path support. The existing framework include flags now use
quotes so paths containing backslashes or spaces reach the compiler intact.
The host's global long-path policy was not changed.

Firmware 2624 SHA-256: `18c71cb8b430e5eeecf0aee395c91252ee13847c9e725f7b60d0ef913f115751`

Firmware 2635 SHA-256: `fa2b0553d70ef693203f60878b9e7662571fc3b94bec17522b1f5db2c03e8073`
