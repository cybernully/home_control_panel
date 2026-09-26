#include "runtime_stats.h"

#include <Arduino.h>

namespace {
uint32_t g_window_started_ms = 0;
uint64_t g_busy_us = 0;
uint8_t g_app_loop_percent = 0;
}

void runtime_stats_note_loop(uint32_t busy_us) {
    const uint32_t now = millis();
    if (!g_window_started_ms) g_window_started_ms = now;
    g_busy_us += busy_us;
    const uint32_t elapsed_ms = now - g_window_started_ms;
    if (elapsed_ms < 5000UL) return;
    const uint64_t elapsed_us = static_cast<uint64_t>(elapsed_ms) * 1000ULL;
    const uint64_t percent = elapsed_us ? (g_busy_us * 100ULL) / elapsed_us : 0;
    g_app_loop_percent = static_cast<uint8_t>(percent > 100 ? 100 : percent);
    g_busy_us = 0;
    g_window_started_ms = now;
}

uint8_t runtime_stats_app_loop_percent() { return g_app_loop_percent; }
