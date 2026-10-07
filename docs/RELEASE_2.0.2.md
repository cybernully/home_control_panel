# Home Control Panel 2.0.2

## OTA upload repair

The reported 2.0.1 upload lost its HTTP connection partway through the transfer.
The screenshot alone cannot distinguish a panel reset, Wi-Fi failure, or browser
disconnect. No device serial log was available to establish the exact trigger.

The previous implementation parsed an entire multi-megabyte multipart upload in
one synchronous `WebServer::handleClient()` call. Its `Update.write()` callbacks
did not delay, so a continuously buffered TCP stream could prevent the normal
panel loop from running and leave inadequate scheduling opportunities during
flash writes. The pinned Arduino multipart parser only waits when data runs out.

2.0.2 changes that path:

- Web Admin sends one acknowledged 16 KB multipart chunk per HTTP request. Each
  request returns to the main loop; flash callbacks also call `delay(1)`.
- An authenticated begin request declares the exact image size. A random session
  and strict byte offsets prevent another request from overwriting the upload.
- Chunks never activate firmware. A separate finish request checks that every
  declared byte arrived and asks the Arduino updater to validate the image before
  switching the boot partition. The legacy endpoint also waits for the completed
  HTTP request before activation.
- Interrupted or abandoned sessions release the updater after two minutes.
  Invalid images, truncated chunks, write errors, and validation failures abort
  before activation. Configuration saves, restores, and manual reboot are blocked
  while an update is active.
- Progress reports bytes acknowledged by the panel. Background status polling
  pauses during maintenance. A lost acknowledgement is checked against the
  server's session state, and a lost activation reply is reconciled with verified
  state or a changed boot ID. Connection loss no longer claims the old firmware
  necessarily remains active.

Both hardware targets, schema 10, saved layouts, and the consistent panel headers
from 2.0.1 are retained. The application still writes the inactive 6 MB app
partition; this release does not install a filesystem image.

## Installing when the existing OTA page fails

The installed old firmware handles the first upload of 2.0.2. Selecting a newer
binary cannot fix that running handler before installation. The supplied recovery
uploader uses the old authenticated endpoint, streams 1 KB blocks at 64 KB/s, and
creates gaps that give the old parser time to yield. This is a recovery attempt,
not a proven on-device fix for the unknown connection failure.

From the workspace in PowerShell, replace `PANEL_IP` with the panel address and
choose the binary for the hardware actually installed:

```powershell
& .\.build-venv\Scripts\python.exe scripts/upload_firmware_recovery.py --host PANEL_IP --firmware releases/2.0.2/firmware-2.0.2-2635.bin
```

For 2624 hardware use `firmware-2.0.2-2624.bin` instead. The script prompts for
the existing Web Admin password; use `--username` if the username differs from
`admin`. It never stores credentials or automatically repeats an upload.
Afterward it checks for 2.0.2 and a new boot, rather than treating sent bytes as
proof of activation.

If the old OTA handler still cannot complete this paced transfer, install 2.0.2
once through the existing USB firmware upload procedure for the correct target.
Do not erase flash or upload SPIFFS: those operations can remove the saved panel
configuration. After 2.0.2 is installed, reload Web Admin so the new chunked
uploader is loaded. Confirm the displayed version and normal panel operation.

The recovery uploader does not infer hardware revision from a filename or panel
name; selecting the correct 2624/2635 image remains necessary.

See [validation evidence](VALIDATION_2.0.2.md) for host checks, build artifacts,
and the remaining device verification boundary.
