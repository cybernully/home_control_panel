# Release 1.4.1 - Home Assistant command stability

## Fixed

- Controls are only queued after Wi-Fi is stable and the Home Assistant WebSocket is authenticated and has completed discovery. A tap during reconnect now says **Home Assistant reconnecting** instead of being accepted and then failing later.
- REST service calls have a 3.5-second connection timeout and a 5-second request timeout, which accommodates normal TLS and cross-VLAN latency without blocking indefinitely.
- Idempotent commands (lights, covers, scenes, brightness, mute, volume set, and source selection) retry once after a short delay for connection, timeout, rate-limit, or server errors. The result shows `(retry)` so recovery is visible.
- Non-idempotent media commands (play/pause, next, previous, volume step, and play-media) are deliberately never retried: a lost response must not cause an accidental second action.

## Verification

Run the project structure, host UI, and native Room tests, then build the target matching the panel rear label. After upload, verify a light, scene, media transport, and a Wi-Fi reconnect from the physical panel.
