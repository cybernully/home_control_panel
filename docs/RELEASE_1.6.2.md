# Release 1.6.2 - Shell alignment refinement

Version 1.6.2 simplifies the persistent shell while preserving the Room screen and Home Assistant behavior introduced in 1.6.1.

## Interface refinements

- Panel title and date/time remain left aligned in the persistent header.
- Home Assistant status, Wi-Fi, battery, and Settings are arranged as equal right-aligned icon columns.
- Each header caption is centered beneath its icon for faster visual scanning.
- Subtle vertical dividers retain separation without turning the header sections into buttons.
- The bottom navigation no longer has an enclosing rounded box or outlined inactive tabs. Only the active tab receives a filled highlight.

## Compatibility and validation

Room selection, status cards, module navigation, the Home Assistant worker, command queue, web manager, storage, OTA, and display driver are unchanged. Host render and interaction checks plus both ESP32-P4 firmware builds remain the release gates. Physical touch, live Home Assistant behavior, and long-duration stability still require on-device validation.
