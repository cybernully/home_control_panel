# Home Control Panel v1.9.6

## Expanded Dynamic Attention

- Raises the dynamic attention-only Security collection from eight to sixteen
  Home Assistant entities. The separate persistent monitored-device collection
  remains limited to eight cards.
- Sizes configuration storage, parser scratch space, state snapshots, Home
  Assistant subscriptions, and Web Admin validation from the same firmware
  limit so the browser cannot accept data that the panel would later truncate.
- Keeps normal dynamic devices invisible and preserves the compact four-item
  attention summary with an accurate overflow count for larger collections.
- Retains cross-list duplicate prevention, reverse-abnormal rules, availability
  handling, custom icons, labels, and severity colors.

## Ordered Room favorites

- Adds numbered Favorite positions to the selected Room editor in Web Admin.
- Provides touch-friendly Earlier and Later actions only for controls currently
  marked as favorites.
- Reorders favorites within the active panel room without moving controls in
  another room or separating a control from its complete configuration.
- Refreshes ordering controls immediately when a control's placement changes.

## Compatibility and verification boundary

Configuration schema 10 remains unchanged. Existing dynamic devices and Room
control order load without migration. Host parser, Web Admin, LVGL regression,
and dual-target firmware builds verify the integration. Flashing, touch behavior,
and live Home Assistant/Alarmo transitions still require panel-side validation.
