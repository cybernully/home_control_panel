# Release 1.3.0 - Home Assistant Media Bindings

## LVGL 9.3 artwork compatibility

The corrected release no longer stores or constructs LVGL's opaque
`lv_fs_path_ex_t` type.  JPEG and PNG artwork now use persistent RAW variable
image descriptors backed by PSRAM, which resolves both the original
incomplete-type build failure and the unreliable LVGL 9.3 MEMFS image-source
path.

Home Assistant artwork URLs now have 512-byte storage so signed media-proxy
URLs are not cut off at 223 characters.  Unauthenticated external artwork
requests follow HTTP redirects; authenticated Home Assistant requests do not,
so a bearer token cannot cross to another redirect host.  Queue, download,
format, cache, and display results are reported to the serial console without
printing the token-bearing URL.

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

LVGL 9.3's built-in LodePNG and TJpgDec decoders are enabled.  Both PNG and JPEG
use an in-memory image descriptor, so normal supported streams can be decoded
without writing image files to flash.

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
