# Hardware targets

Home Control Panel 1.0.0 supports two known JC8012P4A1C builds from one source
tree.

## 2624

- Rear label/batch: `10153001 (2624)`
- PlatformIO environment: `jc8012p4a1c_2624`
- PlatformIO board: `esp32-p4`
- Display profile: `JC8012P4A1-V2`
- PCLK: 70 MHz
- DSI lane bitrate: 1500 Mbps
- vertical timing: 4 / 10 / 20

This preserves the hardware/display path already proven by the Family Calendar.

## 2635 / V3

- Rear label/batch: `10153001-V3 (2635)`
- PlatformIO environment: `jc8012p4a1c_2635`
- PlatformIO board: `esp32-p4_r3`
- Tested unit: ESP32-P4 revision v3.2
- Display profile: `JC8012P4A1-V2`
- PCLK: 70 MHz
- DSI lane bitrate: 1500 Mbps
- vertical timing: 4 / 10 / 20

The application/display code is shared.  The production-silicon PlatformIO
board target is required for the 2635 so the boot/runtime components are built
for the correct P4 generation.

## DSI PHY clock selection

The pinned display driver is patched at build time to inspect the actual P4
silicon revision.  Pre-v3 silicon uses the legacy `PLL_F20M` PHY reference;
v3.x production silicon uses `XTAL`.

## Toolchain

Both environments use pioarduino `55.03.39`.  Upload speed defaults to 115200
for reliability; this can be overridden locally later if desired.
