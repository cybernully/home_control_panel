# Release 1.3.7 - Media Dashboard Redesign

## Baseline

This incremental interface release applies to v1.3.6.

## Redesigned now-playing card

The 760x394 now-playing card uses the decoded artwork as its background.  LVGL's
`LV_IMAGE_ALIGN_COVER` mode preserves the source aspect ratio and crops the
overflow instead of stretching it.  A single 50% dark scrim keeps titles,
metadata, playback controls, and status text readable across both bright and
dark cover art.

The visual hierarchy now gives the track title the most prominence, places
artist, album, and player state beneath it, and centers larger previous,
play/pause, and next controls across the card.

## Volume controls

The volume card retains direct slider control and adds 64-pixel touch targets
for decrement and increment.  These buttons queue Home Assistant's native
`media_player.volume_down` and `media_player.volume_up` services.  The panel
does not guess a step size or update the displayed value optimistically; it
waits for the subscribed Home Assistant state update.  Mute is reduced to a
compact secondary button while preserving its existing mute/unmute state.

## ESP32-P4 resource budget

- The existing PSRAM-backed RGB565 artwork buffer is reused as the background.
- No duplicate decoded artwork or additional full-screen buffer is allocated.
- The readability layer is one flat LVGL object rather than a computed blur or
  gradient effect.
- Artwork is redrawn when its source changes; no animation is added.
- Existing 1280x800 card boundaries and lazy module creation remain unchanged.

## Expected serial checkpoint

```text
[Media] Artwork shown: JPEG progressive full, ..., 256x256, mode=cover
```

## Upgrade options

- Apply `home_control_panel_v1.3.6_to_v1.3.7.patch` to v1.3.6 with
  `git apply`.
- Replace the project with the complete `home_control_panel_v1.3.7` release
  tree, preserving the local untracked `include/app_secrets.h` file.
