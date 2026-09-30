#pragma once
void ui_shell_begin();
void ui_shell_loop();
void ui_shell_refresh_header();
// A short-lived local action message is surfaced by the persistent header,
// so command feedback does not disappear when the user changes tabs.
void ui_shell_report_status(const char *message);
