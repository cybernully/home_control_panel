# Release 1.5.2 - Media Browse Favorites

Media now keeps two distinct action collections:

- Three one-touch shortcuts on the main Media screen.
- Up to six saved **Browse favorites** in `Media > Browse`.

Browse favorites are configured from the Media section of the local web
manager. Each entry has a label, target player, media content ID, and content
type. After a Home Assistant scan, the player field offers discovered players
as suggestions.

Saved Browse favorites appear ahead of integration-provided browse items. Their
target media players are included in the explicit Home Assistant subscription,
so favorites can safely target a player other than the one currently selected.
