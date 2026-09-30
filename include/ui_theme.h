#pragma once

#include <lvgl.h>
#include <stdint.h>

namespace ui_theme {
constexpr uint32_t BG = 0x06182B;
constexpr uint32_t SURFACE = 0x0C223A;
constexpr uint32_t SURFACE_RAISED = 0x102A46;
constexpr uint32_t BORDER = 0x315678;
constexpr uint32_t BORDER_ACTIVE = 0x0EA5FF;
constexpr uint32_t TEXT = 0xF7FAFF;
constexpr uint32_t MUTED = 0xA9C3EE;
constexpr uint32_t ACCENT = 0x078BFF;
constexpr uint32_t ACCENT_DARK = 0x0758C9;
constexpr uint32_t CYAN = 0x24D8F2;
constexpr uint32_t YELLOW = 0xFFD94A;
constexpr uint32_t SUCCESS = 0x42ED83;
constexpr uint32_t WARN = 0xFFB547;
constexpr uint32_t DANGER = 0xFF667A;

void surface(lv_obj_t *obj, uint32_t color = SURFACE, int radius = 16, int border = 1);
void interactive(lv_obj_t *obj, bool active, bool enabled = true);
void text(lv_obj_t *obj, uint32_t color = TEXT);
void slider(lv_obj_t *obj);
void set_glyph(lv_obj_t *label, uint32_t codepoint);
}
