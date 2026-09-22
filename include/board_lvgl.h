#pragma once
#include <Arduino.h>
#include <lvgl.h>
bool board_lvgl_begin(uint16_t rotation);
void board_lvgl_loop();
void board_set_backlight(uint8_t percent);
int16_t board_width();
int16_t board_height();
void board_set_display_awake(bool awake, uint8_t restore_percent);
bool board_display_awake();
bool board_take_touch_activity();
bool board_get_touch_state(int16_t &x, int16_t &y, bool &pressed);
