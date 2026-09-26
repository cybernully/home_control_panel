# Release 1.4.2 - Persistent Home Assistant command channel

Home Assistant state updates remained reliable while the panel's separate REST command connections degraded. Commands now use Home Assistant's authenticated persistent WebSocket with `call_service`, the same transport that provides the live entity state.

Only one command can be in flight. The panel waits for Home Assistant's WebSocket result before sending the next command and reports a timeout after six seconds if a result is missing. A socket disconnect clears the pending command and reports the reconnect condition instead of issuing a stale REST request.

## Overview and System screens

Overview is now a home-first startup screen with a prominent live-status card, compact area/light/network summary, large immediate actions, and a clear path to detailed room controls. The System screen is intentionally more technical: it shows internal RAM, PSRAM, SPIFFS storage, CPU clock plus measured panel-app loop duty cycle, uptime, Wi-Fi signal, panel identity, and Home Assistant diagnostics. The app-loop percentage is not presented as whole-chip CPU utilization because Wi-Fi and RTOS work runs outside Arduino's loop task.

## Media artwork reliability

Artwork is now matched to both the media-player entity and its exact artwork URL. The old cover is hidden as soon as a track supplies a new URL, preventing stale covers from being shown. If an artwork download fails after it is accepted by the worker, the panel retries that exact cover every eight seconds until the cache contains it.

The artwork reader now drains bytes already buffered by the HTTP client even after Home Assistant closes the response socket. It also permits covers up to 1 MB and uses a 12-second artwork timeout, improving reliability for media-proxy responses and larger current artwork.

On HTTPS panels, artwork retrieval now releases the idle WebSocket TLS session before starting the artwork TLS handshake. This avoids the ESP32-P4 internal DMA-memory exhaustion reported by `esp-aes` during a second concurrent TLS connection; Home Assistant reconnects and refreshes the area subscription immediately afterward.
