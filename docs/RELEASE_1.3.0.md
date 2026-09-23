# Release 1.3.0 - Home Assistant Media Bindings

## LVGL 9.3 build compatibility

The corrected release keeps `lv_fs_path_ex_t` opaque in `media_module.h` and
uses LVGL's private type definition only inside `media_module.cpp`.  JPEG MEMFS
paths use the four-argument LVGL 9.3 helper and append the `.jpg` decoder hint
within the path object's bounded buffer.  This resolves the incomplete-type
failure in `module_registry.cpp` and `media_module.cpp` without changing the
configured LVGL version.

## Media-page stack protection fix

The Media page no longer creates its media-player and browse-favorite snapshot
arrays on Arduino's `loopTask` stack.  Those persistent work buffers now prefer
PSRAM and fall back to the heap only when needed.  The mute callback reuses the
same buffer.  This prevents the stack-protection panic that occurred when the
lazily created Media page performed its first update.

## Purpose

Version 1.3.0 replaces the Media module's local preview state with live Home
Assistant media-player discovery, state, service calls, browse-media shortcuts,
and artwork.

## Discovery and state

`media_player` is now part of the existing area entity model. The panel continues
to resolve the configured Home Assistant area with `extract_from_target`, then
subscribes only to the supported entities in that area with `subscribe_entities`.
Media attributes are stored in the same PSRAM-backed model used by Room controls.
The area cache limit increases from 24 to 48 supported entities to leave room for
media players without making large rooms drop them simply because lights/switches
were returned first.

The Media page can select among up to four area players and displays live:

- player/friendly name and state;
- title, artist, album and playlist;
- current source and up to four values from `source_list`;
- volume level and mute state.

## Commands

All commands are queued to the existing Home Assistant worker and sent through
Home Assistant services:

- `media_play_pause`
- `media_previous_track` / `media_next_track`
- `volume_set` / `volume_mute`
- `select_source`
- `play_media` for browse-media shortcuts

The UI does not assume a command succeeded. It waits for Home Assistant to send
the resulting subscribed state update.

## Favorites / playlists

When a player is selected, the worker requests `media_player/browse_media`. The
panel displays up to three directly playable children. If the root has no
playable children, it follows one expandable Favorites/Playlists-style container
(or the first expandable container as a fallback) and displays playable children
from that level. Integrations that do not implement browse media simply show no
browse items; normal transport/source controls continue to work.

## Artwork

The worker fetches the selected player's `entity_picture` with a 256 KB encoded
size limit. Home Assistant-relative image URLs use the configured HA credentials;
credentials are not sent to unrelated external image hosts. Completed artwork is
kept in PSRAM and copied to a UI-owned PSRAM buffer before LVGL uses it.

LVGL 9.3's built-in LodePNG and TJpgDec decoders are enabled. PNG uses an in-memory
image descriptor; JPEG uses LVGL's MEMFS path so normal JFIF and Exif JPEG streams
can be decoded without writing image files to flash.

## Architecture preserved

- UI first, network second.
- One Home Assistant worker.
- REST actions retain the one-second inter-request spacing.
- WebSocket and artwork HTTP work stay on that worker.
- Worker/network code never calls LVGL.
- LVGL stays on the UI/main thread.
- The 2624 and 2635 board environments are unchanged.

## Not included

Climate/security bindings, whole-home navigation, and OTA remain on later roadmap
releases. Family Calendar remains a separate project.
