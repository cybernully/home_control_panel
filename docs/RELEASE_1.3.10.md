# Release 1.3.10 - Settings IP Address

## Baseline

This incremental maintenance release applies to v1.3.9.

## Settings network information

The on-device Settings page now displays the panel's current Wi-Fi IP address
in the Panel identity card.  The value is read from the existing network
service during the normal Settings refresh, so a DHCP address change is shown
without rebuilding the page.  When Wi-Fi is disconnected, the field displays
`Offline` rather than retaining a stale address.

## Resource impact

This update adds one retained LVGL label pointer and reuses the existing
Settings refresh cycle and network service.  It creates no task, worker,
framebuffer, or network connection.

## Upgrade options

- Apply `home_control_panel_v1.3.9_to_v1.3.10.patch` to v1.3.9 with
  `git apply`.
- Replace the project with the complete `home_control_panel_v1.3.10` release
  tree, preserving the local untracked `include/app_secrets.h` file.
