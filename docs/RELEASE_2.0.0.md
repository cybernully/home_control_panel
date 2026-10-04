# Home Control Panel v2.0.0

## Configuration backup and restore

- Adds an authenticated JSON configuration download to Web Admin.
- Adds a 64 KB bounded restore workflow with browser preflight checks and
  firmware-side schema, field, collection, capacity, duplicate, and room
  cross-reference validation.
- Validates into a separate `PanelConfig` and changes the active configuration
  only after the complete backup passes.
- Makes all configuration saves atomic using `/panel.tmp`, `/panel.json`, and
  `/panel.bak`. Boot recovery restores the last complete backup if a save loses
  power between file renames.
- Preserves the Home Assistant URL and access token already stored in NVS. They
  are deliberately not exported in configuration backups.

## OTA firmware updates

- Adds authenticated `firmware.bin` uploads from the Administration page.
- Limits images to the 6 MB application-partition size and rejects empty or
  non-binary uploads.
- Writes with the Arduino ESP32 Update API to the inactive OTA slot, verifies
  the completed image, and reboots only after validation succeeds.
- Shows browser upload progress, explicit 2624/2635 build guidance, errors, and
  automatic Web Admin reconnection after a successful reboot.

## Web Admin cleanup

- Removes the placeholder Settings page from Web Admin navigation and the DOM.
- Keeps Administration as the single location for configuration, maintenance,
  and service actions.
- Does not remove the functional Settings tab from the physical panel or its
  module-order configuration.

## Verification boundary

Host validation covers embedded JavaScript syntax/contracts, configuration
parsers, atomic-file integration structure, the full LVGL regression suite,
headless browser rendering, and both firmware targets. Physical OTA activation,
power-loss recovery, touch interaction, and live restored Home Assistant state
remain device-side release checks.
