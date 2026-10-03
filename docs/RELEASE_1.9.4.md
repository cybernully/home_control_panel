# Home Control Panel v1.9.4

## Unified tab presentation

- Gives Overview, Rooms, Media, Weather, Calendar, Climate, Security, and
  Settings the same page-title and descriptive-subtitle hierarchy.
- Adds the missing Rooms page header and shifts its compact room status,
  favorite controls, and quick-access cards down without reducing touch areas.
- Renames the System page heading to Settings so the page and navigation label
  agree.
- Aligns primary content to a shared 80-pixel baseline where the layout does
  not require a secondary selector or navigation row.

## Layout refinements

- Moves Calendar's week label and navigation below the page header, followed by
  a tighter day strip and full-width agenda that still fits the available panel
  height.
- Keeps Weather's configured provider on the subtitle row instead of crowding
  the page title.
- Preserves Media's top-right popup controls while bringing the now-playing and
  quick-play sections into the same vertical rhythm as the other tabs.
- Removes redundant live-state pills; connection health remains available in
  the universal panel header.

## Compatibility and verification boundary

Configuration schema 10 is unchanged, so existing room, overview, media,
weather, calendar, climate, Security, and Settings configuration is preserved.
Host render and interaction tests plus both firmware builds verify the source
and layout integration. Flashing, physical touch behavior, and live Home
Assistant state or command behavior still require validation on the panel.
