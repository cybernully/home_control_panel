# Release 1.6.1 - Compact room context

Version 1.6.1 keeps room-specific state inside the Room screen and simplifies the persistent shell.

## Interface refinements

- Room selection is a native dropdown at the top of the Room screen; changing it rebinds the existing cards without recreating the screen.
- Temperature, humidity, online-device count, and room health share the same compact inline strip as the selected room.
- The former lower Room Status section and title are removed. Favorite and Quick Access cards move upward into a clearer two-section layout.
- The persistent header displays panel identity, date/time, Home Assistant status, Wi-Fi, battery, and Settings as transparent information zones separated by vertical dividers.
- Settings is no longer duplicated in bottom navigation. Its persistent top-right shortcut still opens the same Settings module.

## Compatibility and validation

The Home Assistant worker, state model, command queue, configured room sensors, web manager, OTA, storage, and display driver are unchanged. Host render/interaction tests and both ESP32-P4 firmware targets remain release gates; physical touch and live Home Assistant behavior still require device validation.
