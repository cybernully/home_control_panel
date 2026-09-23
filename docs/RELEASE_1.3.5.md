# Release 1.3.5 - Artwork Viewport Scaling

## Baseline

This incremental maintenance release applies to v1.3.4.

## Corrected artwork sizing

Version 1.3.4 successfully displays artwork.  Progressive JPEG input is
decoded by JPEGDEC as a 1/8-size first-scan thumbnail, which is commonly 32x32
for a 256x256 source image.  The Media UI's previous 100% scale ceiling left
that thumbnail at 32x32 inside the 202x202 artwork viewport.

Version 1.3.5 allows LVGL scale values above 256 so small decoded artwork is
enlarged to the viewport.  The existing aspect-ratio calculation remains in
place, so rectangular images fit within 202x202 without distortion or cropping.
Scaling is capped at 4096 (16x) to prevent pathological dimensions from
creating an excessive transformed draw area.

For a 32x32 progressive thumbnail, the calculated scale is 1616/256, producing
approximately 202x202 displayed artwork.

## Expected serial checkpoint

```text
[Media] Artwork shown: JPEG progressive thumbnail, ..., 32x32, scale=1616/256
```

## Upgrade options

- Apply `home_control_panel_v1.3.4_to_v1.3.5.patch` to v1.3.4 with
  `git apply`.
- Replace the project with the complete `home_control_panel_v1.3.5` release
  tree, preserving the local untracked `include/app_secrets.h` file.
