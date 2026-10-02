# Home Control Panel v1.7.2

## Weather experience

- Replaces the Weather placeholder with a live current-conditions, hourly, and multi-day forecast screen.
- Uses Home Assistant's authenticated WebSocket `weather.get_forecasts` service and retains only eight hourly and five daily entries in a bounded cache.
- Coalesces forecast refreshes for 30 minutes while current conditions continue to update through the existing entity subscription.
- Keeps Home Assistant transport and JSON parsing separate from the UI through `HomeAssistantWeatherSnapshot` and `WeatherViewModel`.

## Web Admin

- Selects any discovered `weather.*` provider by friendly name or entity ID.
- Offers balanced, current-focus, and forecast-focus layouts.
- Independently shows or hides current conditions, hourly forecasts, and multi-day forecasts.
- Optionally places a compact current-temperature summary in the persistent panel header.
- Adds independent current, hourly, and multi-day Weather cards to the configurable Overview catalog.

## Compatibility and validation

- Configuration schema 4 preserves older Weather cards by migrating their editor type to Current Weather.
- Forecast arrays and all display strings are fixed-size; unavailable, loading, and unconfigured states render without dereferencing missing forecast data.
- Both ESP32-P4 targets are built as release checks. Live provider response shape, timezone presentation, and touch/layout behavior still require verification on the panel with its configured Home Assistant instance.
