# Home Control Panel v1.8.0

## Calendar experience

- Replaces the Calendar placeholder with a combined weekly agenda for up to six Home Assistant `calendar.*` entities.
- Opens on the current week and current day, with touch-friendly previous, today, and next navigation.
- Every day shows its event count. Selecting a day displays its agenda, including all-day and multi-day events.
- Selecting an event opens a detail overlay with its source calendar, time or date range, location, and description.
- The existing Overview Calendar card now summarizes the next event from the combined selection.

## Web Admin

- Discovers calendars by Home Assistant friendly name and entity ID.
- Supports adding, removing, and reordering up to six calendars.
- Each source has an editable display name and accent color.
- Week-start preference supports Monday or Sunday.
- Existing single-calendar configurations migrate automatically into the new collection.

## Reliability

- Calendar JSON and Home Assistant communication remain outside the LVGL module through a fixed UI state model.
- Events use a bounded 48-entry cache allocated in PSRAM when available.
- Requests use the authenticated Home Assistant WebSocket connection and one response-returning `calendar.get_events` action per selected source, keeping each response below the panel's bounded WebSocket frame budget.
- Five-minute refresh coalescing, request timeout, retry backoff, loading, empty, unavailable, and unsynchronized-clock states are handled explicitly.
- Event ranges use exclusive end times and local calendar-day boundaries, including daylight-saving transitions.

## Release validation boundary

- Native LVGL rendering, web-editor behavior, structure validation, and both ESP32-P4 firmware targets are release checks.
- Live calendar integration behavior, long descriptions, timezone presentation, and touch interaction still require verification on the configured panel and Home Assistant instance.
