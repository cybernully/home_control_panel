# Home Control Panel v2.0.1

## Uniform headers on every panel screen

- Gives Overview, Rooms, Media, Climate, Security, Weather, Calendar, and
  Settings the same icon, title, and subtitle geometry and typography.
- Shares the page and navigation icon mapping so each screen has one identity.
- Uses Montserrat 14 for every persistent-header caption, with one centered
  baseline for weather, status, Home Assistant communication, Wi-Fi, battery,
  and Settings.
- Aligns glyph icons and the drawn battery and signal bars to a common icon
  row. Status colors and battery/signal indicators retain their meaning.
- Bounds page text with ellipsis and reserves space for Media popup buttons
  and Weather's provider label; the provider uses the shared subtitle font.

Configuration schema 10 is unchanged. Host rendering, interactions, regression
checks, and both firmware targets are checked before delivery. Physical touch,
on-device rendering, and live Home Assistant behavior remain device checks.
See [validation results](VALIDATION_2.0.1.md) for the completed checks and
firmware checksums.
