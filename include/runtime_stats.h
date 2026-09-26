#pragma once

#include <stdint.h>

// Measures the panel application's own loop duty cycle. It intentionally does
// not claim to be total ESP32 CPU utilization, because Wi-Fi and RTOS tasks
// run outside Arduino's loop task.
void runtime_stats_note_loop(uint32_t busy_us);
uint8_t runtime_stats_app_loop_percent();
