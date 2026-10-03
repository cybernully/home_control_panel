# Home Control Panel v1.9.1

## Dynamic attention devices

Security configuration now has two independent ordered collections:

- Up to eight monitored devices with persistent status cards.
- Up to eight dynamic attention devices that stay hidden while normal and
  appear only in the Dynamic Attention summary when abnormal or unavailable.

Both collections use the same entity, display name, icon, rule-state list,
normal/abnormal labels, attention color, ordering, and reverse-rule behavior.
The same entity cannot be added to both collections.

The state model evaluates both collections without coupling Home Assistant
transport to LVGL. Persistent cards still read only the monitored collection;
the attention summary combines abnormalities from both and shows a compact
overflow count when more than four devices need attention.

## Validation and migration

Configuration schema 8 adds `security_dynamic_devices`. Existing schema 7
installations migrate with an empty dynamic list, preserving all Alarmo and
monitored-device settings. The Web Admin default abnormal-state rule is now
domain-aware and always fits the persisted field. Backend validation reports
the specific invalid field and item rather than one generic Security error.

Host coverage includes parser atomicity, cross-list duplicate detection,
normal and abnormal dynamic states, attention-only rendering, Web Admin payload
serialization, and the existing Alarmo confirmation/keypad flows. Flashing,
touch behavior, and live Home Assistant/Alarmo state transitions still require
validation on the physical panel.
