#pragma once
#include <stddef.h>
#include <stdint.h>
#define UPDATE_SIZE_UNKNOWN ((size_t)-1)
#define U_FLASH 0
struct FakeUpdate {
    bool begin_ok = true, write_ok = true, end_ok = true;
    bool running = false, activated = false, allow_remaining = false;
    unsigned aborts = 0, ends = 0;
    size_t declared = 0, written = 0;
    bool begin(size_t size, int command) { declared = size; running = begin_ok && command == U_FLASH; return running; }
    size_t write(uint8_t *, size_t size) { if (!write_ok) return 0; written += size; return size; }
    bool end(bool remaining) { ++ends; allow_remaining = remaining; if (!end_ok) return false; activated = true; running = false; return true; }
    void abort() { ++aborts; running = false; }
    const char *errorString() { return "simulated flash failure"; }
    unsigned getError() { return 7; }
};
extern FakeUpdate Update;
