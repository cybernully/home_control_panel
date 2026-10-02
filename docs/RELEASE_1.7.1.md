# Release 1.7.1 - Persistent Web Admin header actions

Version 1.7.1 keeps the Web Admin header and navigation visible while the
configuration page scrolls.

- The panel identity, connection state, navigation tabs, Save, and Reboot
  controls now share one sticky, translucent application header.
- Save submits the same validated configuration form from every Web Admin tab.
- Save remains disabled until the current configuration has loaded successfully.
- Reboot is available globally in the header; the duplicate Administration
  button and bottom floating save card were removed.
- The responsive header keeps connection information and both actions usable on
  narrow screens without covering page content.

Web source checks cover the sticky header, globally bound actions, single save
submission path, and JavaScript syntax. Both ESP32-P4 variants remain release
build gates; browser behavior should still be confirmed on the deployed panel.
