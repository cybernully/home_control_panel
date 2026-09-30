# Release 1.5.3 - Refined Room Control

- Retires the separate **All Rooms** placeholder. Existing `rooms` module
  entries migrate to the configurable **Room** module during configuration
  load, retaining saved controls and room sub-tabs.
- Adds a persistent local date/time display and a tappable Home Assistant
  status badge to the global header. The badge distinguishes ready, working,
  and failed states; tapping it shows the latest status and when it occurred.
- Reworks the Room screen into a denser touch-friendly layout: compact
  favorites, polished category cards with icons and counts, and room tabs
  remain easy to target.
