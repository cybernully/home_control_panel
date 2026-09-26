#pragma once
#include <string>
#include <stdint.h>
#include <cstdio>
#include <cstdarg>
using String = std::string;
inline uint32_t millis() { return 10000; }
template<typename T> inline T constrain(T value, T low, T high) { return value < low ? low : value > high ? high : value; }
struct HostSerial {
    void println(const char *text) { std::puts(text); }
    void printf(const char *format, ...) { va_list args; va_start(args, format); std::vprintf(format, args); va_end(args); }
};
inline HostSerial Serial0;
