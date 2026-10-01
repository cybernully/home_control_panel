# Release 1.7.0 - Media workspace

Version 1.7.0 rebuilds the Media tab around the visual system established by
the Room and Overview tabs while retaining the proven playback and artwork
pipeline.

## Panel experience

- A single now-playing card keeps large album art, bounded metadata, transport,
  source state, volume, mute, and the selected player together.
- Transport and volume controls are icon-led, touch-friendly, and expose short
  captions where a symbol alone could be ambiguous.
- Up to six quick-play cards fit in one consistent row and use configurable
  Music, Radio, Podcast, Playlist, Favorite, or Speaker icons.
- Players, sources, and favorites open from compact bubble controls. Each popup
  is persistent while visible, preventing the once-per-second refresh flash.
- The player popup supports six configured devices; the ordered first player is
  the default. The favorites popup shows six configured/discovered actions.

## Web configuration

Web Admin now edits three ordered media collections: players, quick-play cards,
and popup favorites. Entries are read into JavaScript models before a move, and
the complete object is moved and rendered again. Labels, icons, target players,
content IDs, and content types therefore cannot separate during reordering.

The save API accepts bounded JSON arrays for shortcuts and favorites while
retaining the legacy indexed-field parser for older clients. Existing saved
actions default to a suitable icon when they do not yet contain one.

## State and artwork safeguards

`MediaPlayerViewModel` is the presentation boundary between Home Assistant
snapshots and LVGL. It owns display text and capabilities; the widget layer does
not parse JSON, WebSocket payloads, or Home Assistant attributes.

Artwork remains PSRAM-backed and generation-checked. A change in title or
artist now forces an artwork refresh even when an integration reuses the same
proxy URL. Player changes clear stale covers immediately, while same-player
track changes keep the previous cover until the replacement is decoded. Failed
requests use the existing bounded retry interval.

Host render/interaction tests cover six players, six shortcuts, both popup
menus, bound volume gestures, unavailable/empty states, and same-URL track
refreshes. Both ESP32-P4 firmware variants remain release gates; physical touch
and live Home Assistant behavior require validation after flashing.
