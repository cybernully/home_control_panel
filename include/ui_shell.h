#pragma once
#include <stddef.h>
#include <stdint.h>

void ui_shell_begin();
void ui_shell_loop();
void ui_shell_refresh_header();
// A short-lived local action message is surfaced by the persistent header,
// so command feedback does not disappear when the user changes tabs.
void ui_shell_report_status(const char *message);
bool ui_shell_select_tab(const char *id);
const char *ui_shell_active_tab_id();
const char *ui_shell_active_tab_title();
void ui_shell_set_remote_message(const char *message, uint32_t duration_ms);
void ui_shell_dismiss_remote_message();
bool ui_shell_get_remote_message(char *message, size_t capacity, uint32_t *remaining_ms);
