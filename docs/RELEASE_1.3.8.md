# Release 1.3.8 - Native-Resolution Artwork Layout

## Baseline

This incremental interface release applies to v1.3.7.

## Split now-playing layout

The Media dashboard returns artwork to a dedicated left-hand region rather
than using a square image as the background of a wide card.  The new 320x320
artwork well avoids the aggressive cover crop introduced in v1.3.7.  Player,
track, artist, album, source/state, transport controls, and command status use
a focused column to the right.

## Native-resolution artwork

Artwork is centered and aspect-ratio preserving.  The display scale is capped
at 256/256, so an image can be reduced to fit the 304x304 usable area but is
never enlarged beyond its decoded dimensions.  A typical 256x256 Home Assistant
image therefore displays at exactly 256x256 with intentional padding around it.
Rectangular images fit without stretching or cropping.

This release does not change the JPEG or PNG download/decode paths introduced
in the earlier artwork fixes.  It continues to use the same single
PSRAM-preferred RGB565 buffer and does not add a backdrop, blur, duplicate image,
or animation.

## Expected serial checkpoint

```text
[Media] Artwork shown: JPEG progressive full, ..., 256x256, scale=256/256
```

## Upgrade options

- Apply `home_control_panel_v1.3.7_to_v1.3.8.patch` to v1.3.7 with
  `git apply`.
- Replace the project with the complete `home_control_panel_v1.3.8` release
  tree, preserving the local untracked `include/app_secrets.h` file.
