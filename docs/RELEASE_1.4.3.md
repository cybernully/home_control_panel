# Release 1.4.3 - HTTPS artwork memory fix

Serial logs showed the artwork failure was an ESP32-P4 internal DMA-memory shortage during a second TLS handshake: the persistent Home Assistant WebSocket already held TLS memory when the panel opened an HTTPS artwork request. Version 1.4.3 closes that idle WebSocket session before fetching HTTPS artwork, then reconnects and refreshes Home Assistant state immediately afterward.

Artwork download handling also drains buffered bytes after socket closure, permits up to 1 MB cover art, and uses a 12-second artwork timeout.
