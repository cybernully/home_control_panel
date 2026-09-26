# Release 1.4.4 - Seamless media artwork reconnect

HTTPS artwork retrieval must temporarily release the WebSocket TLS session on the ESP32-P4. Version 1.4.4 preserves the discovered media snapshot across that controlled reconnect and re-subscribes directly to the retained entity set afterward. The Media tab therefore keeps its player, title, and playback state visible while new artwork downloads.
