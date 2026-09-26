# Release 1.4.2 - Persistent Home Assistant command channel

Home Assistant state updates remained reliable while the panel's separate REST command connections degraded. Commands now use Home Assistant's authenticated persistent WebSocket with `call_service`, the same transport that provides the live entity state.

Only one command can be in flight. The panel waits for Home Assistant's WebSocket result before sending the next command and reports a timeout after six seconds if a result is missing. A socket disconnect clears the pending command and reports the reconnect condition instead of issuing a stale REST request.
