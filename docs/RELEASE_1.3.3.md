# Release 1.3.3 - In-Memory JPEG Rendering

## Baseline

This incremental maintenance release applies to v1.3.2, which was built from
GitHub `main` commit `ee7bcabea5ebe49100852e0d3fb31d09d34d203c` plus the
v1.3.2 stack-protection patch.

## Corrected artwork rendering

Version 1.3.2 successfully downloaded and cached media artwork without the
earlier stack crash, but LVGL did not render JPEG data supplied through the
in-memory `lv_image_dsc_t`.  LVGL 9.3's Tiny JPEG decoder explicitly requires
the memory-filesystem adapter for variable JPEG sources.

Version 1.3.3:

- enables `LV_USE_FS_MEMFS` for LVGL;
- assigns the adapter the reserved `M` drive letter (numeric value 77);
- checks `lv_image_decoder_get_info()` before hiding the artwork placeholder;
- reports a decoder rejection in both the serial log and Media status text;
- invalidates the image widget after making decoded artwork visible.

PNG variable images continue to use LVGL's LodePNG decoder and are unaffected
by the memory-filesystem requirement.

## Expected serial checkpoints

After opening Media while JPEG artwork is available:

```text
[HA] Media artwork cached: JPEG, ...
[Media] Artwork shown: JPEG, ...
```

If LVGL rejects an image, the new diagnostic is:

```text
[Media] Artwork decoder rejected JPEG variable image
```

## Upgrade options

- Apply `home_control_panel_v1.3.2_to_v1.3.3.patch` to v1.3.2 with
  `git apply`.
- Replace the project with the complete `home_control_panel_v1.3.3` release
  tree, preserving the local untracked `include/app_secrets.h` file.
