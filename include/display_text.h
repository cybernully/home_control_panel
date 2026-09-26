#pragma once
#include <stddef.h>
#include <stdint.h>

// Montserrat's bundled subset lacks dash glyphs. Preserve complete UTF-8
// sequences, normalize en/em dashes, and never leave a partial codepoint.
inline void panel_display_text(char *dest, size_t capacity, const char *text) {
    if (!capacity) return;
    const auto *p = reinterpret_cast<const uint8_t *>(text ? text : "");
    size_t n = 0;
    while (*p) {
        size_t width = *p < 0x80 ? 1 : (*p & 0xE0) == 0xC0 ? 2 :
                       (*p & 0xF0) == 0xE0 ? 3 : (*p & 0xF8) == 0xF0 ? 4 : 0;
        if (!width) { ++p; continue; }
        bool valid = true;
        for (size_t i = 1; i < width; ++i) {
            if (!p[i] || (p[i] & 0xC0) != 0x80) { valid = false; break; }
        }
        if (!valid) { ++p; continue; }
        if (width == 3 && p[0] == 0xE2 && p[1] == 0x80 && (p[2] == 0x93 || p[2] == 0x94)) {
            if (n + 1 >= capacity) break;
            dest[n++] = '-'; p += 3; continue;
        }
        if (n + width >= capacity) break;
        for (size_t i = 0; i < width; ++i) dest[n++] = static_cast<char>(*p++);
    }
    dest[n] = '\0';
}
