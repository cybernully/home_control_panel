# Release 1.3.4 - Direct JPEG Artwork Decode

## Baseline

This incremental maintenance release applies to v1.3.3.  The underlying GitHub
baseline remains `ee7bcabea5ebe49100852e0d3fb31d09d34d203c`, followed by the
v1.3.2 and v1.3.3 incremental updates.

## Corrected artwork rendering

The v1.3.3 log confirmed that LVGL accepted the in-memory image descriptor but
failed later in its Tiny JPEG `decoder_open` operation.  Version 1.3.4 removes
that decoder from the JPEG display path.

JPEG artwork is now:

1. downloaded into the existing encoded PSRAM buffer;
2. decoded with JPEGDEC 1.6.1 into a separate PSRAM-backed RGB565 buffer;
3. supplied to LVGL as an uncompressed RGB565 variable image.

This avoids LVGL's streaming JPEG open operation entirely.  Baseline images
are reduced by 1/2, 1/4, or 1/8 when necessary to keep the decoded image at or
below 256 pixels on its longest side.  JPEGDEC supports progressive images by
decoding their first scan as a 1/8-size thumbnail.

PNG images continue to use LVGL's LodePNG decoder.

## Font warning cleanup

Live media titles and metadata can contain U+2013 en dash or U+2014 em dash.
Those glyphs are not present in the firmware's built-in Montserrat fonts.
Version 1.3.4 converts them to an ASCII hyphen before updating LVGL labels.

## Expected serial checkpoints

For a baseline JPEG:

```text
[HA] Media artwork cached: JPEG, ...
[Media] Artwork shown: JPEG, ...
```

For a progressive JPEG:

```text
[HA] Media artwork cached: JPEG, ...
[Media] Artwork shown: JPEG progressive thumbnail, ...
```

JPEGDEC failures now include an explicit numeric error code.

## Upgrade options

- Apply `home_control_panel_v1.3.3_to_v1.3.4.patch` to v1.3.3 with
  `git apply`.
- Replace the project with the complete `home_control_panel_v1.3.4` release
  tree, preserving the local untracked `include/app_secrets.h` file.
