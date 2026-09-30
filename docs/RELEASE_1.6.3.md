# Release 1.6.3 - Room controls and communication clarity

Version 1.6.3 sharpens the Room screen and separates room health from Home Assistant communication state.

## Room controls

- Favorite cards are shorter and reserve their top row for the icon, ellipsized name, and state text.
- The active control is placed along the bottom edge so long names cannot overlap toggles, sliders, actions, or fan speeds.
- Percentage-capable fans use touch-friendly **Off**, **Low**, **Med**, and **High** buttons instead of a slider in favorites and grouped-control sheets.
- The shared card component now includes a FAN variant and swaps variants only when the bound control type changes.
- Room System Status summarizes availability for the controls assigned to the selected room; it no longer mirrors global WebSocket state.

## Icons and persistent header

- Temperature uses a thermometer rather than the previous light-bulb glyph.
- Humidity uses a water-percent glyph and Weather uses a partly-cloudy glyph rather than the shade glyph.
- A dedicated communication icon sits between System Status and Wi-Fi. It reports **Connected**, **Syncing**, or **Offline** and opens the existing detailed status popup.
- The embedded MDI font remains a bounded subset containing only the glyphs required by the panel.

## Compatibility and validation

Home Assistant discovery, fan percentage state, the bounded command queue, storage, OTA, and the display driver retain their existing contracts. Host render/interaction tests and both ESP32-P4 firmware targets remain release gates. Physical touch, live fan service behavior, Home Assistant communication transitions, and long-duration stability still require on-device validation.
