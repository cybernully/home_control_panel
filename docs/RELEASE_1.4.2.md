# Release 1.4.2 - Persistent Home Assistant command channel

Home Assistant state updates remained reliable while the panel's separate REST command connections degraded. Commands now use Home Assistant's authenticated persistent WebSocket with `call_service`, the same transport that provides the live entity state.

Only one command can be in flight. The panel waits for Home Assistant's WebSocket result before sending the next command and reports a timeout after six seconds if a result is missing. A socket disconnect clears the pending command and reports the reconnect condition instead of issuing a stale REST request.

## Overview and System screens

Overview is now a home-first startup screen with a prominent live-status card, compact area/light/network summary, large immediate actions, and a clear path to detailed room controls. The System screen is intentionally more technical: it shows internal RAM, PSRAM, SPIFFS storage, CPU clock plus measured panel-app loop duty cycle, uptime, Wi-Fi signal, panel identity, and Home Assistant diagnostics. The app-loop percentage is not presented as whole-chip CPU utilization because Wi-Fi and RTOS work runs outside Arduino's loop task.
