# Room regression tests

Run `node tests/room_web_test.cjs` for the web editor's discovery/edit/save
roundtrip using a minimal DOM and mocked API (no live device access).

`python scripts/test_room_native.py` compiles the actual `src/room_config.cpp`
with a host-only Arduino String alias and the pinned ArduinoJson 7.4.3 single
header at `.test-deps/ArduinoJson.h`. It tests invalid and valid preferences,
capacity limits, rejection without mutation, and UTF-8 display normalization.
Set `CXX` to a C++17 compiler, or install `ziglang==0.13.0` in `.build-venv` on
Windows. Test shims are outside the firmware source and never enter its build.

`python scripts/test_room_render.py` builds the actual Room module with the
pinned LVGL 9.3.0 source extracted into `.test-deps/lvgl-9.3.0`. It uses the local
Zig compiler, deterministic HA fixtures, and a headless LVGL display. Screenshots
are PPM files in `.test-build`; the interactions never contact Home Assistant.
The first Zig build compiles its native runtime and can take several minutes.

Full firmware validation remains `pio run -e jc8012p4a1c_2624 -e
jc8012p4a1c_2635`. Do not flash the SPIFFS image on an existing panel just to
upgrade firmware: that would replace its saved panel settings.

Run the render script with `--header` to exercise the actual UI shell alongside
the Room module and produce full-screen header previews. The fixture checks
battery center alignment, fill bounds and clearing stale/empty readings.

Run with `--media` to render the real Media module and UI shell with an offline
four-player fixture. Checks cover playback/shortcut/source/browse targeting,
preserving a volume drag during refresh, canceled drags, popup dismissal, long
metadata, unavailable players, and empty discovery. This uses the installed
JPEGDEC dependency at `.pio/libdeps/jc8012p4a1c_2635/JPEGDEC/src`, the real stb
decoder, and the synthetic progressive JPEG in `tests/fixtures`. Native media
builds disable UBSan for embedded codec integer operations. The baseline JPEG
path failed in the Windows native harness; this fixture validates the unchanged
progressive path, not baseline decoding on hardware.
