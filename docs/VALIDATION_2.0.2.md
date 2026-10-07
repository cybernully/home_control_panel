# v2.0.2 validation

## OTA regression coverage

`scripts/test_firmware_update.py` compiles the actual `firmware_update.cpp`
against deterministic flash and clock adapters. Twelve cases cover a full
3,625,600-byte image, scheduling delays throughout streaming, declared-size
enforcement, invalid image magic, inactive-partition failure, write failure,
verification failure, cancellation, retry after failure, timeout across millis
wraparound, and legacy unknown-size compatibility. Activation happens only at
finish and is preserved if a later abort arrives.

The same runner compiles the production OTA handler functions extracted directly
from `web_manager.cpp` against an authenticated WebServer adapter. Ten cases
exercise authentication, session ownership, offsets, chunk/content length,
truncation, multiple file parts, disconnect after file end, explicit finish,
idempotent finish retry, and legacy activation after HTTP completion. The native
adapter does not reproduce the Arduino TCP/multipart parser or physical flash.

`tests/firmware_web_test.cjs` executes the real browser helpers in a mocked
network/DOM. Nine scenarios cover acknowledged progress, lost chunk replies,
partial chunks, server rejection, failed verification, lost activation replies,
reboot during reconciliation, a panel that responds without rebooting, and bad
image magic. Background polling pauses during maintenance, and original control
disabled states are restored after failure.

`tests/firmware_recovery_test.py` verifies a release-sized byte stream through the
actual Python recovery uploader, including exact multipart Content-Length,
payload preservation, bounded 1 KB writes, pacing, server rejection, and invalid
image rejection before any upload.

## Release verification

- Project structure validation, native Room/Overview/Climate/Security/UTF-8 and
  Home Assistant capability checks, Calendar state checks, and Web Admin layout
  checks passed.
- LVGL Room, header, Overview, Media, Weather, Calendar, Climate, and Security
  render/interaction suites passed. The eight-screen header fixture still checks
  identical persistent header pixels, caption geometry, shared icon mapping, and
  connected/syncing/offline states.
- Final 2624 build: 76,040 bytes RAM (23.2%); 3,555,198 bytes flash (56.5%).
- Final 2635 build: 213,308 bytes RAM (65.1%); 3,556,612 bytes flash (56.5%).
- Both images contain the 2.0.2 version string, fit the inactive 6 MB partition,
  and were packaged after the final source changes. The temporary short drive
  mapping was removed after each build.

| Target | Binary bytes | SHA-256 |
| --- | ---: | --- |
| 2624 | 3,632,704 | `161fece2907da6f8ff77c7bd628f0a147d7c4a426a907a6dec98e5f024ebb132` |
| 2635 | 3,634,416 | `0cef508bcfc6502edff0368f793ed12ccd851d1fd24362b12ec550cb2f60a53e` |

Versioned artifacts and checksums are in `releases/2.0.2/`. Standard build outputs
remain in `.pio/build/jc8012p4a1c_2624/firmware.bin` and
`.pio/build/jc8012p4a1c_2635/firmware.bin`.

The stack remains Espressif32 55.3.37, Arduino 3.3.7, LVGL 9.3.0, and ESP-Hosted
2.11.6. Existing third-party WebSockets/SPI warnings did not prevent builds.

The embedded Web Admin fixtures were extracted, including received-byte progress
and uncertain-activation states. Headless Edge returned without generating
screenshots; the runner now detects this instead of reporting success. Browser
policy also blocked local-file preview access. New Web Admin visual verification
is incomplete. The mocked browser behavior tests passed; they do not establish
pixel-level layout correctness.

Read-only inspection of the already-open live Web Admin showed v2.0.1 alongside
the old upload-failure notice. This verifies the current displayed version, not
the cause of the failed connection or when that version was activated.

## Device boundary

No firmware has been flashed during this change. Real ESP-Hosted Wi-Fi upload,
watchdog behavior, OTA boot selection, physical power-loss recovery, touch, and
live Home Assistant behavior remain unverified. The user's failure screenshot
establishes connection loss; the exact original device failure remains unknown.
The paced first-install helper has been tested against a transport adapter only.
