# Release 1.3.6 - Full-Resolution Progressive Artwork

## Baseline

This incremental maintenance release applies to v1.3.5.

## Corrected progressive JPEG quality

Versions 1.3.4 and 1.3.5 used JPEGDEC's supported first-scan decode for
progressive JPEG images.  For a 256x256 source, that produced a 32x32 thumbnail.
Version 1.3.5 correctly enlarged the thumbnail to the artwork viewport, but the
missing image detail made the result visibly blurry.

Version 1.3.6 keeps JPEGDEC for baseline JPEGs and adds stb_image v2.30 for
full progressive JPEG decoding.  The implementation:

- decodes all progressive scans at the original image resolution;
- allocates stb_image working memory from PSRAM when possible;
- converts the decoded RGB888 image to the existing RGB565 artwork buffer;
- limits displayed artwork to 256 pixels on its longest side;
- enables LVGL image antialiasing when fitting artwork to the 202x202 viewport.

The vendored `stb_image.h` is pinned to upstream commit
`2c980bb59875b0d32144a71867fbdebb2f77cd20` and compiled with JPEG-only and
no-filesystem options.

## Expected serial checkpoints

```text
[HA] Media artwork cached: JPEG, ..., 256x256
[Media] Progressive JPEG decoded full resolution: 256x256 -> 256x256
[Media] Artwork shown: JPEG progressive full, ..., 256x256, scale=202/256
```

## Upgrade options

- Apply `home_control_panel_v1.3.5_to_v1.3.6.patch` to v1.3.5 with
  `git apply`.
- Replace the project with the complete `home_control_panel_v1.3.6` release
  tree, preserving the local untracked `include/app_secrets.h` file.
