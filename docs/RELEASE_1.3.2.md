# Release 1.3.2 - Media Artwork Stack Protection

## Baseline

This incremental maintenance release is based on GitHub `main` commit
`ee7bcabea5ebe49100852e0d3fb31d09d34d203c`, identified as v1.3.1.

## Corrected crash

The v1.3.1 media artwork path successfully downloaded and cached JPEG artwork,
but the synchronous LVGL decoder call exceeded Arduino's approximately 8 KB
`loopTask` stack.  The captured failure placed the stack pointer 948 bytes below
the task's lower stack boundary.

Version 1.3.2 makes two related corrections:

- configures Arduino's `loopTask` with a 16 KB stack before task creation;
- moves the two `HomeAssistantMediaArtworkInfo` work structures into the
  Media module's PSRAM-preferred allocation instead of creating them on the
  loop stack.

The existing PSRAM-backed player, favorite, encoded-artwork, and Home Assistant
state buffers remain unchanged.

## Expected serial checkpoints

At boot:

```text
Home Control Panel v1.3.2 starting...
[Runtime] loopTask stack: 16384 bytes
```

After opening Media while artwork is available:

```text
[HA] Media artwork cached: JPEG, ...
[Media] Artwork shown: JPEG, ...
```

## Upgrade options

- Apply `home_control_panel_v1.3.1_to_v1.3.2.patch` to the v1.3.1 GitHub
  baseline with `git apply`.
- Replace the project with the complete `home_control_panel_v1.3.2` release
  tree, preserving the local untracked `include/app_secrets.h` file.
